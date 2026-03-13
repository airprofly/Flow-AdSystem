#pragma once

// Author: airprofly
//
// 竞价决策模块
// 实现 First Price 和 Second Price 拍卖算法

#include "openrtb_models.h"
#include "openrtb_response_models.h"
#include "../ranker/ad_candidate.hpp"

#include <optional>
#include <string>
#include <vector>

namespace flow_ad::openrtb {

/**
 * @brief 拍卖类型 (使用 OpenRTB 2.5 定义)
 */
using AuctionType = openrtb::AuctionType;

/**
 * @brief 竞价决策结果
 */
struct AuctionResult {
    bool has_winner;                                       ///< 是否有获胜广告
    std::optional<flow_ad_system::ranker::AdCandidate> winner;  ///< 获胜广告
    double final_price;                                    ///< 最终成交价 (微美元)
    AuctionType auction_type;                              ///< 拍卖类型
    std::string reason;                                    ///< 未获胜原因 (如果无获胜者)

    /**
     * @brief 获取成交价 (单位: 元/千次展示)
     */
    double get_price_cpm() const {
        return final_price / 1000.0;
    }

    /**
     * @brief 获取成交价 (单位: 元)
     */
    double get_price_yuan() const {
        return final_price / 1000000.0;
    }
};

/**
 * @brief 竞价决策器
 *
 * 根据拍卖类型和排序后的广告列表,决定获胜广告和成交价
 */
class AuctionDecision {
public:
    explicit AuctionDecision(AuctionType auction_type = AuctionType::SECOND_PRICE)
        : auction_type_(auction_type) {}

    ~AuctionDecision() = default;

    /**
     * @brief 执行竞价决策
     * @param sorted_candidates 排序后的广告列表 (按 eCPM 降序)
     * @param request OpenRTB Request (用于获取广告位 ID 等)
     * @return 竞价决策结果
     *
     * 算法流程:
     * 1. 选择 eCPM 最高的广告
     * 2. 检查预算和频控 (如果有)
     * 3. 根据拍卖类型计算成交价
     * 4. 返回获胜广告和成交价
     */
    AuctionResult decide(
        const std::vector<flow_ad_system::ranker::AdCandidate>& sorted_candidates,
        const OpenRTBRequest& request
    ) const;

    /**
     * @brief 设置拍卖类型
     */
    void set_auction_type(AuctionType type) {
        auction_type_ = type;
    }

    /**
     * @brief 获取拍卖类型
     */
    AuctionType get_auction_type() const {
        return auction_type_;
    }

private:
    AuctionType auction_type_;

    /**
     * @brief First Price 拍卖
     * @param winner 获胜广告
     * @return 成交价 (微美元)
     *
     * 公式: price = bid_price (转换为微美元)
     */
    double first_price_auction(const flow_ad_system::ranker::AdCandidate& winner) const;

    /**
     * @brief Second Price 拍卖
     * @param winner 获胜广告
     * @param second_bid 第二高出价 (如果有)
     * @return 成交价 (微美元)
     *
     * 公式: price = max(second_bid + 0.01, winner_bid * 0.9)
     * 说明: 至少为赢家出价的 90%,避免价格过低
     */
    double second_price_auction(
        const flow_ad_system::ranker::AdCandidate& winner,
        const std::optional<double>& second_bid
    ) const;

    /**
     * @brief 将出价转换为微美元
     * @param bid_price_cpm 出价 (元/千次展示)
     * @return 微美元
     *
     * 公式: micro_cpm = bid_price_cpm * 1000000 / 1000 = bid_price_cpm * 1000
     */
    static double to_micro_cpm(double bid_price_cpm) {
        return bid_price_cpm * 1000.0;
    }

    /**
     * @brief 检查广告是否满足底价要求
     * @param candidate 广告候选
     * @param request OpenRTB Request
     * @return true 如果满足底价要求
     */
    bool check_bid_floor(
        const flow_ad_system::ranker::AdCandidate& candidate,
        const OpenRTBRequest& request
    ) const;
};

} // namespace flow_ad::openrtb
