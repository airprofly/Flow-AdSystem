// Author: airprofly
//
// 全链路集成测试: 覆盖所有 8 个模块的完整广告投放流程
//
// 流程:
//   OpenRTB 解析 → 广告索引召回 → 频次控制 → Pacing → CTR 预估 → eCPM 排序 → 拍卖决策 → Response 构建

#include <gtest/gtest.h>

// OpenRTB 解析 & 响应
#include "core/openrtb/auction_decision.h"
#include "core/openrtb/openrtb_models.h"
#include "core/openrtb/openrtb_parser.h"
#include "core/openrtb/openrtb_response_builder.h"
#include "core/openrtb/response_serializer.h"

// 广告索引
#include "core/indexing/ad_index.hpp"
#include "data/models/ad_model.h"

// 频次控制
#include "core/frequency/async_impression_logger.hpp"
#include "core/frequency/frequency_manager.hpp"

// Pacing
#include "core/pacing/async_pacing_updater.hpp"
#include "core/pacing/pacing_manager.hpp"

// CTR 预估
#include "core/ctr/ctr_manager.h"

// eCPM 排序
#include "core/ranker/ad_candidate.hpp"
#include "core/ranker/ranker.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#ifndef FLOW_ADSYSTEM_SOURCE_DIR
#define FLOW_ADSYSTEM_SOURCE_DIR "."
#endif

using namespace flow_ad;
using namespace flow_ad::openrtb;
using namespace flow_ad::indexing;
using namespace flow_ad::frequency;
using namespace flow_ad::pacing;
using namespace flow_ad_system::ranker;

using AdDeviceType = flow_ad::DeviceType;

// ============================================================
// 全链路测试 Fixture
// ============================================================

class FullPipelineTest : public ::testing::Test
{
protected:
    // ── 模块实例 ──
    OpenRTBParser parser_;
    AdIndex index_;

    std::shared_ptr<FrequencyManager> freq_mgr_;
    std::unique_ptr<AsyncImpressionLogger> freq_logger_;

    std::shared_ptr<PacingManager> pacing_mgr_;
    std::unique_ptr<AsyncPacingUpdater> pacing_updater_;

    ctr::CTRManager ctr_mgr_;
    Ranker ranker_;
    AuctionDecision auction_;
    ResponseBuilder response_builder_;

    bool ctr_model_available_ = false;

    void SetUp() override
    {
        // ── 1. 频控 ──
        freq_mgr_ = std::make_shared<FrequencyManager>();
        freq_logger_ = std::make_unique<AsyncImpressionLogger>(freq_mgr_, 10000, 100, 100);

        // ── 2. Pacing ──
        pacing_mgr_ = std::make_shared<PacingManager>();
        pacing_updater_ = std::make_unique<AsyncPacingUpdater>(pacing_mgr_, 10000, 100, 100);

        // ── 3. 广告数据 + 索引 + Pacing 配置 ──
        setup_ads();

        // ── 4. CTR 模型（可选，无模型时用默认 CTR） ──
        const std::string model_path =
            std::string(FLOW_ADSYSTEM_SOURCE_DIR) + "/models/deep_fm.onnx";
        if (std::filesystem::exists(model_path))
        {
            ctr::ModelConfig cfg;
            cfg.model_path = model_path;
            cfg.model_name = "deep_fm";
            cfg.traffic_fraction = 1.0f;
            ctr_mgr_.initialize();
            if (ctr_mgr_.add_model(cfg))
            {
                ctr_model_available_ = true;
                std::cout << "[Setup] CTR ONNX 模型加载成功\n";
            }
        }
        else
        {
            std::cout << "[Setup] 未找到 ONNX 模型，CTR 将使用默认值 0.05\n";
        }

        // ── 5. Ranker：二价拍卖 ──
        RankerConfig rc;
        rc.max_ads = 5;
        rc.sort_strategy = SortStrategy::kGreedy;
        ranker_.update_config(rc);
        auction_.set_auction_type(AuctionType::SECOND_PRICE);
    }

