#include <gtest/gtest.h>

#include "resource_cache.h"

TEST(ResourceCacheTest, KeepsNewestValueWhenUpdatingKey)
{
    ResourceCache<std::string, std::string> cache{ 10 };
    cache.Put("k", "abcd");
    cache.Put("k", "xy");

    auto value = cache.Get("k");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "xy");
}

TEST(ResourceCacheTest, EvictsOldestEntryWhenOverCapacity)
{
    ResourceCache<std::string, std::string> cache{ 6 };
    cache.Put("a", "abc");
    cache.Put("b", "de");
    cache.Put("c", "fgh");

    EXPECT_FALSE(cache.Get("a").has_value());
    auto b = cache.Get("b");
    auto c = cache.Get("c");
    ASSERT_TRUE(b.has_value());
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(b.value(), "de");
    EXPECT_EQ(c.value(), "fgh");
}

TEST(ResourceCacheTest, OverwriteDoesNotForceWrongEviction)
{
    ResourceCache<std::string, std::string> cache{ 5 };
    cache.Put("a", "abc");
    cache.Put("a", "d");
    cache.Put("b", "efgh");

    auto a = cache.Get("a");
    auto b = cache.Get("b");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(a.value(), "d");
    EXPECT_EQ(b.value(), "efgh");
}
