// Author: airprofly

/**
 * @file integration_test_openrtb_indexing.cpp
 * @brief OpenRTB 解析与广告索引联调测试
 *
 * 本测试展示如何将 Step 2（OpenRTB 解析）和 Step 3（广告索引）组合使用：
 * 1. 使用 OpenRTB Parser 解析请求
 * 2. 使用 AdIndex 进行广告召回
 * 3. 完整的广告投放流程演示
 */

#include <gtest/gtest.h>
#include "core/openrtb/openrtb_parser.h"
#include "core/openrtb/openrtb_models.h"
#include "core/indexing/ad_index.hpp"
#include "data/models/ad_model.h"
#include <iostream>
#include <iomanip>

using namespace flow_ad;
using namespace flow_ad::openrtb;
using namespace flow_ad::indexing;

// 解决 DeviceType 命名冲突
using AdDeviceType = flow_ad::DeviceType;
using OpenRTBDeviceType = flow_ad::openrtb::DeviceType;

// ========== 联调测试：OpenRTB 解析 + 广告索引 ==========

class OpenRTBIndexingIntegrationTest : public ::testing::Test
{
protected:
    OpenRTBParser parser;
    AdIndex index;

    void SetUp() override
    {
        // 准备测试广告数据
        setup_test_ads();
    }

    void setup_test_ads()
    {
        // 广告 1: 游戏类广告，定向北京+移动端+游戏兴趣
        Ad ad1;
        ad1.id = 1;
        ad1.campaign_id = 101;
        ad1.creative_id = 1001;
        ad1.bid_price = 2.5;
        ad1.categories = {AdCategory::GAMING};
        ad1.targeting_devices = {AdDeviceType::MOBILE};
        ad1.targeting_geo = {"北京", "上海", "深圳"};
        ad1.targeting_interests = {"游戏", "电竞", "娱乐"};
        index.add_ad(ad1.id, ad1);

        // 广告 2: 电商类广告，定向上海+桌面端+购物兴趣
        Ad ad2;
        ad2.id = 2;
        ad2.campaign_id = 102;
        ad2.creative_id = 1002;
        ad2.bid_price = 1.8;
        ad2.categories = {AdCategory::ECOMMERCE};
        ad2.targeting_devices = {AdDeviceType::DESKTOP};
        ad2.targeting_geo = {"上海", "杭州", "南京"};
        ad2.targeting_interests = {"购物", "优惠", "时尚"};
        index.add_ad(ad2.id, ad2);

        // 广告 3: 教育类广告，定向广州+所有设备+学习兴趣
        Ad ad3;
        ad3.id = 3;
        ad3.campaign_id = 103;
        ad3.creative_id = 1003;
        ad3.bid_price = 1.2;
        ad3.categories = {AdCategory::EDUCATION};
        ad3.targeting_devices = {AdDeviceType::MOBILE, AdDeviceType::DESKTOP, AdDeviceType::TABLET};
        ad3.targeting_geo = {"北京", "广州", "成都"};
        ad3.targeting_interests = {"学习", "在线课程", "考试"};
        index.add_ad(ad3.id, ad3);

        // 广告 4: 金融类广告，定向深圳+平板+理财兴趣
        Ad ad4;
        ad4.id = 4;
        ad4.campaign_id = 104;
        ad4.creative_id = 1004;
        ad4.bid_price = 3.0;
        ad4.categories = {AdCategory::FINANCE};
        ad4.targeting_devices = {AdDeviceType::TABLET};
        ad4.targeting_geo = {"深圳", "厦门", "青岛"};
        ad4.targeting_interests = {"理财", "投资", "基金"};
        index.add_ad(ad4.id, ad4);

        std::cout << "\n========================================" << std::endl;
        std::cout << "测试环境准备完成" << std::endl;
        std::cout << "已加载广告数量: " << index.size() << std::endl;
        std::cout << "========================================\n" << std::endl;
    }
};

// ========== 测试 1: 完整流程测试 ==========

