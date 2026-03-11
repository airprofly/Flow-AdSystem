// Author: airprofly
#pragma once
#include "budget_config.hpp"
#include "budget_state.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <array>
#include <memory>
#include <chrono>

namespace flow_ad {
namespace pacing {

// 分片数量(设置为 CPU 核心数的 2-4 倍)
constexpr uint32_t kNumPacingShards = 64;

// 单个分片
struct PacingShard {
    mutable std::shared_mutex mutex;

    // campaign_id -> (config, state)
    std::unordered_map<uint64_t, std::pair<BudgetConfig, BudgetState>> campaigns;

    // 统计信息
    std::atomic<uint64_t> check_count{0};
    std::atomic<uint64_t> allow_count{0};
    std::atomic<uint64_t> deny_count{0};
};

// Pacing 管理器
class PacingManager {
public:
    PacingManager();
    ~PacingManager() = default;

    // 禁止拷贝和移动
    PacingManager(const PacingManager&) = delete;
    PacingManager& operator=(const PacingManager&) = delete;

    /**
     * 检查是否允许投放广告
     * @param campaign_id 广告计划 ID
     * @param bid_price 出价(元)
     * @return true = 允许投放; false = 拒绝投放
     */
    bool allow_impression(uint64_t campaign_id, double bid_price);

    /**
     * 记录一次广告展示(竞价成功后调用)
     * @param campaign_id 广告计划 ID
     * @param win_price 竞价获胜价格(元)
     */
    void record_impression(uint64_t campaign_id, double win_price);

    /**
     * 添加/更新广告计划配置
     */
    void update_campaign_config(uint64_t campaign_id, const BudgetConfig& config);

    /**
     * 移除广告计划
     */
    void remove_campaign(uint64_t campaign_id);

    /**
     * 获取统计信息
     */
    struct Stats {
        uint64_t total_campaigns;
        uint64_t total_checks;
        uint64_t total_allows;
        uint64_t total_denies;
        double allow_rate;
        uint64_t total_impressions;
        double total_spent;
    };
    Stats get_stats() const;

    /**
     * 清理过期数据(后台线程调用)
     */
    void cleanup_expired();

private:
    // 分片数组
    std::array<PacingShard, kNumPacingShards> shards_;

    // 当前时间戳(毫秒)
    uint64_t current_time_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // 计算分片索引
    uint32_t get_shard_index(uint64_t campaign_id) const {
        return campaign_id % kNumPacingShards;
    }

    // 获取分片
    PacingShard& get_shard(uint64_t campaign_id) {
        return shards_[get_shard_index(campaign_id)];
    }

    // ========== Pacing 算法实现 ==========

    // 令牌桶算法
    bool token_bucket_check(BudgetConfig& config, BudgetState& state, double bid_price);
    void token_bucket_refill(BudgetConfig& config, BudgetState& state);

    // 自适应 Pacing
    bool adaptive_pacing_check(BudgetConfig& config, BudgetState& state, double bid_price);
    void adaptive_pacing_update(BudgetConfig& config, BudgetState& state);

    // PID 控制
    bool pid_control_check(BudgetConfig& config, BudgetState& state, double bid_price);
    void pid_control_update(BudgetConfig& config, BudgetState& state);
};

} // namespace pacing
} // namespace flow_ad
