#pragma once

// Author: airprofly
//
// OpenRTB 2.5 Bid Response 数据模型
// 用于将竞价结果包装成符合 OpenRTB 规范的响应

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace flow_ad::openrtb {

/**
 * @brief 竞价对象 (OpenRTB 2.5 Section 4.3)
 *
 * 单个广告的竞价信息
 */
struct Bid {
    // ========== 必填字段 ==========
    std::string id;              ///< 竞价 ID
    std::string impid;           ///< 广告位 ID (与 Request 对应)
    double price;                ///< 成交价 (微美元, CPM/1000)

    // ========== 推荐字段 ==========
    std::optional<std::string> adid;  ///< 广告 ID
    std::optional<std::string> nurl;  ///< 赢价通知 URL (Win Notice URL)
    std::optional<std::string> adm;   ///< 广告素材 Markup (Ad Markup)

    // ========== 可选字段 ==========
    std::optional<std::vector<std::string>> adomain;  ///< 广告主域名
    std::optional<std::vector<int>> cat;              ///< IAB 广告分类
    std::optional<std::vector<int>> attr;             ///< 广告属性
    std::optional<std::vector<int>> api;              ///< API 框架 (VPAID 1.0/2.0)
    std::optional<int> protocol;                      ///< 协议 (VAST 1.0/2.0/3.0/4.0)
    std::optional<int> qagmediarating;               ///< 媒体评级 (0-1)
    std::optional<std::string> language;             ///< 语言
    std::optional<std::string> bundle;               ///< 应用包名 (Android/iOS)
    std::optional<std::string> ext;                  ///< 扩展字段

    // ========== 辅助方法 ==========
    Bid() = default;

    Bid(const std::string& id_, const std::string& impid_, double price_)
        : id(id_), impid(impid_), price(price_) {}

    /**
     * @brief 获取成交价 (单位: 元/千次展示)
     */
    double get_price_cpm() const {
        return price / 1000.0;
    }
};

/**
 * @brief 竞价座席 (OpenRTB 2.5 Section 4.2)
 *
 * 代表一个竞价参与者(广告主/DSP)
 */
struct SeatBid {
    std::vector<Bid> bid;                      ///< 竞价列表 (必填)
    std::optional<std::string> seat;           ///< 竞价座席 ID (推荐)
    std::optional<uint32_t> group;             ///< 是否可以打包购买 (可选)
    std::optional<std::string> ext;            ///< 扩展字段 (可选)

    // ========== 辅助方法 ==========
    SeatBid() = default;

    /**
     * @brief 添加竞价
     */
    void add_bid(const Bid& bid_obj) {
        bid.push_back(bid_obj);
    }

    /**
     * @brief 判断是否有竞价
     */
    bool has_bid() const {
        return !bid.empty();
    }
};

/**
 * @brief OpenRTB Bid Response 根对象 (OpenRTB 2.5 Section 4)
 *
 * 完整的竞价响应
 */
struct BidResponse {
    std::string id;                            ///< 请求 ID (必填, 与 Bid Request 对应)
    std::vector<SeatBid> seatbid;              ///< 竞价座席列表 (必填)

    // ========== 推荐字段 ==========
    std::optional<std::string> bidid;          ///< 竞价 ID
    std::optional<std::string> cur;            ///< 货币 (默认: USD)

    // ========== 可选字段 ==========
    std::optional<std::string> nbr;            ///< 无竞价原因
    std::optional<std::string> ext;            ///< 扩展字段

    // ========== 辅助方法 ==========
    BidResponse() = default;

    explicit BidResponse(const std::string& request_id)
        : id(request_id), cur("USD") {}

    /**
     * @brief 添加竞价座席
     */
    void add_seatbid(const SeatBid& seatbid_obj) {
        seatbid.push_back(seatbid_obj);
    }

    /**
     * @brief 判断是否有竞价
     */
    bool has_bid() const {
        if (seatbid.empty()) {
            return false;
        }
        for (const auto& sb : seatbid) {
            if (!sb.bid.empty()) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 判断是否为无竞价响应
     */
    bool is_no_bid() const {
        return nbr.has_value() || !has_bid();
    }
};

/**
 * @brief 无竞价原因枚举 (OpenRTB 2.5)
 */
enum class NoBidReason {
    UNKNOWN = 0,
    NO_VALID_CANDIDATES = 1,        // 无有效候选广告
    BID_FLOOR_NOT_MET = 2,          // 未达到底价
    BUDGET_EXCEEDED = 3,            // 预算超限
    FREQUENCY_CAP = 4,              // 频次限制
    INVALID_REQUEST = 5,            // 无效请求
    PAUSED_CAMPAIGN = 6             // 计划暂停
};

/**
 * @brief 将 NoBidReason 转换为字符串
 */
inline std::string no_bid_reason_to_string(NoBidReason reason) {
    switch (reason) {
        case NoBidReason::NO_VALID_CANDIDATES:
            return "No valid candidates";
        case NoBidReason::BID_FLOOR_NOT_MET:
            return "Bid floor not met";
        case NoBidReason::BUDGET_EXCEEDED:
            return "Budget exceeded";
        case NoBidReason::FREQUENCY_CAP:
            return "Frequency cap exceeded";
        case NoBidReason::INVALID_REQUEST:
            return "Invalid request";
        case NoBidReason::PAUSED_CAMPAIGN:
            return "Campaign paused";
        default:
            return "Unknown";
    }
}

} // namespace flow_ad::openrtb
