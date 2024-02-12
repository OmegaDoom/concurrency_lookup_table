#pragma once

#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
#include <list>
#include <utility>
#include <algorithm>
#include <cmath>

namespace omega
{
class multiple_lock
{
    std::vector<std::unique_lock<std::mutex>> m_locks;
public:
    multiple_lock(std::vector<std::mutex>& locks)
    {
        m_locks.resize(locks.size());
	    for (int i = 0; i < locks.size(); ++i)
	    {
            m_locks[i] = std::unique_lock<std::mutex>(locks[i], std::defer_lock_t{});
        }

        int idx = 0;
        do
        {
            m_locks[idx].lock();
            for (int i = 0; i < locks.size() - 1; ++i)
            {
                std::size_t n = (i + idx + 1) % locks.size();
                if (!m_locks[n].try_lock())
                {
                    for (int j = 0; j < i + 1; ++j)
                    {
                        m_locks[n - j - 1].unlock();
                    }
                    break; 
                }
            }
        } while (!m_locks[idx].owns_lock());
    }

    ~multiple_lock()
    {
	    for (int i = 0; i < m_locks.size(); ++i)
	    {
            m_locks[i].unlock();
        }
    }
};

template<typename Key, typename Value, std::size_t MaxLoadFactor = 4, typename Hash=std::hash<Key>>
class concurrent_lookup_table
{
private:
    class bucket_type
    {
        using bucket_value = std::pair<Key,Value>;
        using bucket_data = std::list<bucket_value>;
        using bucket_iterator = typename bucket_data::iterator;
        using const_bucket_iterator = typename bucket_data::const_iterator;
        bucket_data m_data;
        std::size_t m_size;

    public:
        bucket_iterator find_entry(Key const& key)
        {
            return std::find_if(m_data.begin(), m_data.end(),
                                [&key](bucket_value const& item)
                                { return item.first == key; });
        }

        const bucket_data& get_data() const
        {
            return m_data;
        }

        const_bucket_iterator find_entry(Key const& key) const
        {
            return std::find_if(m_data.cbegin(), m_data.cend(),
                                [&key](bucket_value const& item)
                                { return item.first == key; });
        }

        std::optional<Value> get_value(Key const& key) const
        {
            const_bucket_iterator const found_entry = find_entry(key);
            return (found_entry != m_data.cend()) ? std::make_optional(found_entry->second) : std::optional<Value>{};
        }

        void remove(Key const& key)
        {
            bucket_iterator const found_entry = find_entry(key);
            if(found_entry != m_data.end())
            {
                m_data.erase(found_entry);
                --m_size;
            }
        }

        std::size_t add_or_update(Key const& key, Value const& value)
        {
            bucket_iterator const found_entry = find_entry(key);
            if (found_entry == m_data.end())
            {
                m_data.push_back(bucket_value(key, value));
                ++m_size;
            }
            else
            {
                found_entry->second = value;
            }

            return m_size;
        }
    };

    class table_type
    {
    public:
        std::vector<bucket_type> m_buckets;
    
    private:
        std::vector<std::mutex> m_locks;
        std::size_t m_budget;
        Hash hasher;

        std::size_t get_mutex_index(Key const& key) const
        {
            return get_bucket_index(key) / m_budget;
        }

        std::size_t get_bucket_index(Key const& key) const
        {
            return hasher(key) % m_buckets.size();
        }

        const bucket_type& get_bucket(Key const& key) const
        {
            return m_buckets[get_bucket_index(key)];
        }

        bucket_type& get_bucket(Key const& key)
        {
            return m_buckets[get_bucket_index(key)];
        }

    public:
        struct table_size
        {
            std::size_t buckets_size;
            std::size_t current_bucket_size;
        };

        table_type(std::size_t concurrency, std::size_t buckets_count)
            : m_buckets{buckets_count}
            , m_locks{concurrency}
            , m_budget{std::size_t(std::ceil(float(buckets_count) / concurrency))}
        {}

