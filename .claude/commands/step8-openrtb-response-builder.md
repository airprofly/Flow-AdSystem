# Step 8: OpenRTB Response 响应包装模块

## 📋 任务概述

**业务目标**: 实现广告投放系统的最后一环——**OpenRTB Bid Response 响应包装模块**,将排序后的竞价结果包装成符合 OpenRTB 2.5/2.6 规范的响应数据,用于与 DSP/SSP 对接。

**前置依赖**:
- ✅ Step 1: 数据生成模块(10 万+ 广告池)
- ✅ Step 2: OpenRTB 2.5 协议解析器
- ✅ Step 3: 广告索引引擎(海选召回)
- ✅ Step 4: ECPM 排序模块(Ranker)
- ✅ Step 6: CTR 预估模块(可选,用于返回调试信息)

**当前状态**: 🚀 **规划完成,待实现**

---

## 🎯 核心目标

### 功能目标
1. **构建 OpenRTB Response**: 根据 OpenRTB 2.5 规范构建 Bid Response
2. **竞价决策**: 实现 First Price 和 Second Price 拍卖算法
3. **价格计算**: 根据拍卖类型计算最终成交价
4. **JSON 序列化**: 将响应对象序列化为 JSON 字符串
5. **调试支持**: 支持返回扩展字段(如预估 CTR、eCPM 等)

### 性能指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 响应构建延迟 | P99 < 5ms | 构建单个响应 |
| 序列化延迟 | P99 < 3ms | JSON 序列化 |
| 总响应时间 | < 50ms | 包含所有步骤 |
| 内存占用 | < 10MB | 响应对象 |

---

## 📐 架构设计

### 系统定位

```
┌─────────────────────────────────────────────────────────────┐
│                   广告投放系统流程                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1️⃣ 接收请求 (OpenRTB Bid Request)                          │
│     └─> Step 2: OpenRTB Parser                              │
│         └─> 解析为结构化对象                                 │
│                                                              │
│  2️⃣ 海选召回 (Step 3: Ad Indexing)                          │
│     └─> 输出: 倒排索引 + 候选广告                             │
│                                                              │
│  3️⃣ CTR 预估 (Step 6: CTR Prediction)                      │
│     └─> 输出: ad_id → predicted_ctr                         │
│                                                              │
│  4️⃣ ECPM 排序 (Step 4: Ranker)                             │
│     └─> 输出: Top-K 排序后的广告                             │
│                                                              │
│  5️⃣ 竞价决策 & 响应构建 (Step 5) ⭐ 本步骤目标               │
│     ├─> 5.1 竞价决策 (AuctionDecision)                       │
│     │   ├─> First Price Auction                            │
│     │   └─> Second Price Auction                           │
│     ├─> 5.2 选择获胜广告                                     │
│     └─> 5.3 构建 OpenRTB Response (ResponseBuilder)         │
│         ├─> 填充 SeatBid                                    │
│         ├─> 填充 Bid                                        │
│         ├─> 计算成交价 (price)                              │
│         └─> 序列化为 JSON                                    │
│                                                              │
│  6️⃣ 返回响应                                                 │
│     └─> HTTP 200 OK + OpenRTB Bid Response JSON             │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### OpenRTB Bid Response 结构

```
OpenRTB Bid Response (OpenRTB 2.5 规范)
├── id (必填) - 请求 ID (与 Bid Request 对应)
├── seatbid (必填) - 竞价座席列表 [SeatBid]
│   └── [0]
│       ├── bid (必填) - 竞价列表 [Bid]
│       │   └── [0]
│       │       ├── id (必填) - 竞价 ID
│       │       ├── impid (必填) - 广告位 ID (与 Request 对应)
│       │       ├── price (必填) - 成交价 (微美元, CPM/1000)
│       │       ├── adid (推荐) - 广告 ID
│       │       ├── nurl (推荐) - 赢价通知 URL
│       │       ├── adm (可选) - 广告素材 Markup
│       │       ├── adomain (可选) - 广告主域名
│       │       ├── cat (可选) - IAB 广告分类
│       │       ├── attr (可选) - 广告属性
│       │       ├── api (可选) - API 框架
│       │       ├── protocol (可选) - 协议
│       │       ├── qagmediarating (可选) - 媒体评级
│       │       ├── language (可选) - 语言
│       │       ├── bundle (可选) - 应用包名
│       │       └── ext (可选) - 扩展字段
│       └── seat (推荐) - 竞价座席 ID
└── bidid (推荐) - 竞价 ID
└── cur (推荐) - 货币 (默认: USD)
└── ext (可选) - 扩展字段
```

### 竞价决策流程

```
┌─────────────────────────────────────────────────────────────┐
│                    竞价决策模块                               │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  输入: 排序后的广告列表 (按 eCPM 降序)                        │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         1. 确定拍卖类型                               │   │
│  │  - First Price Auction (一价拍卖)                   │   │
│  │    成交价 = 赢家出价                                 │   │
│  │  - Second Price Auction (二价拍卖)                  │   │
│  │    成交价 = max(赢家出价, 第二高出价 + 0.01)         │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         2. 选择获胜广告                               │   │
│  │  - 排序第一位的广告获胜                              │   │
│  │  - 检查预算是否充足                                  │   │
│  │  - 检查频控是否允许                                  │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         3. 计算成交价                                 │   │
│  │  First Price: price = bid_price (单位转换)          │   │
│  │  Second Price: price = second_bid_price + 0.01      │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  输出: 获胜广告 + 成交价                                     │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 📁 文件结构

