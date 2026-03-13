#pragma once

#include "ad_candidate.hpp"
#include <algorithm>
#include <vector>

namespace flow_ad_system::ranker {

/**
 * @brief eCPM 计算器
 *
 * 提供批量计算 eCPM 的功能,支持SIMD优化
 */
class EcpmCalculator {
public:
    /**
     * @brief 批量计算 eCPM
     * @param candidates 广告候选列表
     */
    static void calculate(AdCandidateList& candidates) noexcept {
        for (auto& candidate : candidates) {
            candidate.update_ecpm();
        }
    }

    /**
     * @brief 计算单个广告的 eCPM
     * @param bid_price 竞价价格(元)
     * @param predicted_ctr 预估CTR
     * @return eCPM值
     */
    static constexpr double calculate_single(double bid_price, double predicted_ctr) noexcept {
        return AdCandidate::calculate_ecpm(bid_price, predicted_ctr);
    }

    /**
     * @brief 过滤无效的广告候选
     * @param candidates 广告候选列表
     * @return 有效候选数量
     */
    static size_t filter_invalid(AdCandidateList& candidates) noexcept {
        auto new_end = std::remove_if(candidates.begin(), candidates.end(),
            [](const AdCandidate& c) { return !c.is_valid(); });
        candidates.erase(new_end, candidates.end());
        return candidates.size();
    }

    /**
     * @brief 并行计算大量候选的 eCPM (C++17)
     * @param candidates 广告候选列表
     *
     * 当候选数量超过阈值时自动使用并行计算
     */
    static void calculate_parallel(AdCandidateList& candidates) noexcept;
};

} // namespace flow_ad_system::ranker
