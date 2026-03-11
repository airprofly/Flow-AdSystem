// Author: airprofly
#pragma once
#include "frequency_record.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <array>
#include <memory>
#include <atomic>

namespace flow_ad {
namespace frequency {

// 分片数量 (建议设置为 CPU 核心数的 2-4 倍)
constexpr uint32_t kNumShards = 64;

// 单个分片
struct FreqShard {
    // 使用读写锁,提高并发读性能
    mutable std::shared_mutex mutex;

    // 存储: key = combine(user_id, ad_id, window)
    std::unordered_map<uint64_t, FreqRecord> records;

    // 统计信息
    std::atomic<uint64_t> hit_count{0};
    std::atomic<uint64_t> miss_count{0};
    std::atomic<uint64_t> eviction_count{0};
};

// 频控管理器 (核心类)
class FrequencyManager {
public:
    FrequencyManager();
    ~FrequencyManager() = default;

    // 禁止拷贝和移动
    FrequencyManager(const FrequencyManager&) = delete;
    FrequencyManager& operator=(const FrequencyManager&) = delete;

    /**
     * 检查是否达到频控上限
     * @param user_id 用户 ID
     * @param ad_id 广告 ID
     * @param cap 频控规则
     * @return true = 已达上限,应该拦截; false = 未达上限,可以展示
     */
    bool is_capped(uint64_t user_id, uint64_t ad_id, const FrequencyCap& cap);

    /**
     * 记录一次广告展示 (原子操作)
     * @param user_id 用户 ID
     * @param ad_id 广告 ID
     * @param window 时间窗口
     * @return 当前展示次数
     */
    uint32_t record_impression(uint64_t user_id, uint64_t ad_id, TimeWindow window);

    /**
     * 获取当前展示次数
     */
    uint32_t get_count(uint64_t user_id, uint64_t ad_id, TimeWindow window);

    /**
     * 清理过期数据 (后台线程调用)
     */
    void cleanup_expired();

    /**
     * 获取统计信息
     */
    struct Stats {
        uint64_t total_records;
        uint64_t total_hits;
        uint64_t total_misses;
        double hit_rate;
        uint64_t total_evictions;
        size_t memory_usage_bytes;
    };
    Stats get_stats() const;

private:
    // 分片数组
    std::array<FreqShard, kNumShards> shards_;

    // 当前时间戳 (毫秒)
    uint64_t current_time_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // 组合 Key: user_id + ad_id + window
    uint64_t make_key(uint64_t user_id, uint64_t ad_id, TimeWindow window) const {
        // 使用质数乘法,减少冲突
        constexpr uint64_t kPrime1 = 91138233;
        constexpr uint64_t kPrime2 = 972663749;
        return user_id * kPrime1 + ad_id * kPrime2 + static_cast<uint64_t>(window);
    }

    // 计算分片索引
    uint32_t get_shard_index(uint64_t key) const {
        return key % kNumShards;
    }

    // 获取分片
    FreqShard& get_shard(uint64_t key) {
        return shards_[get_shard_index(key)];
    }

    const FreqShard& get_shard(uint64_t key) const {
        return shards_[get_shard_index(key)];
    }
};

} // namespace frequency
} // namespace flow_ad