### 新增文件

```
core/openrtb/
├── openrtb_response_models.h          # OpenRTB Response 数据模型
├── openrtb_response_builder.h         # Response 构建器
├── openrtb_response_builder.cpp       # Response 构建器实现
├── auction_decision.h                 # 竞价决策算法
├── auction_decision.cpp               # 竞价决策实现
└── response_serializer.h              # JSON 序列化器

tests/unit/
└── openrtb_response_test.cpp          # 单元测试

tests/benchmark/
└── response_builder_benchmark.cpp     # 性能基准测试
```

### 修改文件

```
core/openrtb/
└── CMakeLists.txt                     # 添加新文件
```

---

## 🚀 实现阶段

### Phase 1: OpenRTB Response 数据模型 (2-3 小时)

**文件**: `core/openrtb/openrtb_response_models.h`

**实现要点**:

```cpp
#pragma once

// Author: airprofly

#include <string>
#include <vector>
#include <optional>

namespace flow_ad::openrtb {

/**
 * @brief 竞价对象 (OpenRTB 2.5 Section 4.3)
 *
 * 单个广告的竞价信息
 */
struct Bid {
    std::string id;                           // 必填: 竞价 ID
    std::string impid;                        // 必填: 广告位 ID (与 Request 对应)
    double price;                             // 必填: 成交价 (微美元, CPM/1000)

    // ========== 推荐字段 ==========
    std::optional<std::string> adid;          // 广告 ID
    std::optional<std::string> nurl;          // 赢价通知 URL (Win Notice URL)
    std::optional<std::string> adm;           // 广告素材 Markup (Ad Markup)

    // ========== 可选字段 ==========
    std::optional<std::vector<std::string>> adomain;  // 广告主域名
    std::optional<std::vector<int>> cat;             // IAB 广告分类
    std::optional<std::vector<int>> attr;            // 广告属性
    std::optional<std::vector<int>> api;             // API 框架 (VPAID 1.0/2.0)
    std::optional<int> protocol;                    // 协议 (VAST 1.0/2.0/3.0/4.0)
    std::optional<int> qagmediarating;             // 媒体评级 (0-1)
    std::optional<std::string> language;           // 语言
    std::optional<std::string> bundle;            // 应用包名 (Android/iOS)
    std::optional<std::string> ext;               // 扩展字段

    // ========== 辅助方法 ==========
    Bid() = default;

    Bid(
        const std::string& id_,
        const std::string& impid_,
        double price_
    ) : id(id_), impid(impid_), price(price_) {}
};

/**
 * @brief 竞价座席 (OpenRTB 2.5 Section 4.2)
 *
 * 代表一个竞价参与者(广告主/DSP)
 */
struct SeatBid {
    std::vector<Bid> bid;                      // 必填: 竞价列表
    std::optional<std::string> seat;           // 推荐: 竞价座席 ID
    std::optional<uint32_t> group;             // 可选: 是否可以打包购买
    std::optional<uint32_t> ext;               // 可选: 扩展字段

    // ========== 辅助方法 ==========
    SeatBid() = default;

    /**
     * @brief 添加竞价
     */
    void add_bid(const Bid& bid) {
        this->bid.push_back(bid);
    }
};

/**
 * @brief OpenRTB Bid Response 根对象 (OpenRTB 2.5 Section 4)
 *
 * 完整的竞价响应
 */
struct BidResponse {
    std::string id;                            // 必填: 请求 ID (与 Bid Request 对应)
    std::vector<SeatBid> seatbid;              // 必填: 竞价座席列表

    // ========== 推荐字段 ==========
    std::optional<std::string> bidid;          // 竞价 ID
    std::optional<std::string> cur;            // 货币 (默认: USD)

    // ========== 可选字段 ==========
    std::optional<std::string> nbr;            // 无竞价原因
    std::optional<std::string> ext;            // 扩展字段

    // ========== 辅助方法 ==========
    BidResponse() = default;

    explicit BidResponse(const std::string& request_id)
        : id(request_id), cur("USD") {}

    /**
     * @brief 添加竞价座席
     */
    void add_seatbid(const SeatBid& seatbid) {
        this->seatbid.push_back(seatbid);
    }

    /**
     * @brief 判断是否有竞价
     */
    bool has_bid() const {
        if (seatbid.empty()) return false;
        for (const auto& sb : seatbid) {
            if (!sb.bid.empty()) return true;
        }
        return false;
    }
};

/**
 * @brief 扩展字段 (用于调试)
 *
 * 可以在 ext 字段中返回调试信息,如预估 CTR、eCPM 等
 */
struct DebugExt {
    std::optional<double> predicted_ctr;       // 预估 CTR
    std::optional<double> ecpm;                // eCPM
    std::optional<double> original_bid_price;  // 原始出价
    std::optional<uint64_t> auction_time_us;   // 竞价耗时 (微秒)

    /**
     * @brief 转换为 JSON 字符串 (用于 ext 字段)
     */
    std::string to_json() const;
};

} // namespace flow_ad::openrtb
```