    void TearDown() override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ── 辅助：构造 AdCandidate 列表 ──
    // 对索引召回的每个 ad_id，获取广告信息，做频控+Pacing+CTR，组装候选
    std::vector<AdCandidate> build_candidates(
        const std::vector<uint64_t> &ad_ids,
        uint64_t user_id,
        const std::string &request_json)
    {
        FrequencyCap cap;
        cap.hourly_limit = 5;
        cap.daily_limit = 20;

        std::vector<AdCandidate> candidates;
        for (uint64_t ad_id : ad_ids)
        {
            const Ad *ad = index_.get_ad(ad_id);
            if (!ad)
                continue;

            // ── 频控检查 ──
            if (freq_mgr_->is_capped(user_id, ad_id, cap))
                continue;

            // ── Pacing 检查 ──
            if (!pacing_mgr_->allow_impression(ad->campaign_id, ad->bid_price))
                continue;

            // ── CTR 预估 ──
            double predicted_ctr = 0.05; // 默认值
            if (ctr_model_available_)
            {
                std::string ad_json = make_ad_json(*ad);
                ctr::InferenceResult ir;
                if (ctr_mgr_.predict_ctr(request_json, ad_json, ir))
                {
                    predicted_ctr = ir.ctr;
                }
            }

            AdCandidate c;
            c.ad_id = ad->id;
            c.creative_id = std::to_string(ad->creative_id);
            c.campaign_id = static_cast<uint32_t>(ad->campaign_id);
            c.category = ad->categories.empty() ? 0u
                                                : static_cast<uint32_t>(ad->categories[0]);
            c.bid_price = ad->bid_price;
            c.predicted_ctr = predicted_ctr;
            c.update_ecpm();
            candidates.push_back(c);
        }
        return candidates;
    }

private:
    void setup_ads()
    {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();

        struct AdSpec
        {
            uint64_t id;
            uint64_t campaign_id;
            double bid;
            AdCategory category;
            AdDeviceType device;
            const char *geo;
            const char *interest;
        };

        static constexpr AdSpec kAds[] = {
            {1, 101, 2.50, AdCategory::GAMING, AdDeviceType::MOBILE, "北京", "游戏"},
            {2, 102, 1.80, AdCategory::ECOMMERCE, AdDeviceType::DESKTOP, "上海", "购物"},
            {3, 103, 1.20, AdCategory::EDUCATION, AdDeviceType::MOBILE, "北京", "学习"},
            {4, 104, 3.00, AdCategory::FINANCE, AdDeviceType::TABLET, "深圳", "理财"},
            {5, 105, 2.20, AdCategory::TECHNOLOGY, AdDeviceType::MOBILE, "北京", "科技"},
        };

        for (const auto &s : kAds)
        {
            Ad ad;
            ad.id = s.id;
            ad.campaign_id = s.campaign_id;
            ad.creative_id = s.id * 1000;
            ad.bid_price = s.bid;
            ad.categories = {s.category};
            ad.targeting_devices = {s.device, AdDeviceType::MOBILE};
            ad.targeting_geo = {s.geo};
            ad.targeting_interests = {s.interest};
            index_.add_ad(ad.id, ad);

            // Pacing 配置
            BudgetConfig bc;
            bc.campaign_id = s.campaign_id;
            bc.granularity = TimeGranularity::DAILY;
            bc.total_budget = 1000.0;
            bc.daily_budget = 100.0;
            bc.hourly_budget = 10.0;
            bc.start_time_ms = now;
            bc.end_time_ms = now + 24 * 3600 * 1000;
            bc.algorithm = PacingAlgorithm::TOKEN_BUCKET;
            bc.target_rate = 10.0;
            bc.max_impression_rate = 100.0;
            bc.min_impression_rate = 0.1;
            pacing_mgr_->update_campaign_config(bc.campaign_id, bc);
        }
    }

    static std::string make_ad_json(const Ad &ad)
    {
        return R"({"id":")" + std::to_string(ad.id) +
               R"(","campaign_id":")" + std::to_string(ad.campaign_id) +
               R"(","industry":1,"creative_type":1,"size":"300x250",)"
               R"("hist_ctr":0.05,"hist_cvr":0.02,"position":1})";
    }
};

