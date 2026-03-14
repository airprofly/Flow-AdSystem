#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace flow_ad_system::ranker
{

    /**
     * @brief 广告候选者结构体
     *
     * 表示经过频控、预算控制、CTR预估后待排序的广告候选
     */
    struct AdCandidate
    {
        // 基本信息
        uint64_t ad_id;          ///< 广告ID
        std::string creative_id; ///< 创意ID
        uint32_t campaign_id;    ///< 计划ID
        uint32_t category;       ///< 广告类别

        // 竞价相关
        double bid_price;     ///< 竞价价格(元)
        double predicted_ctr; ///< 预估CTR (0-1)
        double ecpm = 0.0;    ///< eCPM = bid_price * ctr * 1000

        // 预算和消费信息
        std::optional<double> remaining_budget = std::nullopt; ///< 剩余预算(元)
        std::optional<uint64_t> daily_spend = std::nullopt;    ///< 今日已消费(微元)

        // 扩展字段
        std::optional<std::string> deal_id = std::nullopt;  ///< Deal ID (PMP交易)
        std::optional<double> quality_score = std::nullopt; ///< 质量分

        /**
         * @brief 计算 eCPM
         * @param bid 竞价价格(元)
         * @param ctr 预估CTR
         * @return eCPM值
         */
        static constexpr double calculate_ecpm(double bid, double ctr) noexcept
        {
            return bid * ctr * 1000.0;
        }

        /**
         * @brief 更新 eCPM 值
         */
        void update_ecpm() noexcept
        {
            ecpm = calculate_ecpm(bid_price, predicted_ctr);
        }

        /**
         * @brief 检查广告是否有效
         * @return true 如果 eCPM > 0 且 CTR 在有效范围内
         */
        bool is_valid() const noexcept
        {
            return ecpm > 0.0 && predicted_ctr > 0.0 && predicted_ctr <= 1.0;
        }

        /**
         * @brief 按类别去重的比较器
         */
        struct CategoryEqual
        {
            bool operator()(const AdCandidate &lhs, const AdCandidate &rhs) const
            {
                return lhs.category == rhs.category;
            }
        };

        /**
         * @brief 按 Deal ID 去重的比较器
         */
        struct DealEqual
        {
            bool operator()(const AdCandidate &lhs, const AdCandidate &rhs) const
            {
                return lhs.deal_id.has_value() && rhs.deal_id.has_value() &&
                       lhs.deal_id == rhs.deal_id;
            }
        };
    };

    using AdCandidateList = std::vector<AdCandidate>;

} // namespace flow_ad_system::ranker
