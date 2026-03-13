#pragma once

// Author: airprofly
//
// OpenRTB Response 构建器
// 将竞价决策结果包装成符合 OpenRTB 规范的 Response

#include "openrtb_response_models.h"
#include "auction_decision.h"
#include "openrtb_models.h"
#include "../ranker/ad_candidate.hpp"

#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace flow_ad::openrtb {

/**
 * @brief Response 构建器配置
 */
struct ResponseBuilderConfig {
    bool include_debug_ext;           ///< 是否包含调试扩展字段
    std::string win_notice_domain;    ///< 赢价通知域名
    std::string ad_markup_domain;     ///< 广告素材域名
    std::string click_tracker_domain; ///< 点击跟踪域名
    uint32_t bid_ttl_seconds;         ///< 竞价 TTL (秒)

    ResponseBuilderConfig()
        : include_debug_ext(false)
        , win_notice_domain("http://win-notice.example.com")
        , ad_markup_domain("https://cdn.example.com")
        , click_tracker_domain("https://click-tracker.example.com")
        , bid_ttl_seconds(30)
    {}
};

/**
 * @brief OpenRTB Response 构建器
 *
 * 将竞价决策结果包装成符合 OpenRTB 规范的 Response
 */
class ResponseBuilder {
public:
    explicit ResponseBuilder(const ResponseBuilderConfig& config = ResponseBuilderConfig());
    ~ResponseBuilder() = default;

    /**
     * @brief 构建 Bid Response
     * @param auction_result 竞价决策结果
     * @param request 原始 OpenRTB Request
     * @return Bid Response 对象
     */
    BidResponse build(
        const AuctionResult& auction_result,
        const OpenRTBRequest& request
    ) const;

    /**
     * @brief 构建无竞价响应 (No Bid)
     * @param request 原始 OpenRTB Request
     * @param reason 无竞价原因
     * @return 空 Bid Response
     */
    BidResponse build_no_bid(
        const OpenRTBRequest& request,
        const std::string& reason = "No eligible candidates"
    ) const;

    /**
     * @brief 更新配置
     */
    void set_config(const ResponseBuilderConfig& config) {
        config_ = config;
    }

    /**
     * @brief 获取配置
     */
    const ResponseBuilderConfig& get_config() const {
        return config_;
    }

private:
    ResponseBuilderConfig config_;
    mutable std::mt19937 rng_;  ///< 随机数生成器 (用于生成 ID)

    /**
     * @brief 构建 Bid 对象
     * @param auction_result 竞价决策结果
     * @param request 原始 OpenRTB Request
     * @param imp_id 广告位 ID
     * @return Bid 对象
     */
    Bid build_bid(
        const AuctionResult& auction_result,
        const OpenRTBRequest& request,
        const std::string& imp_id
    ) const;

    /**
     * @brief 生成竞价 ID (UUID v4 格式)
     */
    std::string generate_bid_id() const;

    /**
     * @brief 生成竞价通知 URL (nurl)
     * @param bid_id 竞价 ID
     * @return Win Notice URL
     */
    std::string generate_win_notice_url(const std::string& bid_id) const;

    /**
     * @brief 生成广告素材 Markup (adm)
     * @param ad 获胜广告
     * @param imp 对应的广告位
     * @return Ad Markup (HTML/JSON/XML)
     */
    std::string generate_ad_markup(
        const flow_ad_system::ranker::AdCandidate& ad,
        const Impression& imp
    ) const;

    /**
     * @brief 构建调试扩展字段
     * @param auction_result 竞价决策结果
     * @return JSON 字符串
     */
    std::string build_debug_ext(const AuctionResult& auction_result) const;

    /**
     * @brief 生成随机十六进制数
     */
    std::string random_hex(size_t length) const;
};

} // namespace flow_ad::openrtb