        std::optional<Value> get_value(Key const& key) const
        {
            return get_bucket(key).get_value(key);
        }

        void remove(Key const& key)
        {
            return get_bucket(key).remove(key);
        }

        table_size add_or_update(Key const& key, Value const& value)
        {
            return table_size{m_buckets.size(), get_bucket(key).add_or_update(key, value)};
        }

        std::lock_guard<std::mutex> lock(Key const& key)
        {
            auto bucket_index = get_mutex_index(key);
            return std::lock_guard<std::mutex> {m_locks[bucket_index]};
        }

        multiple_lock lock_all()
        {
            return multiple_lock{m_locks};
        }

        std::size_t get_buckets_size() const
        {
            return m_buckets.size();
        }

        std::size_t get_locks_size() const
        {
            return m_locks.size();
        }
    };

    void resize(std::size_t new_size)
    {
        for(;;)
        {
            auto table = std::atomic_load_explicit(&m_table, std::memory_order_acquire);
            auto const lock = table->lock_all();
            auto after_lock_table = std::atomic_load_explicit(&m_table, std::memory_order_acquire);
            if (table != after_lock_table)
                continue;

            std::size_t new_concurrency = m_grow_mutexes_on_resize ?
                std::min(2 * table->get_locks_size(), MAX_LOCK_NUMBER) :
                table->get_locks_size();
            std::size_t new_capacity = 2 * table->get_buckets_size() + 1;
            auto new_table = std::make_shared<table_type>(new_concurrency, new_capacity);

            for (int i = 0; i < table->get_buckets_size(); ++i)
            {
                for(const auto& val : table->m_buckets[i].get_data())
                {
                    new_table->add_or_update(val.first, val.second);
                }
            }

            std::atomic_store_explicit(&m_table, new_table, std::memory_order_release);
            std::atomic_flag_clear_explicit(&m_resize_in_process, std::memory_order_relaxed); 
            break;
        }
    }

public:
    std::shared_ptr<table_type> m_table;
    bool m_grow_mutexes_on_resize;
    std::atomic_flag m_resize_in_process = false;
    constexpr static std::size_t MAX_LOCK_NUMBER = 1024;
    concurrent_lookup_table(std::size_t concurrency, std::size_t capacity, bool grow_concurrency_on_resize = true)
        : m_table{std::make_shared<table_type>(concurrency, std::max(capacity, concurrency))}
        , m_grow_mutexes_on_resize{grow_concurrency_on_resize}
    {
    }

    concurrent_lookup_table(concurrent_lookup_table const& other)=delete;
    concurrent_lookup_table& operator=(concurrent_lookup_table const& other)=delete;

    std::optional<Value> get_value(Key const& key) const
    {
        auto table = std::atomic_load_explicit(&m_table, std::memory_order_acquire);
        return table->get_value(key);
    }

    void add_or_update(Key const& key, Value const& value)
    {
        typename table_type::table_size size;
        bool should_resize = false;
        for(;;)
        {
            auto table = std::atomic_load_explicit(&m_table, std::memory_order_acquire);
            auto const lock = table->lock(key);
            auto after_lock_table = std::atomic_load_explicit(&m_table, std::memory_order_acquire);
            if (table != after_lock_table)
                continue;
            
            size = table->add_or_update(key, value);
            if (size.current_bucket_size > MaxLoadFactor &&
                std::atomic_flag_test_and_set_explicit(&m_resize_in_process, std::memory_order_relaxed))
            {
                should_resize = true;
            }
            break;
        }

        if (should_resize)
        {
            resize(2 * size.buckets_size + 1);
        }
    }

    void remove(Key const& key)
    {
        for(;;)
        {
            auto table = std::atomic_load_explicit(&m_table, std::memory_order_acquire);
            auto const lock = table->lock(key);
            auto after_lock_table = std::atomic_load_explicit(&m_table, std::memory_order_acquire);
            if (table != after_lock_table)
                continue;
            
            table->remove(key);
            break;
        }
    }
};
}