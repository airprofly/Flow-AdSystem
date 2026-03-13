#pragma once

#include "ad_candidate.hpp"
#include <algorithm>
#include <vector>

namespace flow_ad_system::ranker {

/**
 * @brief Top-K 选择器
 *
 * 从大量候选中快速选出 eCPM 最高的 K 个广告
 * 使用 std::partial_sort 和 std::nth_element 优化性能
 */
class TopKSelector {
public:
    /**
     * @brief 选择 Top-K 候选
     * @param candidates 广告候选列表
     * @param k 需要选择的数量
     *
     * 使用 std::partial_sort,时间复杂度 O(n log k)
     */
    static void select(AdCandidateList& candidates, size_t k) {
        if (k >= candidates.size()) {
            // 如果 k 大于等于候选数量,直接排序
            std::sort(candidates.begin(), candidates.end(),
                [](const AdCandidate& a, const AdCandidate& b) {
                    return a.ecpm > b.ecpm;
                });
            return;
        }

        // 使用 partial_sort 只排序前 k 个元素
        std::partial_sort(candidates.begin(), candidates.begin() + k, candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.ecpm > b.ecpm;
            });

        // 删除剩余元素
        candidates.resize(k);
    }

    /**
     * @brief 选择 Top-K 候选(优化版本)
     * @param candidates 广告候选列表
     * @param k 需要选择的数量
     *
     * 使用 std::nth_element,时间复杂度 O(n),更快但不保证前 k 个完全有序
     */
    static void select_fast(AdCandidateList& candidates, size_t k) {
        if (k >= candidates.size()) {
            select(candidates, k);
            return;
        }

        // 使用 nth_element 找到第 k 大的元素
        std::nth_element(candidates.begin(), candidates.begin() + k, candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.ecpm > b.ecpm;
            });

        // 对前 k 个元素进行排序
        std::sort(candidates.begin(), candidates.begin() + k,
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.ecpm > b.ecpm;
            });

        // 删除剩余元素
        candidates.resize(k);
    }

    /**
     * @brief 选择 Top-K 候选(并行版本)
     * @param candidates 广告候选列表
     * @param k 需要选择的数量
     *
     * 使用 C++17 并行算法,适用于大量数据
     */
    static void select_parallel(AdCandidateList& candidates, size_t k);

    /**
     * @brief 获取第 k 大的 eCPM 值
     * @param candidates 广告候选列表
     * @param k 排名
     * @return 第 k 大的 eCPM 值,如果不存在返回 0
     */
    static double get_kth_ecpm(const AdCandidateList& candidates, size_t k) {
        if (k >= candidates.size()) {
            return 0.0;
        }

        AdCandidateList temp = candidates;
        std::nth_element(temp.begin(), temp.begin() + k, temp.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.ecpm > b.ecpm;
            });

        return temp[k].ecpm;
    }

    /**
     * @brief 过滤低于 eCPM 阈值的候选
     * @param candidates 广告候选列表
     * @param min_ecpm 最小 eCPM 阈值
     * @return 被过滤的候选数量
     */
    static size_t filter_by_ecpm(AdCandidateList& candidates, double min_ecpm) {
        auto new_end = std::remove_if(candidates.begin(), candidates.end(),
            [min_ecpm](const AdCandidate& c) {
                return c.ecpm < min_ecpm;
            });
        size_t removed = std::distance(new_end, candidates.end());
        candidates.erase(new_end, candidates.end());
        return removed;
    }

    /**
     * @brief 去重相同类别的广告(保留 eCPM 最高的)
     * @param candidates 广告候选列表
     *
     * 如果有多个同类广告,只保留 eCPM 最高的一个
     */
    static void deduplicate_by_category(AdCandidateList& candidates) {
        if (candidates.size() <= 1) {
            return;
        }

        // 先按类别和 eCPM 排序
        std::sort(candidates.begin(), candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                if (a.category != b.category) {
                    return a.category < b.category;
                }
                return a.ecpm > b.ecpm;  // 同类中 eCPM 高的在前
            });

        // 去重,保留每个类别的第一个(即 eCPM 最高的)
        auto new_end = std::unique(candidates.begin(), candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.category == b.category;
            });
        candidates.erase(new_end, candidates.end());

        // 重新按 eCPM 排序
        std::sort(candidates.begin(), candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.ecpm > b.ecpm;
            });
    }

    /**
     * @brief 去重相同 Deal ID 的广告
     * @param candidates 广告候选列表
     */
    static void deduplicate_by_deal(AdCandidateList& candidates) {
        auto new_end = std::unique(candidates.begin(), candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.deal_id.has_value() && b.deal_id.has_value() &&
                       a.deal_id == b.deal_id;
            });
        candidates.erase(new_end, candidates.end());
    }
};

} // namespace flow_ad_system::ranker
