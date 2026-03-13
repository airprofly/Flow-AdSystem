#pragma once

#include "ad_candidate.hpp"
#include "ecpm_calculator.hpp"
#include "sorting_strategy.hpp"
#include "topk_selector.hpp"
#include <cstdint>
#include <memory>

namespace flow_ad_system::ranker {

/**
 * @brief Ranker 配置
 */
struct RankerConfig {
    SortStrategy sort_strategy = SortStrategy::kGreedy;  ///< 排序策略
    size_t max_ads = 10;                                  ///< 最大返回广告数
    double min_ecpm = 0.0;                                ///< 最小 eCPM 阈值
    bool enable_category_dedup = false;                   ///< 是否启用类别去重
    bool enable_deal_dedup = false;                       ///< 是否启用 Deal 去重
    bool enable_parallel = true;                          ///< 是否启用并行计算
    double diversity_weight = 0.3;                        ///< 多样性权重(用于 DiverseSort)
    double budget_weight = 0.2;                           ///< 预算权重(用于 BudgetSort)

    // 性能相关
    size_t parallel_threshold = 1000;                     ///< 启用并行计算的阈值

    // 统计相关
    bool enable_stats = false;                            ///< 是否启用统计
};

/**
 * @brief Ranker 统计信息
 */
struct RankerStats {
    uint64_t total_requests = 0;          ///< 总请求数
    uint64_t total_candidates = 0;        ///< 总候选数
    uint64_t filtered_invalid = 0;        ///< 过滤的无效候选数
    uint64_t filtered_by_ecpm = 0;        ///< 被 eCPM 阈值过滤的候选数
    double avg_processing_time_us = 0.0;  ///< 平均处理时间(微秒)
    double p99_processing_time_us = 0.0;  ///< P99 处理时间(微秒)
};

/**
 * @brief 广告排序器
 *
 * 核心功能:
 * 1. 计算 eCPM
 * 2. 过滤无效候选
 * 3. 排序
 * 4. Top-K 选择
 * 5. 去重
 */
class Ranker {
public:
    explicit Ranker(const RankerConfig& config = RankerConfig{});
    ~Ranker() = default;

    /**
     * @brief 执行排序
     * @param candidates 广告候选列表(输入输出参数)
     * @return 排序后的候选数量
     */
    size_t rank(AdCandidateList& candidates);

    /**
     * @brief 更新配置
     */
    void update_config(const RankerConfig& config) {
        config_ = config;
    }

    /**
     * @brief 获取配置
     */
    const RankerConfig& get_config() const {
        return config_;
    }

    /**
     * @brief 获取统计信息
     */
    const RankerStats& get_stats() const {
        return stats_;
    }

    /**
     * @brief 重置统计信息
     */
    void reset_stats() {
        stats_ = RankerStats{};
    }

    /**
     * @brief 创建默认配置的 Ranker
     */
    static std::unique_ptr<Ranker> create_default();

    /**
     * @brief 创建高性能配置的 Ranker
     */
    static std::unique_ptr<Ranker> create_high_performance();

private:
    RankerConfig config_;
    RankerStats stats_;

    /**
     * @brief 计算 eCPM
     */
    void calculate_ecpm(AdCandidateList& candidates);

    /**
     * @brief 过滤无效候选
     */
    void filter_invalid(AdCandidateList& candidates);

    /**
     * @brief 过滤低 eCPM 候选
     */
    void filter_by_ecpm(AdCandidateList& candidates);

    /**
     * @brief 执行排序
     */
    void sort_candidates(AdCandidateList& candidates);

    /**
     * @brief Top-K 选择
     */
    void select_topk(AdCandidateList& candidates);

    /**
     * @brief 去重
     */
    void deduplicate(AdCandidateList& candidates);

    /**
     * @brief 更新统计信息
     */
    void update_stats(size_t input_size, size_t output_size);
};

} // namespace flow_ad_system::ranker
