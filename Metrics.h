#pragma once
#include "crow.h"
#include <atomic>
#include <chrono>
#include <string>

class Metrics
{
  public:
    static inline std::atomic<uint64_t> totalRequests{0};
    static inline std::atomic<uint64_t> successRequests{0}; // 2xx
    static inline std::atomic<uint64_t> errorRequests{0};   // 4xx, 5xx
    static inline std::atomic<uint64_t> cacheHits{0};
    static inline std::atomic<uint64_t> cacheMisses{0};

    static void incrementRequests() { totalRequests++; }
    static void recordSuccess() { successRequests++; }
    static void recordError() { errorRequests++; }
    static void recordCacheHit() { cacheHits++; }
    static void recordCacheMiss() { cacheMisses++; }

    static crow::json::wvalue toJson()
    {
        crow::json::wvalue j;
        j["total_requests"] = totalRequests.load();
        j["success_count"] = successRequests.load();
        j["error_count"] = errorRequests.load();
        j["cache_hits"] = cacheHits.load();
        j["cache_misses"] = cacheMisses.load();

        // Расчет Hit Rate
        double hitRate = 0;
        uint64_t totalCache = cacheHits + cacheMisses;
        if (totalCache > 0)
            hitRate = (double)cacheHits / totalCache * 100.0;
        j["cache_hit_rate_percent"] = hitRate;

        return j;
    }
};