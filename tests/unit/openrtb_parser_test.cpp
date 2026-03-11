// Author: airprofly

#include "core/openrtb/openrtb_models.h"
#include "core/openrtb/openrtb_parser.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

using namespace flow_ad::openrtb;

class OpenRTBParserTest : public ::testing::Test
{
protected:
    OpenRTBParser parser;

    // 读取测试数据文件
    std::string read_test_file(const std::string &filename)
    {
        std::ifstream file("../../data/data/" + filename);
        if (!file.is_open())
        {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

// ========== 基础解析测试 ==========

TEST_F(OpenRTBParserTest, ParseValidRequest)
{
    std::string json = R"({
        "id": "test-request-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}],
        "device": {
            "ua": "Mozilla/5.0",
            "ip": "192.168.1.1",
            "devicetype": 1,
            "os": 1
        },
        "user": {"id": "user-001"}
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.id, "test-request-001");
    EXPECT_EQ(result.request.imp.size(), 1);
    EXPECT_TRUE(result.request.device.has_value());
    EXPECT_TRUE(result.request.user.has_value());
}

TEST_F(OpenRTBParserTest, ParseMissingRequiredField)
{
    std::string json = R"({
        "imp": [{"id": "imp-1"}]
    })";

    auto result = parser.parse(json);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_type, ParseError::MISSING_REQUIRED_FIELD);
}

TEST_F(OpenRTBParserTest, ParseInvalidJSON)
{
    std::string json = R"({"id": "test", "imp": [})";

    auto result = parser.parse(json);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_type, ParseError::INVALID_JSON);
}

// ========== Banner 解析测试 ==========

TEST_F(OpenRTBParserTest, ParseBannerWithSingleSize)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.request.imp[0].banner.has_value());
    EXPECT_EQ(result.request.imp[0].banner->w->size(), 1);
    EXPECT_EQ((*result.request.imp[0].banner->w)[0], 300);
}

TEST_F(OpenRTBParserTest, ParseBannerWithMultipleSizes)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": [300, 728], "h": [250, 90]}}]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.imp[0].banner->w->size(), 2);
    EXPECT_EQ((*result.request.imp[0].banner->w)[0], 300);
    EXPECT_EQ((*result.request.imp[0].banner->w)[1], 728);
}

TEST_F(OpenRTBParserTest, BannerGetPrimarySize)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    auto size = result.request.imp[0].banner->get_primary_size();
    EXPECT_EQ(size.first, 300);
    EXPECT_EQ(size.second, 250);
}

// ========== Device 解析测试 ==========

TEST_F(OpenRTBParserTest, ParseDeviceWithAllFields)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1"}],
        "device": {
            "ua": "Mozilla/5.0",
            "ip": "192.168.1.1",
            "geo": {"country": "CHN", "city": "北京"},
            "devicetype": 1,
            "os": 1,
            "osv": "16.0",
            "connectiontype": 2
        }
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.request.device->ua.has_value());
    EXPECT_TRUE(result.request.device->geo.has_value());
    EXPECT_EQ(result.request.device->geo->country, "CHN");
    EXPECT_EQ(result.request.device->geo->city, "北京");
    EXPECT_EQ(*result.request.device->devicetype, 1);
    EXPECT_EQ(*result.request.device->os, 1);
}

// ========== User 解析测试 ==========

TEST_F(OpenRTBParserTest, ParseUserWithDemographics)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1"}],
        "user": {
            "id": "user-001",
            "yob": 1990,
            "gender": "M",
            "keywords": ["游戏", "科技"]
        }
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.user->yob, 1990);
    EXPECT_EQ(result.request.user->gender, "M");
    EXPECT_EQ(result.request.user->keywords->size(), 2);
}

// ========== 批量解析测试 ==========