**预计工作量**: 2-3 小时

---

### Phase 2: 竞价决策算法实现 (3-4 小时)

**文件**: `core/openrtb/auction_decision.h`, `core/openrtb/auction_decision.cpp`

**头文件**:

```cpp
#pragma once

// Author: airprofly

#include "openrtb_response_models.h"
#include "openrtb_models.h"
#include "../ranker/ad_candidate.hpp"
#include <vector>
#include <optional>

namespace flow_ad::openrtb {

/**
 * @brief 拍卖类型
 */
enum class AuctionType {
    FIRST_PRICE = 1,      // 一价拍卖 (成交价 = 出价)
    SECOND_PRICE = 2      // 二价拍卖 (成交价 = 第二高价 + 0.01)
};

/**
 * @brief 竞价决策结果
 */
struct AuctionResult {
    bool has_winner;                           // 是否有获胜广告
    std::optional<ranker::AdCandidate> winner; // 获胜广告
    double final_price;                        // 最终成交价 (微美元)
    AuctionType auction_type;                  // 拍卖类型
    std::string reason;                        // 未获胜原因 (如果无获胜者)

    /**
     * @brief 获取成交价 (单位: 元/千次展示)
     */
    double get_price_cpm() const {
        return final_price / 1000.0;
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
        const std::vector<ranker::AdCandidate>& sorted_candidates,
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
    double first_price_auction(const ranker::AdCandidate& winner) const;

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
        const ranker::AdCandidate& winner,
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
};

} // namespace flow_ad::openrtb
```

**实现文件**:

```cpp
// Author: airprofly

#include "auction_decision.h"
#include <algorithm>
#include <stdexcept>

namespace flow_ad::openrtb {

AuctionResult AuctionDecision::decide(
    const std::vector<ranker::AdCandidate>& sorted_candidates,
    const OpenRTBRequest& request
) const {
    AuctionResult result;
    result.auction_type = auction_type_;
    result.has_winner = false;

    // 1. 检查是否有候选广告
    if (sorted_candidates.empty()) {
        result.reason = "No candidates available";
        return result;
    }

    // 2. 选择 eCPM 最高的广告作为潜在赢家
    const auto& winner = sorted_candidates[0];
    result.winner = winner;
    result.has_winner = true;

    // 3. 获取第二高出价 (用于 Second Price 拍卖)
    std::optional<double> second_bid;
    if (sorted_candidates.size() >= 2) {
        second_bid = sorted_candidates[1].bid_price;
    }

    // 4. 根据拍卖类型计算成交价
    if (auction_type_ == AuctionType::FIRST_PRICE) {
        result.final_price = first_price_auction(winner);
    } else {
        result.final_price = second_price_auction(winner, second_bid);
    }

    result.reason = "Auction completed successfully";
    return result;
}

double AuctionDecision::first_price_auction(
    const ranker::AdCandidate& winner
) const {
    // First Price: 成交价 = 赢家出价
    return to_micro_cpm(winner.bid_price);
}

double AuctionDecision::second_price_auction(
    const ranker::AdCandidate& winner,
    const std::optional<double>& second_bid
) const {
    double winner_bid = winner.bid_price;

    if (second_bid.has_value()) {
        // Second Price: 成交价 = 第二高价 + 0.01
        double price = second_bid.value() + 0.01;

        // 至少为赢家出价的 90% (floor price)
        double floor_price = winner_bid * 0.9;

        return to_micro_cpm(std::max(price, floor_price));
    } else {
        // 如果没有第二高价,使用 First Price
        return to_micro_cpm(winner_bid);
    }
}

} // namespace flow_ad::openrtb
```

**预计工作量**: 3-4 小时

---

### Phase 3: Response 构建器实现 (4-6 小时)

**文件**: `core/openrtb/openrtb_response_builder.h`, `core/openrtb/openrtb_response_builder.cpp`

**头文件**:

```cpp
#pragma once

// Author: airprofly

#include "openrtb_response_models.h"
#include "auction_decision.h"
#include "openrtb_models.h"
#include "../ranker/ad_candidate.hpp"
#include <string>
#include <vector>

namespace flow_ad::openrtb {

/**
 * @brief Response 构建器配置
 */
struct ResponseBuilderConfig {
    bool include_debug_ext;               // 是否包含调试扩展字段
    std::string win_notice_domain;        // 赢价通知域名
    std::string ad_markup_domain;         // 广告素材域名
    uint32_t bid_ttl_seconds;             // 竞价 TTL (秒)

    ResponseBuilderConfig()
        : include_debug_ext(false)
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
     * @brief 生成竞价 ID
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
     * @return Ad Markup (HTML/JSON/XML)
     */
    std::string generate_ad_markup(const ranker::AdCandidate& ad) const;

    /**
     * @brief 构建调试扩展字段
     * @param auction_result 竞价决策结果
     * @return JSON 字符串
     */
    std::string build_debug_ext(const AuctionResult& auction_result) const;
};

} // namespace flow_ad::openrtb
```

**实现文件**:

```cpp
// Author: airprofly

#include "openrtb_response_builder.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace flow_ad::openrtb {

ResponseBuilder::ResponseBuilder(const ResponseBuilderConfig& config)
    : config_(config) {}

BidResponse ResponseBuilder::build(
    const AuctionResult& auction_result,
    const OpenRTBRequest& request
) const {
    // 1. 检查是否有获胜者
    if (!auction_result.has_winner || !auction_result.winner.has_value()) {
        return build_no_bid(request, "No winner in auction");
    }

    // 2. 创建 Bid Response
    BidResponse response(request.id);

    // 3. 生成竞价 ID
    std::string bid_id = generate_bid_id();
    response.bidid = bid_id;

    // 4. 构建 SeatBid
    SeatBid seatbid;

    // 为每个广告位构建 Bid (通常只有一个)
    for (const auto& imp : request.imp) {
        Bid bid = build_bid(auction_result, request, imp.id);
        seatbid.add_bid(bid);
    }

    // 5. 添加 SeatBid 到 Response
    response.add_seatbid(seatbid);

    return response;
}

BidResponse ResponseBuilder::build_no_bid(
    const OpenRTBRequest& request,
    const std::string& reason
) const {
    BidResponse response(request.id);
    response.nbr = reason;
    return response;
}

Bid ResponseBuilder::build_bid(
    const AuctionResult& auction_result,
    const OpenRTBRequest& request,
    const std::string& imp_id
) const {
    const auto& winner = auction_result.winner.value();

    // 1. 创建 Bid
    Bid bid;
    bid.id = generate_bid_id();
    bid.impid = imp_id;
    bid.price = auction_result.final_price;

    // 2. 填充推荐字段
    bid.adid = std::to_string(winner.ad_id);
    bid.nurl = generate_win_notice_url(bid.id);
    bid.adm = generate_ad_markup(winner);

    // 3. 填充可选字段 (根据 winner 信息)
    if (winner.category > 0) {
        bid.cat = std::vector<int>{static_cast<int>(winner.category)};
    }

    bid.adomain = std::vector<std::string>{"example.com"};  // TODO: 从 Ad 模型获取

    // 4. 填充调试扩展字段
    if (config_.include_debug_ext) {
        bid.ext = build_debug_ext(auction_result);
    }

    return bid;
}

std::string ResponseBuilder::generate_bid_id() const {
    // 生成唯一竞价 ID (UUID 格式)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }

    return ss.str();
}

std::string ResponseBuilder::generate_win_notice_url(const std::string& bid_id) const {
    std::string url = config_.win_notice_domain;
    if (url.empty()) {
        url = "http://win-notice.example.com";
    }

    if (url.back() != '/') {
        url += '/';
    }

    url += "?bid_id=" + bid_id;
    url += "&price=${AUCTION_PRICE}";  // OpenRTB 宏,会被替换为实际价格

    return url;
}

std::string ResponseBuilder::generate_ad_markup(const ranker::AdCandidate& ad) const {
    // 生成广告素材 (简化版,实际应该从创意库获取)
    std::stringstream ss;

    ss << R"(<div class="ad" data-ad-id=")" << ad.ad_id << R"(">)";
    ss << R"(  <a href="https://click-tracker.example.com/click?ad_id=)" << ad.ad_id << R"(">)";
    ss << R"(    <img src="https://cdn.example.com/creative/)" << ad.creative_id << R"(.jpg" alt="Ad" />)";
    ss << R"(  </a>)";
    ss << R"(</div>)";

    return ss.str();
}

std::string ResponseBuilder::build_debug_ext(const AuctionResult& auction_result) const {
    if (!auction_result.winner.has_value()) {
        return "{}";
    }

    const auto& winner = auction_result.winner.value();

    std::stringstream ss;
    ss << R"({"predicted_ctr":)" << winner.predicted_ctr;
    ss << R"(,"ecpm":)" << winner.ecpm;
    ss << R"(,"original_bid_price":)" << winner.bid_price;
    ss << R"(,"auction_type":)" << static_cast<int>(auction_result.auction_type);
    ss << R"(,"final_price_cpm":)" << auction_result.get_price_cpm();
    ss << R"(})";

    return ss.str();
}

} // namespace flow_ad::openrtb
```

**预计工作量**: 4-6 小时

---

### Phase 4: JSON 序列化器实现 (2-3 小时)

**文件**: `core/openrtb/response_serializer.h`

**实现要点**:

```cpp
#pragma once

// Author: airprofly

#include "openrtb_response_models.h"
#include <string>
#include <fstream>

namespace flow_ad::openrtb {

/**
 * @brief Response 序列化器
 *
 * 将 BidResponse 对象序列化为 JSON 字符串
 */
class ResponseSerializer {
public:
    ResponseSerializer() = default;
    ~ResponseSerializer() = default;

    /**
     * @brief 将 BidResponse 序列化为 JSON 字符串
     * @param response BidResponse 对象
     * @param pretty 是否格式化输出 (默认: false)
     * @return JSON 字符串
     */
    static std::string serialize(const BidResponse& response, bool pretty = false);

    /**
     * @brief 将 BidResponse 序列化并保存到文件
     * @param response BidResponse 对象
     * @param file_path 文件路径
     * @param pretty 是否格式化输出
     * @return 是否成功
     */
    static bool save_to_file(
        const BidResponse& response,
        const std::string& file_path,
        bool pretty = true
    );

private:
    /**
     * @brief 序列化 Bid 对象
     */
    static std::string serialize_bid(const Bid& bid, bool pretty);

    /**
     * @brief 序列化 SeatBid 对象
     */
    static std::string serialize_seatbid(const SeatBid& seatbid, bool pretty);
};

} // namespace flow_ad::openrtb
```

**实现文件**:

```cpp
// Author: airprofly

#include "response_serializer.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace flow_ad::openrtb {

using json = nlohmann::json;

std::string ResponseSerializer::serialize(const BidResponse& response, bool pretty) {
    json j;

    // 必填字段
    j["id"] = response.id;

    // seatbid (必填)
    json seatbid_array = json::array();
    for (const auto& seatbid : response.seatbid) {
        json seatbid_obj;

        // bid (必填)
        json bid_array = json::array();
        for (const auto& bid : seatbid.bid) {
            json bid_obj;
            bid_obj["id"] = bid.id;
            bid_obj["impid"] = bid.impid;
            bid_obj["price"] = bid.price;

            // 推荐字段
            if (bid.adid) bid_obj["adid"] = *bid.adid;
            if (bid.nurl) bid_obj["nurl"] = *bid.nurl;
            if (bid.adm) bid_obj["adm"] = *bid.adm;

            // 可选字段
            if (bid.adomain) bid_obj["adomain"] = *bid.adomain;
            if (bid.cat) bid_obj["cat"] = *bid.cat;
            if (bid.attr) bid_obj["attr"] = *bid.attr;
            if (bid.api) bid_obj["api"] = *bid.api;
            if (bid.protocol) bid_obj["protocol"] = *bid.protocol;
            if (bid.qagmediarating) bid_obj["qagmediarating"] = *bid.qagmediarating;
            if (bid.language) bid_obj["language"] = *bid.language;
            if (bid.bundle) bid_obj["bundle"] = *bid.bundle;
            if (bid.ext) {
                // 尝试解析 ext 字段 (可能是 JSON 字符串)
                try {
                    bid_obj["ext"] = json::parse(*bid.ext);
                } catch (...) {
                    bid_obj["ext"] = *bid.ext;
                }
            }

            bid_array.push_back(bid_obj);
        }

        seatbid_obj["bid"] = bid_array;

        // 推荐字段
        if (seatbid.seat) seatbid_obj["seat"] = *seatbid.seat;
        if (seatbid.group) seatbid_obj["group"] = *seatbid.group;
        if (seatbid.ext) seatbid_obj["ext"] = *seatbid.ext;

        seatbid_array.push_back(seatbid_obj);
    }

    j["seatbid"] = seatbid_array;

    // 推荐字段
    if (response.bidid) j["bidid"] = *response.bidid;
    if (response.cur) j["cur"] = *response.cur;

    // 可选字段
    if (response.nbr) j["nbr"] = *response.nbr;
    if (response.ext) j["ext"] = *response.ext;

    // 序列化为字符串
    if (pretty) {
        return j.dump(2);  // 缩进 2 个空格
    } else {
        return j.dump();
    }
}

bool ResponseSerializer::save_to_file(
    const BidResponse& response,
    const std::string& file_path,
    bool pretty
) {
    try {
        std::string json_str = serialize(response, pretty);

        std::ofstream file(file_path);
        if (!file.is_open()) {
            return false;
        }

        file << json_str;
        file.close();

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace flow_ad::openrtb
```

**预计工作量**: 2-3 小时

---

### Phase 5: 单元测试实现 (3-4 小时)

**文件**: `tests/unit/openrtb_response_test.cpp`

**测试用例**:

```cpp
#include <gtest/gtest.h>
#include "core/openrtb/openrtb_response_builder.h"
#include "core/openrtb/response_serializer.h"
#include "core/openrtb/openrtb_parser.h"
#include "core/ranker/ranker.h"

using namespace flow_ad;

class OpenRTBResponseTest : public ::testing::Test {
protected:
    openrtb::ResponseBuilder builder_;
    openrtb::AuctionDecision auction_;
    openrtb::OpenRTBParser parser_;

    // 创建测试用的 OpenRTB Request
    openrtb::OpenRTBRequest create_test_request() {
        openrtb::OpenRTBRequest request;
        request.id = "test-request-001";

        openrtb::Impression imp;
        imp.id = "imp-001";
        imp.bidfloor = 10.0;  // 底价 10 元

        request.imp.push_back(imp);

        return request;
    }

    // 创建测试用的候选广告列表
    std::vector<ranker::AdCandidate> create_test_candidates() {
        return {
            {1, "creative_1", 100, 1, 50.0, 0.02},  // eCPM = 1000
            {2, "creative_2", 101, 2, 80.0, 0.01},  // eCPM = 800
            {3, "creative_3", 102, 1, 60.0, 0.015}  // eCPM = 900
        };
    }
};

// ========== First Price 拍卖测试 ==========

TEST_F(OpenRTBResponseTest, FirstPriceAuction) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    // 设置 First Price 拍卖
    auction_.set_auction_type(openrtb::AuctionType::FIRST_PRICE);

    // 执行竞价决策
    auto result = auction_.decide(candidates, request);

    // 验证结果
    EXPECT_TRUE(result.has_winner);
    EXPECT_TRUE(result.winner.has_value());
    EXPECT_EQ(result.winner->ad_id, 1);  // eCPM 最高
    EXPECT_EQ(result.auction_type, openrtb::AuctionType::FIRST_PRICE);

    // First Price: 成交价 = 出价 = 50 元 = 50000 微美元
    EXPECT_DOUBLE_EQ(result.final_price, 50000.0);
}

// ========== Second Price 拍卖测试 ==========

TEST_F(OpenRTBResponseTest, SecondPriceAuction) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    // 设置 Second Price 拍卖
    auction_.set_auction_type(openrtb::AuctionType::SECOND_PRICE);

    // 执行竞价决策
    auto result = auction_.decide(candidates, request);

    // 验证结果
    EXPECT_TRUE(result.has_winner);
    EXPECT_EQ(result.winner->ad_id, 1);  // eCPM 最高

    // Second Price: 成交价 = 第二高价 + 0.01 = 60.0 + 0.01 = 60.01 元 = 60010 微美元
    EXPECT_DOUBLE_EQ(result.final_price, 60010.0);
}

// ========== No Bid 测试 ==========

TEST_F(OpenRTBResponseTest, NoBid) {
    auto request = create_test_request();
    std::vector<ranker::AdCandidate> empty_candidates;

    // 执行竞价决策
    auto result = auction_.decide(empty_candidates, request);

    // 验证结果
    EXPECT_FALSE(result.has_winner);
    EXPECT_FALSE(result.winner.has_value());
    EXPECT_EQ(result.reason, "No candidates available");
}

// ========== Response 构建测试 ==========

TEST_F(OpenRTBResponseTest, BuildResponse) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    // 执行竞价决策
    auto auction_result = auction_.decide(candidates, request);

    // 构建 Response
    auto response = builder_.build(auction_result, request);

    // 验证 Response
    EXPECT_EQ(response.id, request.id);
    EXPECT_TRUE(response.bidid.has_value());
    EXPECT_TRUE(response.has_bid());

    EXPECT_EQ(response.seatbid.size(), 1);
    EXPECT_EQ(response.seatbid[0].bid.size(), 1);

    const auto& bid = response.seatbid[0].bid[0];
    EXPECT_EQ(bid.impid, "imp-001");
    EXPECT_GT(bid.price, 0);  // 成交价应该大于 0
}

// ========== JSON 序列化测试 ==========

TEST_F(OpenRTBResponseTest, SerializeResponse) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    auto auction_result = auction_.decide(candidates, request);
    auto response = builder_.build(auction_result, request);

    // 序列化为 JSON
    std::string json_str = openrtb::ResponseSerializer::serialize(response);

    // 验证 JSON 字符串
    EXPECT_FALSE(json_str.empty());
    EXPECT_NE(json_str.find("\"id\":"), std::string::npos);
    EXPECT_NE(json_str.find("\"seatbid\":"), std::string::npos);

    // 尝试解析 JSON (验证格式正确)
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        EXPECT_TRUE(j.contains("id"));
        EXPECT_TRUE(j.contains("seatbid"));
    } catch (...) {
        FAIL() << "Failed to parse JSON";
    }
}

// ========== 调试扩展字段测试 ==========

TEST_F(OpenRTBResponseTest, DebugExt) {
    openrtb::ResponseBuilderConfig config;
    config.include_debug_ext = true;

    openrtb::ResponseBuilder builder(config);

    auto request = create_test_request();
    auto candidates = create_test_candidates();

    auto auction_result = auction_.decide(candidates, request);
    auto response = builder.build(auction_result, request);

    // 序列化并检查 ext 字段
    std::string json_str = openrtb::ResponseSerializer::serialize(response);

    EXPECT_NE(json_str.find("\"ext\":"), std::string::npos);
    EXPECT_NE(json_str.find("\"predicted_ctr\":"), std::string::npos);
    EXPECT_NE(json_str.find("\"ecpm\":"), std::string::npos);
}
```

**预计工作量**: 3-4 小时

---

## 🔧 CMakeLists.txt 配置

更新 `core/openrtb/CMakeLists.txt`:

```cmake
# 添加新文件
add_library(openrtb_response STATIC
    openrtb_response_builder.cpp
    auction_decision.cpp
)

target_include_directories(openrtb_response
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core/openrtb
        ${CMAKE_SOURCE_DIR}/core/ranker
)

target_link_libraries(openrtb_response
    PUBLIC
        openrtb_parser
        ad_ranker
)
```

---

## ✅ 验收标准

### 功能验收
- [ ] 能够构建符合 OpenRTB 2.5 规范的 Bid Response
- [ ] 支持 First Price 和 Second Price 拍卖
- [ ] 正确计算成交价(微美元单位)
- [ ] 支持无竞价响应(No Bid)
- [ ] JSON 序列化格式正确

### 质量验收
- [ ] 单元测试覆盖率 > 90%
- [ ] 无内存泄漏(Valgrind 检查)
- [ ] 代码符合项目风格
- [ ] 所有公共 API 有文档注释

### 性能验收
- [ ] 响应构建延迟 P99 < 5ms
- [ ] JSON 序列化延迟 P99 < 3ms
- [ ] 内存占用 < 10MB

---

## 🚀 使用示例

### 完整流程示例

```cpp
#include "core/openrtb/openrtb_parser.h"
#include "core/openrtb/openrtb_response_builder.h"
#include "core/indexing/ad_index.h"
#include "core/ranker/ranker.h"
#include "core/ctr/ctr_manager.h"

using namespace flow_ad;

int main() {
    // 1. 解析 OpenRTB Request
    openrtb::OpenRTBParser parser;
    auto parse_result = parser.parse_file("data/data/openrtb_requests.json");

    if (!parse_result.success) {
        std::cerr << "Failed to parse request" << std::endl;
        return 1;
    }

    // 2. 广告召回
    indexing::AdIndex index;
    index.load_from_file("data/data/ads_data.json");

    auto recall_result = index.query(parse_result.request);

    // 3. CTR 预估
    ctr::CTRManager ctr_manager;
    ctr_manager.load_model("models/deep_fm.onnx");

    for (auto& candidate : recall_result.candidates) {
        auto ctr_result = ctr_manager.predict(candidate.ad_id, /* user_id */);
        if (ctr_result.success) {
            candidate.predicted_ctr = ctr_result.ctr;
            candidate.recalculate_ecpm();
        }
    }

    // 4. ECPM 排序
    ranker::Ranker ranker(ranker::SortStrategy::GREEDY);
    auto sorted_ads = ranker.rank(recall_result.candidates);

    // 5. 竞价决策
    openrtb::AuctionDecision auction(openrtb::AuctionType::SECOND_PRICE);
    auto auction_result = auction.decide(sorted_ads, parse_result.request);

    // 6. 构建 Response
    openrtb::ResponseBuilder builder;
    auto response = builder.build(auction_result, parse_result.request);

    // 7. 序列化为 JSON
    std::string json_response = openrtb::ResponseSerializer::serialize(response);

    // 8. 返回响应
    std::cout << "HTTP/1.1 200 OK" << std::endl;
    std::cout << "Content-Type: application/json" << std::endl;
    std::cout << "Content-Length: " << json_response.length() << std::endl;
    std::cout << std::endl;
    std::cout << json_response << std::endl;

    return 0;
}
```

---

## 🎯 下一步

完成 Step 5 后,系统将具备:
- ✅ 大规模广告池管理能力(Step 1)
- ✅ OpenRTB 2.5 协议解析能力(Step 2)
- ✅ 快速召回能力(Step 3)
- ✅ CTR 预估能力(Step 6)
- ✅ ECPM 排序能力(Step 4)
- ✅ **竞价决策 & Response 构建能力**(Step 5) ⭐ 本步骤目标

**系统完成**: 此时已经具备完整的广告投放能力,可以与 DSP/SSP 对接!

---

## 📚 相关文档

- [项目主 README](../../README.md)
- [Step 1: 数据生成](./step1-generateData.md)
- [Step 2: OpenRTB 解析](./step2-parseOpenRTB.md)
- [Step 3: 广告索引](./step3-adIndexing.md)
- [Step 4: ECPM 排序](./step4-ecpm-ranking-production.md)
- [Step 6: CTR 预估](./step6-ctr-estimation-simple.md)
- [架构设计文档](./schema.md)

---

**文档创建时间**: 2026-03-13
**维护者**: airprofly
**状态**: ✅ 规划完成,待实现