TEST_F(OpenRTBIndexingIntegrationTest, CompleteWorkflow)
{
    std::cout << "\n========== 测试 1: 完整工作流程 ==========\n" << std::endl;

    // Step 1: 准备 OpenRTB 请求 JSON
    std::string openrtb_json = R"({
        "id": "req-001",
        "imp": [
            {
                "id": "imp-001",
                "banner": {
                    "w": [300],
                    "h": [250],
                    "pos": 1
                },
                "bidfloor": 0.5
            }
        ],
        "device": {
            "ua": "Mozilla/5.0",
            "ip": "192.168.1.1",
            "geo": {
                "country": "CHN",
                "city": "北京"
            },
            "devicetype": 1,
            "os": 1
        },
        "user": {
            "id": "user-001",
            "keywords": ["游戏", "娱乐"]
        }
    })";

    // Step 2: 解析 OpenRTB 请求
    std::cout << "Step 1: 解析 OpenRTB 请求..." << std::endl;
    auto parse_result = parser.parse(openrtb_json);

    ASSERT_TRUE(parse_result.success) << "OpenRTB 解析失败: " << parse_result.error_message;
    std::cout << "✓ OpenRTB 请求解析成功" << std::endl;
    std::cout << "  - 请求 ID: " << parse_result.request.id << std::endl;
    std::cout << "  - 广告位数量: " << parse_result.request.imp.size() << std::endl;

    if (parse_result.request.device)
    {
        std::cout << "  - 设备类型: "
                  << device_type_to_string(static_cast<OpenRTBDeviceType>(
                      parse_result.request.device->devicetype.value()))
                  << std::endl;
        if (parse_result.request.device->geo && parse_result.request.device->geo->city)
        {
            std::cout << "  - 地理位置: " << *parse_result.request.device->geo->city << std::endl;
        }
    }

    // Step 3: 使用广告索引进行召回
    std::cout << "\nStep 2: 执行广告召回..." << std::endl;
    QueryResult query_result = index.query(parse_result.request);

    std::cout << "✓ 广告召回完成" << std::endl;
    std::cout << "  - 总广告数: " << query_result.total_ads << std::endl;
    std::cout << "  - 召回广告数: " << query_result.candidate_ad_ids.size() << std::endl;
    std::cout << "  - 召回耗时: " << std::fixed << std::setprecision(3)
              << query_result.recall_time_ms << " ms" << std::endl;
    std::cout << "  - 召回率: " << std::setprecision(2)
              << (query_result.recall_rate() * 100) << "%" << std::endl;

    // Step 4: 获取召回广告的详细信息
    std::cout << "\nStep 3: 召回广告详情..." << std::endl;
    for (size_t i = 0; i < query_result.candidate_ad_ids.size(); ++i)
    {
        uint64_t ad_id = query_result.candidate_ad_ids[i];
        const Ad* ad_ptr = index.get_ad(ad_id);

        ASSERT_NE(ad_ptr, nullptr) << "无法获取广告 ID: " << ad_id;

        std::cout << "\n广告 #" << (i + 1) << ":" << std::endl;
        std::cout << "  ID: " << ad_ptr->id << std::endl;
        std::cout << "  出价: ¥" << ad_ptr->bid_price << std::endl;
        std::cout << "  分类: " << ad_category_to_string(ad_ptr->categories[0]) << std::endl;
        std::cout << "  定向地域: ";
        for (const auto& geo : ad_ptr->targeting_geo)
        {
            std::cout << geo << " ";
        }
        std::cout << std::endl;
    }

    // 验证结果
    EXPECT_GT(query_result.candidate_ad_ids.size(), 0) << "应该召回至少 1 个广告";
    EXPECT_LT(query_result.recall_time_ms, 10.0) << "召回时间应小于 10ms";
}

// ========== 测试 2: 多条件定向测试 ==========

TEST_F(OpenRTBIndexingIntegrationTest, MultiConditionTargeting)
{
    std::cout << "\n========== 测试 2: 多条件定向 ==========\n" << std::endl;

    // 测试用例：北京 + 移动端 + 游戏兴趣
    std::vector<std::pair<std::string, size_t>> test_cases = {
        {"北京+移动+游戏", 1},  // 应该召回广告 1
        {"上海+桌面+购物", 1},  // 应该召回广告 2
        {"广州+平板+学习", 1},  // 应该召回广告 3
        {"深圳+平板+理财", 1},  // 应该召回广告 4
        {"北京+桌面", 1},       // 应该召回广告 3（所有设备）
        {"不存在的城市", 0},    // 不应该召回任何广告
    };

    for (const auto& [test_name, expected_count] : test_cases)
    {
        std::cout << "\n测试用例: " << test_name << std::endl;

        // 构建基础 JSON
        std::string city = "CITY";
        int devicetype = 1;  // Mobile
        std::string keyword = "KEYWORD";

        // 根据测试用例修改参数
        if (test_name.find("北京") != std::string::npos)
        {
            city = "北京";
            keyword = "游戏";
        }
        else if (test_name.find("上海") != std::string::npos)
        {
            city = "上海";
            devicetype = 2;  // Desktop
            keyword = "购物";
        }
        else if (test_name.find("广州") != std::string::npos)
        {
            city = "广州";
            devicetype = 5;  // Tablet
            keyword = "学习";
        }
        else if (test_name.find("深圳") != std::string::npos)
        {
            city = "深圳";
            devicetype = 5;  // Tablet
            keyword = "理财";
        }
        else if (test_name.find("北京+桌面") != std::string::npos)
        {
            city = "北京";
            devicetype = 2;  // Desktop
            keyword = "学习";  // 匹配广告 3
        }
        else
        {
            city = "不存在的城市";
        }

        // 构建 JSON 字符串
        std::string openrtb_json = R"({
            "id": "req-test",
            "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}}],
            "device": {
                "geo": {"city": ")" + city + R"("},
                "devicetype": )" + std::to_string(devicetype) + R"(,
                "os": 1
            },
            "user": {
                "id": "user-test",
                "keywords": [")" + keyword + R"("]
            }
        })";

        // 解析并查询
        auto parse_result = parser.parse(openrtb_json);
        ASSERT_TRUE(parse_result.success);

        QueryResult query_result = index.query(parse_result.request);

        std::cout << "  预期召回: " << expected_count
                  << ", 实际召回: " << query_result.candidate_ad_ids.size() << std::endl;

        EXPECT_EQ(query_result.candidate_ad_ids.size(), expected_count);
    }
}