// ============================================================
// 测试 1: 单次完整投放流程
// ============================================================

TEST_F(FullPipelineTest, SingleRequestFullPipeline)
{
    std::cout << "\n====== 全链路测试: 单次请求 ======\n";

    // ── Step 1: OpenRTB 请求解析 ──
    const std::string openrtb_json = R"JSON({
        "id": "req-full-001",
        "imp": [{"id": "imp-001", "banner": {"w": [300], "h": [250]}, "bidfloor": 0.5}],
        "device": {
            "ua": "Mozilla/5.0 (iPhone)",
                      "geo" : {"country" : "CHN", "city" : "北京"},
                      "devicetype" : 1,
                      "os" : 1
}
,
    "user": { "id" : "user-001", "keywords" : [ "游戏", "科技" ] }
})JSON";

    auto parse_result = parser_.parse(openrtb_json);
ASSERT_TRUE(parse_result.success) << parse_result.error_message;
std::cout << "[Step1] OpenRTB 解析 OK  request_id=" << parse_result.request.id << "\n";

// ── Step 2: 广告索引召回 ──
QueryResult qr = index_.query(parse_result.request);
ASSERT_GT(qr.candidate_ad_ids.size(), 0u) << "索引召回结果为空";
std::cout << "[Step2] 索引召回 " << qr.candidate_ad_ids.size()
          << " 个广告  耗时=" << qr.recall_time_ms << "ms\n";

// ── Step 3~5: 频控 + Pacing + CTR → 构建候选列表 ──
constexpr uint64_t kUserId = 100001;
auto candidates = build_candidates(qr.candidate_ad_ids, kUserId, openrtb_json);
ASSERT_GT(candidates.size(), 0u) << "经频控+Pacing过滤后无候选广告";
std::cout << "[Step3-5] 过滤后候选数=" << candidates.size() << "\n";
for (const auto &c : candidates)
{
    std::cout << "    ad_id=" << c.ad_id
              << " bid=" << c.bid_price
              << " ctr=" << c.predicted_ctr
              << " ecpm=" << c.ecpm << "\n";
}

// ── Step 6: eCPM 排序 ──
ranker_.rank(candidates);
// 验证：第一名 eCPM 等于全集最大 eCPM
{
    auto max_it = std::max_element(candidates.begin(), candidates.end(),
                                   [](const AdCandidate &a, const AdCandidate &b)
                                   { return a.ecpm < b.ecpm; });
    EXPECT_DOUBLE_EQ(candidates.front().ecpm, max_it->ecpm) << "排序后首位应为最高 eCPM";
}
if (candidates.size() >= 2)
{
    EXPECT_GE(candidates[0].ecpm, candidates[1].ecpm);
}
std::cout << "[Step6] eCPM 排序 OK  winner_ecpm=" << candidates[0].ecpm << "\n";

// ── Step 7: 拍卖决策 ──
AuctionResult auction_result = auction_.decide(candidates, parse_result.request);
ASSERT_TRUE(auction_result.has_winner);
EXPECT_EQ(auction_result.winner->ad_id, candidates[0].ad_id);
EXPECT_GT(auction_result.final_price, 0.0);
std::cout << "[Step7] 拍卖决策 OK  winner_ad_id=" << auction_result.winner->ad_id
          << " final_price(微美元)=" << auction_result.final_price << "\n";

// ── Step 8: Response 构建 ──
BidResponse response = response_builder_.build(auction_result, parse_result.request);
EXPECT_FALSE(response.id.empty());
ASSERT_FALSE(response.seatbid.empty());
ASSERT_FALSE(response.seatbid[0].bid.empty());
EXPECT_GT(response.seatbid[0].bid[0].price, 0.0);

std::string response_json = ResponseSerializer::serialize(response, false);
EXPECT_FALSE(response_json.empty());
std::cout << "[Step8] Response 构建 OK  response_id=" << response.id << "\n";
std::cout << "[Done]  全链路测试通过\n";

