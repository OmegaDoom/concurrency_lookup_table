#include "concurrent_lookup_table.h"

#include <thread>
#include <gtest/gtest.h>

TEST(LookupTable, WriteReadValue)
{
    omega::concurrent_lookup_table<int, int> table(64, 256);
    table.add_or_update(0, 5);
    EXPECT_EQ(table.get_value(0).value(), 5);
}

TEST(LookupTable, AddRemoveValue)
{
    omega::concurrent_lookup_table<int, int> table(64, 256);
    table.add_or_update(0, 0);
    table.remove(0);
    EXPECT_FALSE(table.get_value(0).has_value());
}

TEST(LookupTable, WriteReadValues)
{
    omega::concurrent_lookup_table<int, int> table(64, 256);

    for(int i = 0; i < 10000; ++i)
    {
        table.add_or_update(i, i);
    }

    for(int i = 0; i < 10000; ++i)
    {
        EXPECT_EQ(table.get_value(i).value(), i);
    }
}

TEST(LookupTable, ParrallelWriteReadValues)
{
    omega::concurrent_lookup_table<int, std::string> table(64, 256);

    constexpr int iterations = 100000;
    std::thread thread1([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i;
            std::optional<std::string> value;
            while(!value.has_value())
            {
                value = table.get_value(idx);
            }
            EXPECT_EQ(value.value(), "AAAAAAA = " + std::to_string(idx));
        }
    });

    std::thread thread2([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i + iterations;
            std::optional<std::string> value;
            while(!value.has_value())
            {
                value = table.get_value(idx);
            }
            EXPECT_EQ(value.value(), "BBBBBBB = " + std::to_string(idx));
        }
    });

    std::thread thread3([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i;
            table.add_or_update(idx, "AAAAAAA = " + std::to_string(idx));
        }
    });

    std::thread thread4([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i + iterations;
            table.add_or_update(idx, "BBBBBBB = " + std::to_string(idx));
        }
    });

    std::thread thread5([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i + 2 * iterations;
            table.add_or_update(idx, "CCCCCCC = " + std::to_string(idx));
        }
    });

    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    thread5.join();
}

TEST(LookupTable, ParrallelWriteReadValuesWithFixedConcurrency)
{
    omega::concurrent_lookup_table<int, std::string> table(256, 256, false);

    constexpr int iterations = 100000;
    std::thread thread1([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i;
            std::optional<std::string> value;
            while(!value.has_value())
            {
                value = table.get_value(idx);
            }
            EXPECT_EQ(value.value(), "AAAAAAA = " + std::to_string(idx));
        }
    });

    std::thread thread2([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i + iterations;
            std::optional<std::string> value;
            while(!value.has_value())
            {
                value = table.get_value(idx);
            }
            EXPECT_EQ(value.value(), "BBBBBBB = " + std::to_string(idx));
        }
    });

    std::thread thread3([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i;
            table.add_or_update(idx, "AAAAAAA = " + std::to_string(idx));
        }
    });

    std::thread thread4([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i + iterations;
            table.add_or_update(idx, "BBBBBBB = " + std::to_string(idx));
        }
    });

    std::thread thread5([&table] ()
    {
        for(int i = 0; i < iterations; ++i)
        {
            int idx = i + 2 * iterations;
            table.add_or_update(idx, "CCCCCCC = " + std::to_string(idx));
        }
    });

    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    thread5.join();
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
