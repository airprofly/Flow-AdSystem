// Author: airprofly

#include <gtest/gtest.h>
#include "core/indexing/bitmap.hpp"
#include "core/indexing/ad_index.hpp"
#include "core/indexing/index_loader.hpp"
#include "core/openrtb/openrtb_models.h"
#include "data/models/ad_model.h"
#include <random>

using namespace flow_ad;
using namespace flow_ad::indexing;
using namespace flow_ad::openrtb;

// 解决 DeviceType 命名冲突
using AdDeviceType = flow_ad::DeviceType;
using OpenRTBDeviceType = flow_ad::openrtb::DeviceType;

// ========== BitMap 测试 ==========

class BitMapTest : public ::testing::Test
{
protected:
    BitMap bitmap{100};
};

TEST_F(BitMapTest, DefaultConstruction)
{
    BitMap empty_bitmap;
    EXPECT_EQ(empty_bitmap.size(), 0);
    EXPECT_EQ(empty_bitmap.count(), 0);
}

TEST_F(BitMapTest, SetAndTest)
{
    bitmap.set(10);
    bitmap.set(50);
    bitmap.set(99);

    EXPECT_TRUE(bitmap.test(10));
    EXPECT_TRUE(bitmap.test(50));
    EXPECT_TRUE(bitmap.test(99));
    EXPECT_FALSE(bitmap.test(0));
    EXPECT_FALSE(bitmap.test(49));
}

TEST_F(BitMapTest, Count)
{
    EXPECT_EQ(bitmap.count(), 0);

    bitmap.set(0);
    EXPECT_EQ(bitmap.count(), 1);

    bitmap.set(1);
    bitmap.set(2);
    EXPECT_EQ(bitmap.count(), 3);
}

TEST_F(BitMapTest, Clear)
{
    bitmap.set(10);
    EXPECT_TRUE(bitmap.test(10));

    bitmap.clear(10);
    EXPECT_FALSE(bitmap.test(10));
}

TEST_F(BitMapTest, Intersection)
{
    BitMap bitmap1(100);
    BitMap bitmap2(100);

    bitmap1.set(10);
    bitmap1.set(20);
    bitmap1.set(30);

    bitmap2.set(20);
    bitmap2.set(30);
    bitmap2.set(40);

    BitMap result = bitmap1 & bitmap2;

    EXPECT_FALSE(result.test(10));
    EXPECT_TRUE(result.test(20));
    EXPECT_TRUE(result.test(30));
    EXPECT_FALSE(result.test(40));
    EXPECT_EQ(result.count(), 2);
}

TEST_F(BitMapTest, Union)
{
    BitMap bitmap1(100);
    BitMap bitmap2(100);

    bitmap1.set(10);
    bitmap1.set(20);

    bitmap2.set(20);
    bitmap2.set(30);

    BitMap result = bitmap1 | bitmap2;

    EXPECT_TRUE(result.test(10));
    EXPECT_TRUE(result.test(20));
    EXPECT_TRUE(result.test(30));
    EXPECT_EQ(result.count(), 3);
}

TEST_F(BitMapTest, Difference)
{
    BitMap bitmap1(100);
    BitMap bitmap2(100);

    bitmap1.set(10);
    bitmap1.set(20);
    bitmap1.set(30);

    bitmap2.set(20);
    bitmap2.set(40);

    BitMap result = bitmap1 - bitmap2;

    EXPECT_TRUE(result.test(10));
    EXPECT_FALSE(result.test(20));
    EXPECT_TRUE(result.test(30));
    EXPECT_FALSE(result.test(40));
    EXPECT_EQ(result.count(), 2);
}

TEST_F(BitMapTest, GetSetBits)
{
    bitmap.set(5);
    bitmap.set(15);
    bitmap.set(25);

    auto set_bits = bitmap.get_set_bits();

    EXPECT_EQ(set_bits.size(), 3);
    EXPECT_EQ(set_bits[0], 5);
    EXPECT_EQ(set_bits[1], 15);
    EXPECT_EQ(set_bits[2], 25);
}