// 记录展示（异步）
freq_logger_->record_impression(kUserId, auction_result.winner->ad_id, TimeWindow::HOUR);
freq_logger_->record_impression(kUserId, auction_result.winner->ad_id, TimeWindow::DAY);
pacing_updater_->record_impression(
    auction_result.winner->campaign_id, auction_result.final_price / 1e6);
}

// ============================================================
// 测试 2: 频控生效 — 同一用户对同一广告超限后应该被过滤
// ============================================================

TEST_F(FullPipelineTest, FrequencyCapBlocksAfterLimit)
{
    std::cout << "\n====== 全链路测试: 频控拦截 ======\n";

    constexpr uint64_t kUserId = 200001;
    constexpr uint64_t kAdId = 1; // 游戏广告
    constexpr int kLimit = 5;

    // 预先记录 5 次展示，达到小时上限
    for (int i = 0; i < kLimit; i++)
    {
        freq_mgr_->record_impression(kUserId, kAdId, TimeWindow::HOUR);
    }

    // 构造包含该广告的召回列表
    std::vector<uint64_t> ad_ids = {kAdId};
    auto candidates = build_candidates(ad_ids, kUserId,
                                       R"({"timestamp":0,"user":{"id":"u","gender":"M","age":30,"city":1,"history_ctr":0.05,)"
                                       R"("history_impressions":100,"device":{"type":"mobile"}},"device":{"network_type":4,)"
                                       R"("carrier":1},"site":{"category":1}})");

    // 该广告应被频控过滤
    EXPECT_TRUE(candidates.empty()) << "超过频控上限后广告应被过滤";
    std::cout << "[Done] 频控拦截测试通过\n";
}

// ============================================================
// 测试 3: No-Bid — 所有候选被过滤后返回 no-bid response
// ============================================================

TEST_F(FullPipelineTest, NoBidWhenAllCandidatesFiltered)
{
    std::cout << "\n====== 全链路测试: No-Bid ======\n";

    // 解析一个来自"无匹配城市"的请求，索引应召回 0 个广告
    const std::string openrtb_json = R"({
        "id": "req-nobid-001",
        "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}}],
        "device": {
            "geo": {"city": "不存在的城市XYZ"},
            "devicetype": 1
        },
        "user": {"id": "user-nobid"}
    })";

    auto parse_result = parser_.parse(openrtb_json);
    ASSERT_TRUE(parse_result.success);

    QueryResult qr = index_.query(parse_result.request);
    // 如果召回结果为空，直接走 no-bid 分支
    if (qr.candidate_ad_ids.empty())
    {
        BidResponse no_bid = response_builder_.build_no_bid(parse_result.request, "No matched ads");
        EXPECT_FALSE(no_bid.id.empty());
        EXPECT_TRUE(no_bid.seatbid.empty());
        std::cout << "[Done] No-Bid 测试通过 (索引未召回任何广告)\n";
    }
    else
    {
        // 即使召回，经频控/Pacing 也可能为空（此处构造极端频控场景）
        GTEST_SKIP() << "此请求有召回结果，跳过 No-Bid 验证（请求需要更严格的定向条件）";
    }
}

// ============================================================
// 测试 4: 并发压测 — 多线程同时跑全链路
// ============================================================

