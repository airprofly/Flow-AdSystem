// Author: airprofly
//
// 竞价决策模块实现

#include "auction_decision.h"
#include <algorithm>
#include <limits>

namespace flow_ad::openrtb {

AuctionResult AuctionDecision::decide(
    const std::vector<flow_ad_system::ranker::AdCandidate>& sorted_candidates,
    const OpenRTBRequest& request
) const {
    AuctionResult result;
    result.auction_type = auction_type_;
    result.has_winner = false;
    result.final_price = 0.0;

    // 1. 检查是否有候选广告
    if (sorted_candidates.empty()) {
        result.reason = no_bid_reason_to_string(NoBidReason::NO_VALID_CANDIDATES);
        return result;
    }

    // 2. 选择 eCPM 最高的广告作为潜在赢家
    const auto& winner = sorted_candidates[0];

    // 3. 检查广告是否有效
    if (!winner.is_valid()) {
        result.reason = "Invalid candidate (CTR or eCPM out of range)";
        return result;
    }

    // 4. 检查底价要求 (如果有)
    if (!check_bid_floor(winner, request)) {
        result.reason = no_bid_reason_to_string(NoBidReason::BID_FLOOR_NOT_MET);
        return result;
    }

    // 5. 检查预算 (如果有)
    if (winner.remaining_budget.has_value() && winner.remaining_budget.value() <= 0.0) {
        result.reason = no_bid_reason_to_string(NoBidReason::BUDGET_EXCEEDED);
        return result;
    }

    // 6. 获取第二高出价 (用于 Second Price 拍卖)
    std::optional<double> second_bid;
    if (sorted_candidates.size() >= 2) {
        const auto& second_candidate = sorted_candidates[1];
        if (second_candidate.is_valid()) {
            second_bid = second_candidate.bid_price;
        }
    }

    // 7. 根据拍卖类型计算成交价
    if (auction_type_ == AuctionType::FIRST_PRICE) {
        result.final_price = first_price_auction(winner);
    } else {
        result.final_price = second_price_auction(winner, second_bid);
    }

    // 8. 设置获胜者
    result.winner = winner;
    result.has_winner = true;
    result.reason = "Auction completed successfully";

    return result;
}

double AuctionDecision::first_price_auction(
    const flow_ad_system::ranker::AdCandidate& winner
) const {
    // First Price: 成交价 = 赢家出价
    // 注意: bid_price 单位是元,需要转换为微美元 (元/千次展示 * 1000)
    return to_micro_cpm(winner.bid_price);
}

double AuctionDecision::second_price_auction(
    const flow_ad_system::ranker::AdCandidate& winner,
    const std::optional<double>& second_bid
) const {
    double winner_bid = winner.bid_price;

    if (second_bid.has_value()) {
        // Second Price: 成交价 = 第二高价 + 0.01
        double price = second_bid.value() + 0.01;

        // 至少为赢家出价的 90% (floor price),避免价格过低
        double floor_price = winner_bid * 0.9;

        // 取较大值
        price = std::max(price, floor_price);

        return to_micro_cpm(price);
    } else {
        // 如果没有第二高价,使用 First Price
        return to_micro_cpm(winner_bid);
    }
}

bool AuctionDecision::check_bid_floor(
    const flow_ad_system::ranker::AdCandidate& candidate,
    const OpenRTBRequest& request
) const {
    // 如果 request 中没有广告位,或者广告位没有底价,则认为满足底价要求
    if (request.imp.empty()) {
        return true;
    }

    // 检查第一个广告位的底价 (通常只有一个广告位)
    const auto& imp = request.imp[0];
    if (!imp.bidfloor.has_value()) {
        return true;
    }

    // 比较出价和底价
    // bidfloor 单位是元/千次展示,candidate.bid_price 也是元/千次展示
    double bid_floor = imp.bidfloor.value();

    return candidate.bid_price >= bid_floor;
}

} // namespace flow_ad::openrtb
