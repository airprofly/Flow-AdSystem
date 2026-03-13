// Author: airprofly
//
// OpenRTB Response 模块单元测试

#include <gtest/gtest.h>
#include "core/openrtb/openrtb_response_builder.h"
#include "core/openrtb/response_serializer.h"
#include "core/openrtb/openrtb_parser.h"
#include "../../core/ranker/ad_candidate.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

using namespace flow_ad;
using json = nlohmann::json;

class OpenRTBResponseTest : public ::testing::Test {
protected:
    openrtb::ResponseBuilder builder_;
    openrtb::AuctionDecision auction_;
    openrtb::AuctionDecision first_price_auction_;

    // 创建测试用的 OpenRTB Request
    openrtb::OpenRTBRequest create_test_request(const std::string& request_id = "test-request-001") {
        openrtb::OpenRTBRequest request;
        request.id = request_id;

        openrtb::Impression imp;
        imp.id = "imp-001";
        imp.bidfloor = 10.0;  // 底价 10 元

        openrtb::Banner banner;
        banner.w = std::vector<int>{300};
        banner.h = std::vector<int>{250};
        imp.banner = banner;

        request.imp.push_back(imp);

        return request;
    }

    // 创建测试用的候选广告列表
    std::vector<flow_ad_system::ranker::AdCandidate> create_test_candidates() {
        return {
            {1, "creative_1", 100, 1, 50.0, 0.02, 1000.0},  // eCPM = 1000
            {2, "creative_2", 101, 2, 80.0, 0.01, 800.0},  // eCPM = 800
            {3, "creative_3", 102, 1, 60.0, 0.015, 900.0}  // eCPM = 900
        };
    }

    void SetUp() override {
        // 设置 First Price 拍卖
        first_price_auction_.set_auction_type(openrtb::AuctionType::FIRST_PRICE);
    }
};

// ========== First Price 拍卖测试 ==========

TEST_F(OpenRTBResponseTest, FirstPriceAuction_WithValidCandidates) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    // 执行竞价决策
    auto result = first_price_auction_.decide(candidates, request);

    // 验证结果
    EXPECT_TRUE(result.has_winner);
    EXPECT_TRUE(result.winner.has_value());
    EXPECT_EQ(result.winner->ad_id, 1);  // eCPM 最高
    EXPECT_EQ(result.auction_type, openrtb::AuctionType::FIRST_PRICE);

    // First Price: 成交价 = 出价 = 50 元 = 50000 微美元
    EXPECT_DOUBLE_EQ(result.final_price, 50000.0);
}

TEST_F(OpenRTBResponseTest, FirstPriceAuction_CalculatePriceCorrectly) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    auto result = first_price_auction_.decide(candidates, request);

    // 验证价格计算
    EXPECT_DOUBLE_EQ(result.get_price_cpm(), 50.0);     // 元/千次展示
    EXPECT_DOUBLE_EQ(result.get_price_yuan(), 0.05);    // 元
}

// ========== Second Price 拍卖测试 ==========

TEST_F(OpenRTBResponseTest, SecondPriceAuction_WithMultipleCandidates) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    // 执行竞价决策
    auto result = auction_.decide(candidates, request);

    // 验证结果
    EXPECT_TRUE(result.has_winner);
    EXPECT_EQ(result.winner->ad_id, 1);  // eCPM 最高

    // 候选列表按 eCPM 排序:
    // - [0] eCPM=1000, bid=50.0 (赢家)
    // - [1] eCPM=800, bid=80.0 (第二,按 eCPM 排序)
    // - [2] eCPM=900, bid=60.0
    // Second Price: 成交价 = 第二高 bid_price + 0.01 = 80.0 + 0.01 = 80.01 元 = 80010 微美元
    EXPECT_DOUBLE_EQ(result.final_price, 80010.0);
}

TEST_F(OpenRTBResponseTest, SecondPriceAuction_WithFloorPrice) {
    auto request = create_test_request();

    // 创建第二高价远低于赢家出价的场景 (按 eCPM 排序)
    std::vector<flow_ad_system::ranker::AdCandidate> candidates = {
        {1, "creative_1", 100, 1, 100.0, 0.02, 2000.0},  // 赢家,eCPM=2000,出价 100
        {2, "creative_2", 101, 2, 10.0, 0.01, 100.0}     // 第二名,eCPM=100,出价 10
    };

    auto result = auction_.decide(candidates, request);

    // 验证 floor price: max(10.01, 100 * 0.9) = max(10.01, 90) = 90
    EXPECT_DOUBLE_EQ(result.final_price, 90000.0);  // 90 元 = 90000 微美元
}