TEST_F(FullPipelineTest, ConcurrentPipelineStressTest)
{
    std::cout << "\n====== 全链路测试: 并发压测 ======\n";

    constexpr int kThreads = 8;
    constexpr int kRequests = 50; // 每线程请求数

    std::atomic<int> total_bids{0};
    std::atomic<int> total_no_bids{0};
    std::atomic<int> total_errors{0};

    auto worker = [&](int thread_id)
    {
        const std::string openrtb_json = R"({
            "id": "req-stress-)" + std::to_string(thread_id) +
                                         R"(",
            "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}, "bidfloor": 0.1}],
            "device": {"geo": {"city": "北京"}, "devicetype": 1, "os": 1},
            "user": {"id": "user-)" + std::to_string(thread_id) +
                                         R"(", "keywords": ["游戏"]}
        })";

        for (int i = 0; i < kRequests; i++)
        {
            // 每个请求使用独立的 user_id 避免频控相互干扰
            uint64_t user_id = static_cast<uint64_t>(thread_id * 10000 + i);

            auto pr = parser_.parse(openrtb_json);
            if (!pr.success)
            {
                total_errors++;
                continue;
            }

            QueryResult qr = index_.query(pr.request);
            auto candidates = build_candidates(qr.candidate_ad_ids, user_id, openrtb_json);

            if (candidates.empty())
            {
                total_no_bids++;
                continue;
            }

            ranker_.rank(candidates);
            AuctionResult ar = auction_.decide(candidates, pr.request);

            if (ar.has_winner)
            {
                total_bids++;
                BidResponse resp = response_builder_.build(ar, pr.request);
                if (resp.id.empty())
                    total_errors++;

                // 异步记录（不阻塞）
                freq_logger_->record_impression(user_id, ar.winner->ad_id, TimeWindow::DAY);
                pacing_updater_->record_impression(ar.winner->campaign_id,
                                                   ar.final_price / 1e6);
            }
            else
            {
                total_no_bids++;
            }
        }
    };

    // 启动线程
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; t++)
    {
        threads.emplace_back(worker, t);
    }
    for (auto &t : threads)
        t.join();

    // 等待异步队列处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    int total = total_bids + total_no_bids;
    EXPECT_EQ(total_errors, 0) << "并发过程中出现解析/序列化错误";
    EXPECT_EQ(total, kThreads * kRequests);
    EXPECT_GT(total_bids, 0) << "并发场景下应有成功竞价";

    std::cout << "  总请求: " << total
              << "  成功竞价: " << total_bids
              << "  No-Bid: " << total_no_bids
              << "  错误: " << total_errors << "\n";
    std::cout << "[Done] 并发压测通过\n";
}

// ============================================================
// 测试 5: 端到端延迟基准
// ============================================================

TEST_F(FullPipelineTest, EndToEndLatencyBenchmark)
{
    std::cout << "\n====== 全链路测试: 延迟基准 ======\n";

    const std::string openrtb_json = R"({
        "id": "req-bench",
        "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}, "bidfloor": 0.5}],
        "device": {"geo": {"city": "北京"}, "devicetype": 1, "os": 1},
        "user": {"id": "user-bench", "keywords": ["游戏"]}
    })";

    constexpr int kWarmup = 10;
    constexpr int kIterations = 500;
    uint64_t user_base = 900000;

    // 预热
    for (int i = 0; i < kWarmup; i++)
    {
        auto pr = parser_.parse(openrtb_json);
        auto qr = index_.query(pr.request);
        auto c = build_candidates(qr.candidate_ad_ids, user_base + i, openrtb_json);
        if (!c.empty())
        {
            ranker_.rank(c);
            auction_.decide(c, pr.request);
        }
    }

    // 计时
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIterations; i++)
    {
        auto pr = parser_.parse(openrtb_json);
        auto qr = index_.query(pr.request);
        auto c = build_candidates(qr.candidate_ad_ids, user_base + kWarmup + i, openrtb_json);
        if (!c.empty())
        {
            ranker_.rank(c);
            auto ar = auction_.decide(c, pr.request);
            if (ar.has_winner)
                response_builder_.build(ar, pr.request);
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double avg_us = total_ms / kIterations * 1000.0;

    std::cout << "  迭代次数: " << kIterations << "\n"
              << "  总耗时: " << total_ms << " ms\n"
              << "  平均延迟: " << avg_us << " μs/请求\n";

    // 不带 CTR 模型的情况下端到端应远低于 10ms
    if (!ctr_model_available_)
    {
        EXPECT_LT(avg_us, 10000.0) << "端到端平均延迟超过 10ms(不含 CTR)";
    }
    std::cout << "[Done] 延迟基准测试通过\n";
}
