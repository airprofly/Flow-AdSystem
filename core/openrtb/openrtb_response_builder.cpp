// Author: airprofly
//
// OpenRTB Response 构建器实现

#include "openrtb_response_builder.h"
#include <iomanip>
#include <random>
#include <sstream>

namespace flow_ad::openrtb
{

    ResponseBuilder::ResponseBuilder(const ResponseBuilderConfig &config)
        : config_(config), rng_(std::random_device{}())
    {
    }

    BidResponse ResponseBuilder::build(
        const AuctionResult &auction_result,
        const OpenRTBRequest &request) const
    {
        // 1. 检查是否有获胜者
        if (!auction_result.has_winner || !auction_result.winner.has_value())
        {
            return build_no_bid(request, auction_result.reason);
        }

        // 2. 创建 Bid Response
        BidResponse response(request.id);

        // 3. 设置货币
        response.cur = "USD";

        // 4. 生成竞价 ID
        std::string bid_id = generate_bid_id();
        response.bidid = bid_id;

        // 5. 构建 SeatBid
        SeatBid seatbid;

        // 为每个广告位构建 Bid (通常只有一个)
        for (const auto &imp : request.imp)
        {
            Bid bid = build_bid(auction_result, request, imp.id);
            seatbid.add_bid(bid);
        }

        // 6. 添加 SeatBid 到 Response
        response.add_seatbid(seatbid);

        return response;
    }

    BidResponse ResponseBuilder::build_no_bid(
        const OpenRTBRequest &request,
        const std::string &reason) const
    {
        BidResponse response(request.id);
        response.nbr = reason;
        response.cur = "USD";
        return response;
    }

    Bid ResponseBuilder::build_bid(
        const AuctionResult &auction_result,
        const OpenRTBRequest &request,
        const std::string &imp_id) const
    {
        const auto &winner = auction_result.winner.value();

        // 1. 创建 Bid
        Bid bid;
        bid.id = generate_bid_id();
        bid.impid = imp_id;
        bid.price = auction_result.final_price;

        // 2. 填充推荐字段
        bid.adid = std::to_string(winner.ad_id);
        bid.nurl = generate_win_notice_url(bid.id);

        // 3. 生成广告素材 Markup
        // 找到对应的广告位
        const Impression *target_imp = nullptr;
        for (const auto &imp : request.imp)
        {
            if (imp.id == imp_id)
            {
                target_imp = &imp;
                break;
            }
        }

        if (target_imp != nullptr)
        {
            bid.adm = generate_ad_markup(winner, *target_imp);
        }
        else
        {
            // 如果找不到对应广告位,使用默认生成方式
            bid.adm = generate_ad_markup(winner, Impression{});
        }

        // 4. 填充可选字段 (根据 winner 信息)
        if (winner.category > 0)
        {
            bid.cat = std::vector<int>{static_cast<int>(winner.category)};
        }

        bid.adomain = std::vector<std::string>{"example.com"}; // TODO: 从 Ad 模型获取

        // 5. 填充调试扩展字段
        if (config_.include_debug_ext)
        {
            bid.ext = build_debug_ext(auction_result);
        }

        return bid;
    }

    std::string ResponseBuilder::generate_bid_id() const
    {
        // 生成 UUID v4 格式的竞价 ID
        // 格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx

        std::stringstream ss;
        ss << std::hex << std::setfill('0');

        // 时间低位 (8 字符)
        for (int i = 0; i < 8; i++)
        {
            ss << random_hex(1);
        }
        ss << "-";

        // 时间中位 (4 字符)
        for (int i = 0; i < 4; i++)
        {
            ss << random_hex(1);
        }
        ss << "-";

        // 版本和时钟序列高位 (4 字符,版本为 4)
        ss << "4";
        for (int i = 0; i < 3; i++)
        {
            ss << random_hex(1);
        }
        ss << "-";

        // 时钟序列低位和变体 (4 字符,变体为 8/9/A/B)
        std::uniform_int_distribution<> dis_variant(8, 11);
        ss << std::hex << dis_variant(rng_);
        for (int i = 0; i < 3; i++)
        {
            ss << random_hex(1);
        }
        ss << "-";

        // 节点 (12 字符)
        for (int i = 0; i < 12; i++)
        {
            ss << random_hex(1);
        }

        return ss.str();
    }

    std::string ResponseBuilder::generate_win_notice_url(const std::string &bid_id) const
    {
        std::string url = config_.win_notice_domain;

        // 确保域名后缀有斜杠
        if (!url.empty() && url.back() != '/')
        {
            url += '/';
        }

        // 添加查询参数
        url += "?bid_id=" + bid_id;
        url += "&price=${AUCTION_PRICE}"; // OpenRTB 宏,会被替换为实际价格

        return url;
    }

    std::string ResponseBuilder::generate_ad_markup(
        const flow_ad_system::ranker::AdCandidate &ad,
        [[maybe_unused]] const Impression &imp) const
    {
        // 生成广告素材 HTML (简化版本,避免原始字符串嵌套问题)
        std::stringstream ss;

        // 简化版:生成基本的Banner HTML
        ss << "<div class=\"ad\" data-ad-id=\"" << ad.ad_id << "\">";
        ss << "<a href=\"" << config_.click_tracker_domain << "/click?ad_id=" << ad.ad_id << "\" target=\"_blank\">";
        ss << "<img src=\"" << config_.ad_markup_domain << "/creative/" << ad.creative_id << ".jpg\" alt=\"Ad\" />";
        ss << "</a>";
        ss << "</div>";

        return ss.str();
    }

    std::string ResponseBuilder::build_debug_ext(const AuctionResult &auction_result) const
    {
        if (!auction_result.winner.has_value())
        {
            return "{}";
        }

        const auto &winner = auction_result.winner.value();

        std::stringstream ss;
        ss << R"({"predicted_ctr":)" << winner.predicted_ctr;
        ss << R"(,"ecpm":)" << winner.ecpm;
        ss << R"(,"original_bid_price":)" << winner.bid_price;
        ss << R"(,"auction_type":)" << static_cast<int>(auction_result.auction_type);
        ss << R"(,"final_price_cpm":)" << auction_result.get_price_cpm();
        ss << R"(,"ad_id":)" << winner.ad_id;
        ss << R"(})";

        return ss.str();
    }

    std::string ResponseBuilder::random_hex(size_t length) const
    {
        std::stringstream ss;
        std::uniform_int_distribution<> dis(0, 15);

        for (size_t i = 0; i < length; ++i)
        {
            ss << std::hex << dis(rng_);
        }

        return ss.str();
    }

} // namespace flow_ad::openrtb