TEST_F(OpenRTBParserTest, ParseBatchRequests)
{
    // 使用 Step 1 生成的测试数据
    std::string test_data = read_test_file("openrtb_requests.json");

    if (test_data.empty())
    {
        GTEST_SKIP() << "Test data file not found, skipping batch test";
        return;
    }

    auto results = parser.parse_batch(test_data);

    EXPECT_GT(results.size(), 0);

    // 检查成功率
    int success_count = 0;
    for (const auto &result : results)
    {
        if (result.success)
            success_count++;
    }

    // 至少应该有 50% 成功率
    EXPECT_GT(success_count, results.size() / 2);
}

// ========== 验证测试 ==========

TEST_F(OpenRTBParserTest, ValidateValidRequest)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}],
        "device": {"ua": "Mozilla/5.0", "ip": "192.168.1.1"},
        "user": {"id": "user-001"}
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(parser.validate(result.request));
}

TEST_F(OpenRTBParserTest, ValidateInvalidRequest)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1"}],
        "device": {"ua": "Mozilla/5.0"}
    })";

    auto result = parser.parse(json);
    // device 缺少 ip, 但基本验证应该通过
    EXPECT_TRUE(parser.validate(result.request));

    // 详细验证应该返回错误
    auto errors = parser.validate_detailed(result.request);
    EXPECT_GT(errors.size(), 0);
}

// ========== 边界条件测试 ==========

TEST_F(OpenRTBParserTest, ParseEmptyImpressionList)
{
    std::string json = R"({
        "id": "test-001",
        "imp": []
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.get_impression_count(), 0);
}

TEST_F(OpenRTBParserTest, ParseMultipleImpressions)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [
            {"id": "imp-1", "banner": {"w": 300, "h": 250}},
            {"id": "imp-2", "banner": {"w": 728, "h": 90}},
            {"id": "imp-3", "video": {"w": 640, "h": 480}}
        ]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.get_impression_count(), 3);
}

TEST_F(OpenRTBParserTest, ParseRequestWithApp)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1"}],
        "app": {
            "id": "app-001",
            "name": "TestApp",
            "bundle": "com.test.app"
        }
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.request.is_app());
    EXPECT_FALSE(result.request.is_site());
}

TEST_F(OpenRTBParserTest, ParseRequestWithSite)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1"}],
        "site": {
            "id": "site-001",
            "name": "TestSite",
            "domain": "example.com"
        }
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.request.is_site());
    EXPECT_FALSE(result.request.is_app());
}

// ========== 序列化测试 ==========

TEST_F(OpenRTBParserTest, SerializeToJson)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);

    // 序列化回 JSON
    std::string serialized = OpenRTBParser::to_json(result.request);

    // 再次解析
    auto result2 = parser.parse(serialized);
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(result.request.id, result2.request.id);
}

// ========== 枚举转换测试 ==========

TEST_F(OpenRTBParserTest, DeviceTypeToString)
{
    EXPECT_EQ(device_type_to_string(DeviceType::MOBILE), "Mobile");
    EXPECT_EQ(device_type_to_string(DeviceType::PERSONAL_COMPUTER), "Personal Computer");
    EXPECT_EQ(device_type_to_string(DeviceType::TABLET), "Tablet");
}

TEST_F(OpenRTBParserTest, OSToString)
{
    EXPECT_EQ(os_to_string(OS::IOS), "iOS");
    EXPECT_EQ(os_to_string(OS::ANDROID), "Android");
    EXPECT_EQ(os_to_string(OS::WINDOWS), "Windows");
}

TEST_F(OpenRTBParserTest, AuctionTypeToString)
{
    EXPECT_EQ(auction_type_to_string(AuctionType::FIRST_PRICE), "First Price");
    EXPECT_EQ(auction_type_to_string(AuctionType::SECOND_PRICE), "Second Price");
}

TEST_F(OpenRTBParserTest, GenderToString)
{
    EXPECT_EQ(gender_to_string(Gender::MALE), "Male");
    EXPECT_EQ(gender_to_string(Gender::FEMALE), "Female");
}

