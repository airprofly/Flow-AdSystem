// Author: airprofly
#include "frequency_manager.hpp"
#include <algorithm>
#include <mutex>

namespace flow_ad {
namespace frequency {

FrequencyManager::FrequencyManager() {
    // 预留空间,减少 rehash
    for (auto& shard : shards_) {
        shard.records.reserve(10000);
    }
}

bool FrequencyManager::is_capped(uint64_t user_id, uint64_t ad_id,
                                const FrequencyCap& cap) {
    if (!cap.is_enabled()) {
        return false; // 无频控限制
    }

    uint64_t now = current_time_ms();

    // 检查小时限制
    if (cap.hourly_limit > 0) {
        uint64_t hour_key = make_key(user_id, ad_id, TimeWindow::HOUR);
        auto& shard = get_shard(hour_key);

        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.records.find(hour_key);

            if (it != shard.records.end()) {
                // 检查是否过期
                if (it->second.expire_at_ms >= now) {
                    shard.hit_count.fetch_add(1, std::memory_order_relaxed);
                    if (it->second.count >= cap.hourly_limit) {
                        return true; // 已达小时上限
                    }
                } else {
                    // 过期了,需要删除 (但不能在读锁中删除)
                }
            }
        }

        // 过期数据清理 (升级到写锁)
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.records.find(hour_key);
            if (it != shard.records.end() && it->second.expire_at_ms < now) {
                shard.records.erase(it);
                shard.eviction_count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        shard.miss_count.fetch_add(1, std::memory_order_relaxed);
    }

    // 检查天限制
    if (cap.daily_limit > 0) {
        uint64_t day_key = make_key(user_id, ad_id, TimeWindow::DAY);
        auto& shard = get_shard(day_key);

        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.records.find(day_key);

            if (it != shard.records.end() && it->second.expire_at_ms >= now) {
                shard.hit_count.fetch_add(1, std::memory_order_relaxed);
                if (it->second.count >= cap.daily_limit) {
                    return true; // 已达天上限
                }
            }
        }

        // 过期清理
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.records.find(day_key);
            if (it != shard.records.end() && it->second.expire_at_ms < now) {
                shard.records.erase(it);
                shard.eviction_count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        shard.miss_count.fetch_add(1, std::memory_order_relaxed);
    }

    // 检查周限制
    if (cap.weekly_limit > 0) {
        uint64_t week_key = make_key(user_id, ad_id, TimeWindow::WEEK);
        auto& shard = get_shard(week_key);

        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.records.find(week_key);

            if (it != shard.records.end() && it->second.expire_at_ms >= now) {
                shard.hit_count.fetch_add(1, std::memory_order_relaxed);
                if (it->second.count >= cap.weekly_limit) {
                    return true; // 已达周上限
                }
            }
        }

        // 过期清理
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.records.find(week_key);
            if (it != shard.records.end() && it->second.expire_at_ms < now) {
                shard.records.erase(it);
                shard.eviction_count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        shard.miss_count.fetch_add(1, std::memory_order_relaxed);
    }

    return false; // 未达上限
}

uint32_t FrequencyManager::record_impression(uint64_t user_id, uint64_t ad_id,
                                           TimeWindow window) {
    uint64_t key = make_key(user_id, ad_id, window);
    auto& shard = get_shard(key);

    uint64_t now = current_time_ms();
    uint64_t window_size_ms = static_cast<uint64_t>(window);
    uint64_t expire_at = now + window_size_ms;

    std::unique_lock<std::shared_mutex> lock(shard.mutex);

    auto it = shard.records.find(key);
    if (it == shard.records.end()) {
        // 创建新记录
        auto result = shard.records.insert({
            key,
            FreqRecord(1, expire_at)
        });
        return result.first->second.count;
    } else {
        // 检查是否过期
        if (it->second.expire_at_ms < now) {
            // 重置计数
            it->second.count = 1;
            it->second.expire_at_ms = expire_at;
        } else {
            // 增加计数
            it->second.count++;
        }
        return it->second.count;
    }
}

uint32_t FrequencyManager::get_count(uint64_t user_id, uint64_t ad_id,
                                   TimeWindow window) {
    uint64_t key = make_key(user_id, ad_id, window);
    auto& shard = get_shard(key);

    uint64_t now = current_time_ms();

    std::shared_lock<std::shared_mutex> lock(shard.mutex);
    auto it = shard.records.find(key);

    if (it != shard.records.end() && it->second.expire_at_ms >= now) {
        shard.hit_count.fetch_add(1, std::memory_order_relaxed);
        return it->second.count;
    }

    shard.miss_count.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

void FrequencyManager::cleanup_expired() {
    uint64_t now = current_time_ms();

    for (auto& shard : shards_) {
        std::unique_lock<std::shared_mutex> lock(shard.mutex);

        auto it = shard.records.begin();
        while (it != shard.records.end()) {
            if (it->second.expire_at_ms < now) {
                it = shard.records.erase(it);
                shard.eviction_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                ++it;
            }
        }
    }
}

FrequencyManager::Stats FrequencyManager::get_stats() const {
    Stats stats{};
    stats.total_records = 0;
    stats.total_hits = 0;
    stats.total_misses = 0;
    stats.total_evictions = 0;

    for (const auto& shard : shards_) {
        stats.total_records += shard.records.size();
        stats.total_hits += shard.hit_count.load(std::memory_order_relaxed);
        stats.total_misses += shard.miss_count.load(std::memory_order_relaxed);
        stats.total_evictions += shard.eviction_count.load(std::memory_order_relaxed);
    }

    stats.hit_rate = stats.total_hits + stats.total_misses > 0
        ? static_cast<double>(stats.total_hits) / (stats.total_hits + stats.total_misses)
        : 0.0;

    // 估算内存占用
    stats.memory_usage_bytes = stats.total_records *
        (sizeof(uint64_t) + sizeof(FreqRecord) + sizeof(void*)); // 粗略估算

    return stats;
}

} // namespace frequency
} // namespace flow_ad
