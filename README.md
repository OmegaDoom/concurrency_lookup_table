# Concurrent lookup table 

This is a thread safe hash table. It is designed to achieve fine-grained concurrency

## Interface
1. concurrent_lookup_table(std::size_t concurrency, std::size_t capacity, bool grow_concurrency_on_resize = true)
 concurrency - initial number of mutexes to achieve fine-grained concurrency(it grows together with buckets if allowed) 
 capacity - initial number of buckets
 grow_concurrency_on_resize - if number of mutexes grows together with buckets or not

2. std::optional<Value> get_value(Key const& key) const - gets a value by key.

3. void add_or_update(Key const& key, Value const& value) - add a new value by key or updates existing one

4. void remove(Key const& key) - removes value by key

## Requirements
1. C++17 compiler

## Usage example
```cpp
#include "concurrent_lookup_table.h"
omega::concurrent_lookup_table<int, int> table{64, 256};
table.add_or_update(0, 5);
int value = table.get_value(0);
table.remove(0);
```