TEST_F(BitMapTest, Resize)
{
    bitmap.set(50);
    bitmap.resize(200);

    EXPECT_EQ(bitmap.size(), 200);
    EXPECT_TRUE(bitmap.test(50));
    EXPECT_FALSE(bitmap.test(150));
}

TEST_F(BitMapTest, Reset)
{
    bitmap.set(10);
    bitmap.set(20);
    bitmap.set(30);

    EXPECT_EQ(bitmap.count(), 3);

    bitmap.reset();

    EXPECT_EQ(bitmap.count(), 0);
}

// ========== AdIndex 测试 ==========

class AdIndexTest : public ::testing::Test
{
protected:
    AdIndex index;

    void SetUp() override
    {
        // 添加测试广告
        add_test_ads();
    }

    void add_test_ads()
    {
        // 广告 1: 游戏、移动端、北京
        Ad ad1;
        ad1.id = 1;
        ad1.campaign_id = 101;
        ad1.creative_id = 1001;
        ad1.bid_price = 1.5;
        ad1.categories = {AdCategory::GAMING};
        ad1.targeting_devices = {AdDeviceType::MOBILE};
        ad1.targeting_geo = {"北京", "上海"};
        ad1.targeting_interests = {"游戏", "竞技"};

        // 广告 2: 电商、桌面端、上海
        Ad ad2;
        ad2.id = 2;
        ad2.campaign_id = 102;
        ad2.creative_id = 1002;
        ad2.bid_price = 2.0;
        ad2.categories = {AdCategory::ECOMMERCE};
        ad2.targeting_devices = {AdDeviceType::DESKTOP};
        ad2.targeting_geo = {"上海", "深圳"};
        ad2.targeting_interests = {"购物", "优惠"};

        // 广告 3: 教育、所有设备、北京
        Ad ad3;
        ad3.id = 3;
        ad3.campaign_id = 103;
        ad3.creative_id = 1003;
        ad3.bid_price = 1.2;
        ad3.categories = {AdCategory::EDUCATION};
        ad3.targeting_devices = {AdDeviceType::MOBILE, AdDeviceType::DESKTOP, AdDeviceType::TABLET};
        ad3.targeting_geo = {"北京", "广州"};
        ad3.targeting_interests = {"学习", "在线课程"};

        // 广告 4: 金融、平板、深圳
        Ad ad4;
        ad4.id = 4;
        ad4.campaign_id = 104;
        ad4.creative_id = 1004;
        ad4.bid_price = 3.0;
        ad4.categories = {AdCategory::FINANCE};
        ad4.targeting_devices = {AdDeviceType::TABLET};
        ad4.targeting_geo = {"深圳", "杭州"};
        ad4.targeting_interests = {"理财", "投资"};

        index.add_ad(ad1.id, ad1);
        index.add_ad(ad2.id, ad2);
        index.add_ad(ad3.id, ad3);
        index.add_ad(ad4.id, ad4);
    }
};

TEST_F(AdIndexTest, AddAd)
{
    EXPECT_EQ(index.size(), 4);
    EXPECT_TRUE(index.has_ad(1));
    EXPECT_TRUE(index.has_ad(2));
    EXPECT_TRUE(index.has_ad(3));
    EXPECT_TRUE(index.has_ad(4));
}

TEST_F(AdIndexTest, GetAd)
{
    auto ad1_ptr = index.get_ad(1);
    ASSERT_NE(ad1_ptr, nullptr);
    EXPECT_EQ(ad1_ptr->id, 1);
    EXPECT_EQ(ad1_ptr->campaign_id, 101);
    EXPECT_EQ(ad1_ptr->bid_price, 1.5);
}

