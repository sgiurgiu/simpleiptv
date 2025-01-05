#pragma once

#include <concepts>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_map>

template <typename T>
concept HasSize = requires(T a) {
    { a.size() } -> std::convertible_to<int64_t>;
};

template <typename Key,
          HasSize Value,
          typename Hash = std::hash<Key>,
          typename Pred = std::equal_to<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>>
class ResourceCache
{
private:
    struct LoadedResource
    {
        LoadedResource(const Key& key, int64_t resourceMemory)
        : key(key)
        , dateLoaded(std::chrono::steady_clock::now())
        , resourceMemory(resourceMemory)
        {
        }
        Key key;
        std::chrono::steady_clock::time_point dateLoaded;
        int64_t resourceMemory = 0;
    };
    struct loadedResourceGreater
    {
        bool operator()(const LoadedResource& i1, const LoadedResource& i2) const
        {
            return i1.dateLoaded > i2.dateLoaded;
        }
    };

public:
    ResourceCache(int64_t totalMemoryAllowed = 150'000'000ll)
    : totalMemoryAllowed{ totalMemoryAllowed }
    {
    }
    void Put(Key key, Value value)
    {
        std::lock_guard<std::mutex> _{ m };
        data[key] = value;
        int requiredMemory = value.size();
        totalMemoryConsumed += requiredMemory;
        oldestResources.emplace(key, requiredMemory);
        while (totalMemoryConsumed > totalMemoryAllowed)
        {
            totalMemoryConsumed -= oldestResources.top().resourceMemory;
            data.erase(oldestResources.top().key);
            oldestResources.pop();
        }
    }
    std::optional<Value> Get(Key key)
    {
        std::lock_guard<std::mutex> _{ m };
        auto it = data.find(key);
        if (it != data.end())
        {
            return std::make_optional<Value>(it->second);
        }
        return {};
    }

private:
    std::mutex m;
    const int64_t totalMemoryAllowed;
    int64_t totalMemoryConsumed;
    std::unordered_map<Key, Value, Hash, Pred, Alloc> data;
    std::priority_queue<LoadedResource, std::vector<LoadedResource>, loadedResourceGreater>
        oldestResources;
};