TEST_F(OpenRTBResponseTest, SecondPriceAuction_WithSingleCandidate) {
    auto request = create_test_request();
    
    std::vector<flow_ad_system::ranker::AdCandidate> candidates = {
        {1, "creative_1", 100, 1, 50.0, 0.02, 1000.0}
    };

    auto result = auction_.decide(candidates, request);

    // 只有一个候选,使用 First Price
    EXPECT_DOUBLE_EQ(result.final_price, 50000.0);
}

// ========== No Bid 测试 ==========

TEST_F(OpenRTBResponseTest, NoBid_EmptyCandidates) {
    auto request = create_test_request();
    std::vector<flow_ad_system::ranker::AdCandidate> empty_candidates;

    // 执行竞价决策
    auto result = auction_.decide(empty_candidates, request);

    // 验证结果
    EXPECT_FALSE(result.has_winner);
    EXPECT_FALSE(result.winner.has_value());
    EXPECT_EQ(result.reason, "No valid candidates");
}

TEST_F(OpenRTBResponseTest, NoBid_BidFloorNotMet) {
    auto request = create_test_request();
    request.imp[0].bidfloor = 100.0;  // 设置高底价

    auto candidates = create_test_candidates();

    auto result = auction_.decide(candidates, request);

    // 验证结果
    EXPECT_FALSE(result.has_winner);
    EXPECT_EQ(result.reason, "Bid floor not met");
}

TEST_F(OpenRTBResponseTest, NoBid_BudgetExceeded) {
    auto request = create_test_request();

    // 创建剩余预算为 0 的候选广告
    flow_ad_system::ranker::AdCandidate candidate;
    candidate.ad_id = 1;
    candidate.creative_id = "creative_1";
    candidate.campaign_id = 100;
    candidate.category = 1;
    candidate.bid_price = 50.0;
    candidate.predicted_ctr = 0.02;
    candidate.ecpm = 1000.0;
    candidate.remaining_budget = 0.0;  // 剩余预算为 0

    std::vector<flow_ad_system::ranker::AdCandidate> candidates = {candidate};

    auto result = auction_.decide(candidates, request);

    // 验证结果
    EXPECT_FALSE(result.has_winner);
    EXPECT_EQ(result.reason, "Budget exceeded");
}

// ========== Response 构建测试 ==========

TEST_F(OpenRTBResponseTest, BuildResponse_WithWinner) {
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

TEST_F(OpenRTBResponseTest, BuildResponse_NoBid) {
    auto request = create_test_request();
    std::vector<flow_ad_system::ranker::AdCandidate> empty_candidates;

    auto auction_result = auction_.decide(empty_candidates, request);
    auto response = builder_.build(auction_result, request);

    // 验证 No Bid Response
    EXPECT_TRUE(response.is_no_bid());
    EXPECT_TRUE(response.nbr.has_value());
    EXPECT_FALSE(response.has_bid());
}

TEST_F(OpenRTBResponseTest, BuildResponse_CheckBidFields) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    auto auction_result = auction_.decide(candidates, request);
    auto response = builder_.build(auction_result, request);

    const auto& bid = response.seatbid[0].bid[0];

    // 验证必填字段
    EXPECT_FALSE(bid.id.empty());
    EXPECT_EQ(bid.impid, "imp-001");
    EXPECT_GT(bid.price, 0);

    // 验证推荐字段
    EXPECT_TRUE(bid.adid.has_value());
    EXPECT_TRUE(bid.nurl.has_value());
    EXPECT_TRUE(bid.adm.has_value());
}

// ========== JSON 序列化测试 ==========

TEST_F(OpenRTBResponseTest, SerializeResponse_ValidJson) {
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
        json j = json::parse(json_str);
        EXPECT_TRUE(j.contains("id"));
        EXPECT_TRUE(j.contains("seatbid"));
    } catch (...) {
        FAIL() << "Failed to parse JSON";
    }
}

TEST_F(OpenRTBResponseTest, SerializeResponse_PrettyFormat) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    auto auction_result = auction_.decide(candidates, request);
    auto response = builder_.build(auction_result, request);

    // 序列化为格式化 JSON
    std::string json_str = openrtb::ResponseSerializer::serialize(response, true);

    // 验证包含缩进
    EXPECT_NE(json_str.find("\n"), std::string::npos);
    EXPECT_NE(json_str.find("  "), std::string::npos);
}

TEST_F(OpenRTBResponseTest, SerializeResponse_CheckStructure) {
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    auto auction_result = auction_.decide(candidates, request);
    auto response = builder_.build(auction_result, request);

    std::string json_str = openrtb::ResponseSerializer::serialize(response);
    json j = json::parse(json_str);

    // 验证结构
    EXPECT_TRUE(j.contains("id"));
    EXPECT_TRUE(j.contains("seatbid"));
    EXPECT_TRUE(j["seatbid"].is_array());
    EXPECT_GT(j["seatbid"].size(), 0);

    auto& seatbid = j["seatbid"][0];
    EXPECT_TRUE(seatbid.contains("bid"));
    EXPECT_TRUE(seatbid["bid"].is_array());
    EXPECT_GT(seatbid["bid"].size(), 0);

    auto& bid = seatbid["bid"][0];
    EXPECT_TRUE(bid.contains("id"));
    EXPECT_TRUE(bid.contains("impid"));
    EXPECT_TRUE(bid.contains("price"));
}