TEST_F(AdIndexTest, RemoveAd)
{
    EXPECT_TRUE(index.remove_ad(2));
    EXPECT_EQ(index.size(), 3);
    EXPECT_FALSE(index.has_ad(2));

    // 删除不存在的广告
    EXPECT_FALSE(index.remove_ad(999));
}

TEST_F(AdIndexTest, QueryByGeo)
{
    // 查询北京的广告
    OpenRTBRequest request;
    request.id = "test-request-1";

    Device device;
    device.geo = Geo{};
    device.geo->city = "北京";
    request.device = device;

    QueryResult result = index.query(request);

    // 应该召回广告 1 和 3（都定向到北京）
    EXPECT_EQ(result.candidate_ad_ids.size(), 2);
    EXPECT_NE(std::find(result.candidate_ad_ids.begin(), result.candidate_ad_ids.end(), 1),
              result.candidate_ad_ids.end());
    EXPECT_NE(std::find(result.candidate_ad_ids.begin(), result.candidate_ad_ids.end(), 3),
              result.candidate_ad_ids.end());
}

TEST_F(AdIndexTest, QueryByDevice)
{
    // 查询移动端广告
    OpenRTBRequest request;
    request.id = "test-request-2";

    Device device;
    device.devicetype = static_cast<int>(openrtb::DeviceType::MOBILE);
    request.device = device;

    QueryResult result = index.query(request);

    // 应该召回广告 1 和 3（都支持移动端）
    EXPECT_EQ(result.candidate_ad_ids.size(), 2);
}

TEST_F(AdIndexTest, QueryByInterest)
{
    // 查询对"游戏"感兴趣的用户
    OpenRTBRequest request;
    request.id = "test-request-3";

    User user;
    user.id = "user-1";
    user.keywords = std::vector<std::string>{"游戏", "娱乐"};
    request.user = user;

    QueryResult result = index.query(request);

    // 应该召回广告 1（定向"游戏"兴趣）
    EXPECT_EQ(result.candidate_ad_ids.size(), 1);
    EXPECT_EQ(result.candidate_ad_ids[0], 1);
}

TEST_F(AdIndexTest, QueryByMultipleConditions)
{
    // 查询北京 + 移动端 + 游戏兴趣
    OpenRTBRequest request;
    request.id = "test-request-4";

    Device device;
    device.geo = Geo{};
    device.geo->city = "北京";
    device.devicetype = static_cast<int>(openrtb::DeviceType::MOBILE);
    request.device = device;

    User user;
    user.id = "user-2";
    user.keywords = std::vector<std::string>{"游戏"};
    request.user = user;

    QueryResult result = index.query(request);

    // 应该召回广告 1（满足所有条件）
    EXPECT_EQ(result.candidate_ad_ids.size(), 1);
    EXPECT_EQ(result.candidate_ad_ids[0], 1);
}

TEST_F(AdIndexTest, QueryNoMatch)
{
    // 查询不存在的城市
    OpenRTBRequest request;
    request.id = "test-request-5";

    Device device;
    device.geo = Geo{};
    device.geo->city = "不存在的城市";
    request.device = device;

    QueryResult result = index.query(request);

    // 应该返回空结果
    EXPECT_EQ(result.candidate_ad_ids.size(), 0);
}

TEST_F(AdIndexTest, QueryPerformance)
{
    OpenRTBRequest request;
    request.id = "test-request-6";

    Device device;
    device.geo = Geo{};
    device.geo->city = "北京";
    device.devicetype = static_cast<int>(openrtb::DeviceType::MOBILE);
    request.device = device;

    User user;
    user.id = "user-3";
    user.keywords = std::vector<std::string>{"游戏"};
    request.user = user;

    // 执行多次查询
    for (int i = 0; i < 100; ++i)
    {
        QueryResult result = index.query(request);
        EXPECT_GT(result.candidate_ad_ids.size(), 0);
    }

    // 检查性能统计
    auto stats = index.get_stats();
    EXPECT_EQ(stats.total_queries, 100);
    EXPECT_GT(stats.avg_recall_time_ms, 0.0);
    EXPECT_LT(stats.avg_recall_time_ms, 10.0);  // 平均召回时间应小于 10ms
}