TEST_F(OpenRTBParserTest, StringToDeviceType)
{
    EXPECT_EQ(string_to_device_type("Mobile"), DeviceType::MOBILE);
    EXPECT_EQ(string_to_device_type("1"), DeviceType::MOBILE);
    EXPECT_EQ(string_to_device_type("Tablet"), DeviceType::TABLET);
    EXPECT_EQ(string_to_device_type("5"), DeviceType::TABLET);
}

TEST_F(OpenRTBParserTest, StringToOS)
{
    EXPECT_EQ(string_to_os("iOS"), OS::IOS);
    EXPECT_EQ(string_to_os("1"), OS::IOS);
    EXPECT_EQ(string_to_os("Android"), OS::ANDROID);
}

// ========== Impression 类型测试 ==========

TEST_F(OpenRTBParserTest, ImpressionGetBannerType)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}]
    })";

    auto result = parser.parse(json);
    EXPECT_EQ(result.request.imp[0].get_impression_type(), "banner");
}

TEST_F(OpenRTBParserTest, ImpressionGetVideoType)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "video": {"w": 640, "h": 480}}]
    })";

    auto result = parser.parse(json);
    EXPECT_EQ(result.request.imp[0].get_impression_type(), "video");
}

TEST_F(OpenRTBParserTest, ImpressionGetNativeType)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "native": {"request": "test"}}]
    })";

    auto result = parser.parse(json);
    EXPECT_EQ(result.request.imp[0].get_impression_type(), "native");
}

// ========== 性能测试 (仅在发布模式下运行) ==========

#ifndef DEBUG
TEST_F(OpenRTBParserTest, PerformanceTest)
{
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}],
        "device": {"ua": "Mozilla/5.0", "ip": "192.168.1.1"},
        "user": {"id": "user-001"}
    })";

    // 解析 1000 次
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        auto result = parser.parse(json);
        EXPECT_TRUE(result.success);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 次解析应该在合理时间内完成 (例如 < 1秒)
    EXPECT_LT(duration.count(), 1000);
}
#endif

// ========== 完整 OpenRTB 请求测试 ==========

TEST_F(OpenRTBParserTest, ParseCompleteOpenRTBRequest)
{
    std::string json = R"json({
        "id": "req-001",
        "imp": [
            {
                "id": "imp-1",
                "banner": {
                    "w": [300, 728],
                    "h": [250, 90],
                    "pos": 1
                },
                "bidfloor": 0.5,
                "bidfloorcur": "USD"
            }
        ],
        "device": {
            "ua": "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X)",
            "ip": "192.168.1.100",
            "geo": {
                "country": "CHN",
                "region": "Beijing",
                "city": "Beijing",
                "lat": 39.9042,
                "lon": 116.4074,
                "type": 2
            },
            "devicetype": 5,
            "os": 1,
            "osv": "16.0",
            "connectiontype": 2
        },
        "user": {
            "id": "user-001",
            "buyeruid": "buyer-12345",
            "yob": 1990,
            "gender": "M",
            "keywords": ["gaming", "tech", "sports"]
        },
        "app": {
            "id": "app-001",
            "name": "Test Game",
            "bundle": "com.test.game",
            "cat": [1, 2],
            "ver": "1.0.0"
        },
        "at": 1,
        "bcat": [1, 2, 3],
        "badv": ["bad-advertiser.com"]
    })json";

    auto result = parser.parse(json);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.id, "req-001");
    EXPECT_EQ(result.request.imp.size(), 1);
    EXPECT_TRUE(result.request.device.has_value());
    EXPECT_TRUE(result.request.user.has_value());
    EXPECT_TRUE(result.request.app.has_value());
    EXPECT_EQ(*result.request.at, 1); // First Price Auction
    EXPECT_TRUE(result.request.bcat.has_value());
    EXPECT_EQ(result.request.bcat->size(), 3);
}

// 主函数
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