// ========== 调试扩展字段测试 ==========

TEST_F(OpenRTBResponseTest, DebugExt_IncludedWhenEnabled) {
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

TEST_F(OpenRTBResponseTest, DebugExt_NotIncludedWhenDisabled) {
    openrtb::ResponseBuilderConfig config;
    config.include_debug_ext = false;  // 默认关闭

    openrtb::ResponseBuilder builder(config);

    auto request = create_test_request();
    auto candidates = create_test_candidates();

    auto auction_result = auction_.decide(candidates, request);
    auto response = builder.build(auction_result, request);

    // 序列化并检查 ext 字段不存在
    std::string json_str = openrtb::ResponseSerializer::serialize(response);
    
    // bid 对象中不应该有 ext 字段
    EXPECT_EQ(json_str.find("\"predicted_ctr\":"), std::string::npos);
}

// ========== 端到端测试 ==========

TEST_F(OpenRTBResponseTest, EndToEnd_CompleteFlow) {
    // 1. 创建请求和候选
    auto request = create_test_request();
    auto candidates = create_test_candidates();

    // 2. 竞价决策
    auto auction_result = auction_.decide(candidates, request);
    ASSERT_TRUE(auction_result.has_winner);

    // 3. 构建 Response
    auto response = builder_.build(auction_result, request);
    ASSERT_TRUE(response.has_bid());

    // 4. 序列化
    std::string json_str = openrtb::ResponseSerializer::serialize(response, true);
    ASSERT_FALSE(json_str.empty());

    // 5. 验证完整流程
    json j = json::parse(json_str);
    EXPECT_EQ(j["id"], request.id);
    EXPECT_GT(j["seatbid"][0]["bid"][0]["price"], 0);
}

// ========== 边界情况测试 ==========

TEST_F(OpenRTBResponseTest, EdgeCase_ZeroBidPrice) {
    auto request = create_test_request();
    
    std::vector<flow_ad_system::ranker::AdCandidate> candidates = {
        {1, "creative_1", 100, 1, 0.0, 0.0}  // 出价为 0,无效
    };

    auto result = auction_.decide(candidates, request);
    EXPECT_FALSE(result.has_winner);
}

TEST_F(OpenRTBResponseTest, EdgeCase_VeryHighBidPrice) {
    auto request = create_test_request();
    
    std::vector<flow_ad_system::ranker::AdCandidate> candidates = {
        {1, "creative_1", 100, 1, 10000.0, 0.02, 200000.0}  // 出价 10000 元
    };

    auto result = auction_.decide(candidates, request);
    EXPECT_TRUE(result.has_winner);
    EXPECT_DOUBLE_EQ(result.final_price, 10000000.0);  // 10000 元 = 10M 微美元
}

TEST_F(OpenRTBResponseTest, EdgeCase_MultipleImpressions) {
    auto request = create_test_request();
    
    // 添加多个广告位
    openrtb::Impression imp2;
    imp2.id = "imp-002";
    imp2.bidfloor = 5.0;
    request.imp.push_back(imp2);

    auto candidates = create_test_candidates();

    auto auction_result = auction_.decide(candidates, request);
    auto response = builder_.build(auction_result, request);

    // 验证为每个广告位都生成了 Bid
    EXPECT_EQ(response.seatbid[0].bid.size(), 2);
    EXPECT_EQ(response.seatbid[0].bid[0].impid, "imp-001");
    EXPECT_EQ(response.seatbid[0].bid[1].impid, "imp-002");
}

TEST_F(OpenRTBResponseTest, Config_CustomDomains) {
    openrtb::ResponseBuilderConfig config;
    config.win_notice_domain = "https://custom-win.example.com";
    config.ad_markup_domain = "https://custom-cdn.example.com";
    config.click_tracker_domain = "https://custom-click.example.com";

    openrtb::ResponseBuilder builder(config);

    auto request = create_test_request();
    auto candidates = create_test_candidates();

    auto auction_result = auction_.decide(candidates, request);
    auto response = builder.build(auction_result, request);

    // 验证自定义域名被使用
    const auto& bid = response.seatbid[0].bid[0];
    EXPECT_TRUE(bid.nurl->find("custom-win.example.com") != std::string::npos);
    EXPECT_TRUE(bid.adm->find("custom-cdn.example.com") != std::string::npos);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