TEST_F(AdIndexTest, Clear)
{
    EXPECT_EQ(index.size(), 4);

    index.clear();

    EXPECT_EQ(index.size(), 0);
    EXPECT_FALSE(index.has_ad(1));
}

TEST_F(AdIndexTest, GetStats)
{
    auto stats = index.get_stats();

    EXPECT_EQ(stats.total_ads, 4);
    EXPECT_EQ(stats.max_ad_id, 4);
    EXPECT_GT(stats.index_memory_bytes, 0);
    EXPECT_GT(stats.store_memory_bytes, 0);
}

// ========== 大规模性能测试 ==========

TEST(LargeScaleIndexingTest, StressTest)
{
    AdIndex index;

    // 添加 10000 个广告
    const size_t num_ads = 10000;
    std::vector<Ad> ads;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> geo_dist(0, 9);
    std::uniform_int_distribution<> device_dist(0, 2);
    std::uniform_int_distribution<> interest_dist(0, 19);

    std::vector<std::string> cities = {
        "北京", "上海", "深圳", "广州", "杭州",
        "成都", "重庆", "武汉", "西安", "南京"
    };

    std::vector<std::string> interests = {
        "游戏", "购物", "学习", "理财", "娱乐",
        "体育", "新闻", "社交", "视频", "音乐",
        "旅游", "美食", "健康", "汽车", "科技",
        "时尚", "母婴", "房产", "求职", "交友"
    };

    for (size_t i = 0; i < num_ads; ++i)
    {
        Ad ad;
        ad.id = i;
        ad.campaign_id = i * 10;
        ad.creative_id = i * 100;
        ad.bid_price = 1.0 + (i % 100) * 0.1;

        // 随机选择 1-3 个城市
        int num_geo = 1 + (i % 3);
        for (int j = 0; j < num_geo; ++j)
        {
            ad.targeting_geo.push_back(cities[geo_dist(gen)]);
        }

        // 随机选择 1-2 个设备类型
        int num_devices = 1 + (i % 2);
        for (int j = 0; j < num_devices; ++j)
        {
            ad.targeting_devices.push_back(static_cast<AdDeviceType>(device_dist(gen)));
        }

        // 随机选择 1-5 个兴趣
        int num_interests = 1 + (i % 5);
        for (int j = 0; j < num_interests; ++j)
        {
            ad.targeting_interests.push_back(interests[interest_dist(gen)]);
        }

        ads.push_back(ad);
    }

    // 批量添加
    for (const auto& ad : ads)
    {
        index.add_ad(ad.id, ad);
    }

    EXPECT_EQ(index.size(), num_ads);

    // 执行查询
    OpenRTBRequest request;
    request.id = "stress-test";

    Device device;
    device.geo = Geo{};
    device.geo->city = "北京";
    device.devicetype = static_cast<int>(openrtb::DeviceType::MOBILE);
    request.device = device;

    User user;
    user.id = "test-user";
    user.keywords = std::vector<std::string>{"游戏", "娱乐"};
    request.user = user;

    QueryResult result = index.query(request);

    // 检查查询结果
    EXPECT_GT(result.candidate_ad_ids.size(), 0);
    EXPECT_LT(result.recall_time_ms, 50.0);  // 召回时间应小于 50ms

    // 检查内存占用
    auto stats = index.get_stats();
    std::cout << "Large scale test stats:" << std::endl;
    std::cout << "  Total ads: " << stats.total_ads << std::endl;
    std::cout << "  Recall time: " << result.recall_time_ms << " ms" << std::endl;
    std::cout << "  Index memory: " << (stats.index_memory_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "  Store memory: " << (stats.store_memory_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
