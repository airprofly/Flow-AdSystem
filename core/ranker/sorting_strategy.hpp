#pragma once

#include "ad_candidate.hpp"
#include <algorithm>
#include <functional>
#include <vector>

namespace flow_ad_system::ranker {

/**
 * @brief 排序策略枚举
 */
enum class SortStrategy {
    kGreedy,    ///< 贪心排序 - 按eCPM降序
    kDiverse,   ///< 多样性排序 - 兼顾eCPM和类别多样性
    kBudget     ///< 预算优化排序 - 考虑剩余预算
};

/**
 * @brief 贪心排序策略
 *
 * 纯粹按 eCPM 降序排序,最大化收益
 */
class GreedySort {
public:
    static void sort(AdCandidateList& candidates) noexcept {
        std::sort(candidates.begin(), candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                // eCPM 降序
                if (a.ecpm != b.ecpm) {
                    return a.ecpm > b.ecpm;
                }
                // eCPM相同时,按质量分降序
                if (a.quality_score.has_value() && b.quality_score.has_value()) {
                    return a.quality_score.value() > b.quality_score.value();
                }
                return false;
            });
    }
};

/**
 * @brief 多样性排序策略
 *
 * 在 eCPM 和类别多样性之间取得平衡
 * 避免同类广告连续出现
 */
class DiverseSort {
public:
    /**
     * @brief 执行多样性排序
     * @param candidates 广告候选列表
     * @param diversity_weight 多样性权重 (0-1),默认0.3
     *
     * diversity_weight 越大,越重视类别多样性
     */
    static void sort(AdCandidateList& candidates, double diversity_weight = 0.3) noexcept {
        if (candidates.size() <= 1) {
            return;
        }

        // 先按 eCPM 降序排序
        std::sort(candidates.begin(), candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.ecpm > b.ecpm;
            });

        // 使用改进的贪心算法进行多样性重排
        AdCandidateList result;
        result.reserve(candidates.size());

        std::vector<bool> used(candidates.size(), false);

        for (size_t i = 0; i < candidates.size(); ++i) {
            size_t best_idx = find_best_candidate(candidates, used, result, diversity_weight);
            used[best_idx] = true;
            result.push_back(candidates[best_idx]);
        }

        candidates = std::move(result);
    }

private:
    /**
     * @brief 找到下一个最佳候选
     */
    static size_t find_best_candidate(
        const AdCandidateList& candidates,
        const std::vector<bool>& used,
        const AdCandidateList& result,
        double diversity_weight) noexcept {

        size_t best_idx = 0;
        double best_score = -1.0;

        for (size_t i = 0; i < candidates.size(); ++i) {
            if (used[i]) continue;

            double score = calculate_score(candidates[i], result, diversity_weight);
            if (score > best_score) {
                best_score = score;
                best_idx = i;
            }
        }

        return best_idx;
    }

    /**
     * @brief 计算候选的得分
     */
    static double calculate_score(
        const AdCandidate& candidate,
        const AdCandidateList& result,
        double diversity_weight) noexcept {

        // 归一化 eCPM 得分
        double ecpm_score = candidate.ecpm;

        // 多样性惩罚:如果结果中已有同类广告,降低得分
        double diversity_penalty = 0.0;
        if (!result.empty()) {
            bool has_same_category = std::any_of(
                result.rbegin(), std::rbegin(result) + std::min<size_t>(3, result.size()),
                [&candidate](const AdCandidate& c) {
                    return c.category == candidate.category;
                });
            if (has_same_category) {
                diversity_penalty = 1.0;
            }
        }

        // 综合得分 = eCPM * (1 - diversity_weight * diversity_penalty)
        return ecpm_score * (1.0 - diversity_weight * diversity_penalty);
    }
};

/**
 * @brief 预算优化排序策略
 *
 * 考虑广告剩余预算,避免预算不足的广告排在前面
 */
class BudgetSort {
public:
    /**
     * @brief 执行预算优化排序
     * @param candidates 广告候选列表
     * @param budget_weight 预算权重 (0-1),默认0.2
     */
    static void sort(AdCandidateList& candidates, double budget_weight = 0.2) noexcept {
        std::sort(candidates.begin(), candidates.end(),
            [budget_weight](const AdCandidate& a, const AdCandidate& b) {
                // 计算 eCPM 和预算因子的综合得分
                double score_a = calculate_budget_score(a, budget_weight);
                double score_b = calculate_budget_score(b, budget_weight);
                return score_a > score_b;
            });
    }

private:
    /**
     * @brief 计算预算优化得分
     */
    static double calculate_budget_score(const AdCandidate& candidate, double budget_weight) noexcept {
        double ecpm_score = candidate.ecpm;

        // 预算因子:剩余预算越多,因子越大
        double budget_factor = 1.0;
        if (candidate.remaining_budget.has_value() && candidate.remaining_budget.value() > 0) {
            // 简单的预算因子:假设基准预算为100元
            budget_factor = std::min(2.0, 1.0 + candidate.remaining_budget.value() / 100.0);
        }

        // 综合得分 = eCPM * (1 + budget_weight * (budget_factor - 1))
        return ecpm_score * (1.0 + budget_weight * (budget_factor - 1.0));
    }
};

/**
 * @brief 排序器 - 统一的排序接口
 */
class Sorter {
public:
    /**
     * @brief 执行排序
     * @param candidates 广告候选列表
     * @param strategy 排序策略
     */
    static void sort(AdCandidateList& candidates, SortStrategy strategy = SortStrategy::kGreedy) {
        switch (strategy) {
            case SortStrategy::kGreedy:
                GreedySort::sort(candidates);
                break;
            case SortStrategy::kDiverse:
                DiverseSort::sort(candidates);
                break;
            case SortStrategy::kBudget:
                BudgetSort::sort(candidates);
                break;
        }
    }

    /**
     * @brief 带参数的多样性排序
     */
    static void sort_diverse(AdCandidateList& candidates, double diversity_weight) {
        DiverseSort::sort(candidates, diversity_weight);
    }

    /**
     * @brief 带参数的预算优化排序
     */
    static void sort_budget(AdCandidateList& candidates, double budget_weight) {
        BudgetSort::sort(candidates, budget_weight);
    }
};

} // namespace flow_ad_system::ranker