// ========== 测试 3: 性能压力测试 ==========

TEST_F(OpenRTBIndexingIntegrationTest, PerformanceStressTest)
{
    std::cout << "\n========== 测试 3: 性能压力测试 ==========\n" << std::endl;

    std::string openrtb_json = R"({
        "id": "req-perf",
        "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}}],
        "device": {
            "geo": {"city": "北京"},
            "devicetype": 1,
            "os": 1
        },
        "user": {
            "id": "user-perf",
            "keywords": ["游戏"]
        }
    })";

    const int num_iterations = 1000;
    std::vector<double> recall_times;
    recall_times.reserve(num_iterations);

    std::cout << "执行 " << num_iterations << " 次查询..." << std::endl;

    for (int i = 0; i < num_iterations; ++i)
    {
        auto parse_result = parser.parse(openrtb_json);
        ASSERT_TRUE(parse_result.success);

        QueryResult query_result = index.query(parse_result.request);
        recall_times.push_back(query_result.recall_time_ms);
    }

    // 统计性能数据
    double total_time = 0;
    double min_time = recall_times[0];
    double max_time = recall_times[0];

    for (double time : recall_times)
    {
        total_time += time;
        min_time = std::min(min_time, time);
        max_time = std::max(max_time, time);
    }

    double avg_time = total_time / num_iterations;
    double qps = 1000.0 / avg_time;

    std::cout << "\n性能统计:" << std::endl;
    std::cout << "  平均耗时: " << std::fixed << std::setprecision(3) << avg_time << " ms" << std::endl;
    std::cout << "  最小耗时: " << min_time << " ms" << std::endl;
    std::cout << "  最大耗时: " << max_time << " ms" << std::endl;
    std::cout << "  QPS: " << std::setprecision(0) << qps << std::endl;

    // 性能断言
    EXPECT_LT(avg_time, 5.0) << "平均召回时间应小于 5ms";
    EXPECT_GT(qps, 200) << "QPS 应大于 200";
}

// ========== 测试 4: 边界条件测试 ==========

TEST_F(OpenRTBIndexingIntegrationTest, EdgeCases)
{
    std::cout << "\n========== 测试 4: 边界条件 ==========\n" << std::endl;

    // 测试 1: 空兴趣列表
    {
        std::string openrtb_json = R"({
            "id": "req-edge-1",
            "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}}],
            "device": {
                "geo": {"city": "北京"},
                "devicetype": 1
            }
        })";

        auto parse_result = parser.parse(openrtb_json);
        ASSERT_TRUE(parse_result.success);

        QueryResult query_result = index.query(parse_result.request);
        std::cout << "空兴趣列表测试: 召回 " << query_result.candidate_ad_ids.size() << " 个广告" << std::endl;
        EXPECT_GT(query_result.candidate_ad_ids.size(), 0);
    }

    // 测试 2: 无效设备类型
    {
        std::string openrtb_json = R"({
            "id": "req-edge-2",
            "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}}],
            "device": {
                "geo": {"city": "北京"},
                "devicetype": 99
            }
        })";

        auto parse_result = parser.parse(openrtb_json);
        ASSERT_TRUE(parse_result.success);

        QueryResult query_result = index.query(parse_result.request);
        std::cout << "无效设备类型测试: 召回 " << query_result.candidate_ad_ids.size() << " 个广告" << std::endl;
        // 应该仍然召回广告（因为设备类型不匹配时不做过滤）
    }

    // 测试 3: 无地域信息
    {
        std::string openrtb_json = R"({
            "id": "req-edge-3",
            "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}}],
            "device": {
                "devicetype": 1
            }
        })";

        auto parse_result = parser.parse(openrtb_json);
        ASSERT_TRUE(parse_result.success);

        QueryResult query_result = index.query(parse_result.request);
        std::cout << "无地域信息测试: 召回 " << query_result.candidate_ad_ids.size() << " 个广告" << std::endl;
        EXPECT_GT(query_result.candidate_ad_ids.size(), 0);
    }
}

// ========== 主函数 ==========

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "  OpenRTB + 索引模块联调测试" << std::endl;
    std::cout << "========================================" << std::endl;

    int result = RUN_ALL_TESTS();

    std::cout << "\n========================================" << std::endl;
    std::cout << "  测试完成" << std::endl;
    std::cout << "========================================\n" << std::endl;

    return result;
}
