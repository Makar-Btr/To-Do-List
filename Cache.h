#pragma once
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "Logger.h"
#include "Metrics.h"

class MemoryCache
{
  public:
    std::optional<std::string> get(const std::string &key)
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (cacheMap.count(key))
        {
            Logger::info("Cache HIT: " + key);
            Metrics::recordCacheHit();
            return cacheMap[key];
        }
        Logger::info("Cache MISS: " + key);
        Metrics::recordCacheMiss();
        return std::nullopt;
    }

    void set(const std::string &key, const std::string &value)
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cacheMap[key] = value;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cacheMap.clear();
        Logger::info("Cache cleared due to data update");
    }

  private:
    std::unordered_map<std::string, std::string> cacheMap;
    std::mutex cacheMutex;
};