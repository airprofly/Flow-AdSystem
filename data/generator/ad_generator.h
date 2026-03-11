#pragma once

// Author: airprofly

#include <vector>
#include <memory>
#include <random>
#include "ad_model.h"

namespace flow_ad {

// 生成器配置
struct GeneratorConfig {
    size_t num_ads = 100;              // 生成广告数量
    size_t num_campaigns = 10000;         // 生成计划数量
    size_t num_creatives = 50000;         // 生成创意数量
    size_t num_advertisers = 1000;        // 生成广告主数量

    // 出价范围
    double min_bid_price = 0.1;           // 最低出价 (元)
    double max_bid_price = 100.0;         // 最高出价 (元)

    // 预算范围
    double min_budget = 100.0;            // 最低预算 (元)
    double max_budget = 100000.0;         // 最高预算 (元)

    // 广告分类
    std::vector<AdCategory> allowed_categories;

    // 地域列表
    std::vector<std::string> allowed_geos;

    // 设备类型
    std::vector<DeviceType> allowed_devices;

    // 兴趣标签
    std::vector<std::string> allowed_interests;

    // 随机种子 (用于可重复生成)
    unsigned int random_seed = 42;

    GeneratorConfig() {
        // 默认配置
        allowed_categories = {
            AdCategory::ECOMMERCE,
            AdCategory::GAMING,
            AdCategory::FINANCE,
            AdCategory::EDUCATION,
            AdCategory::HEALTH,
            AdCategory::AUTOMOTIVE,
            AdCategory::TRAVEL,
            AdCategory::ENTERTAINMENT,
            AdCategory::TECHNOLOGY,
            AdCategory::FOOD
        };

        allowed_geos = {
            "北京", "上海", "广州", "深圳", "杭州", "成都", "重庆",
            "武汉", "西安", "南京", "天津", "苏州", "长沙", "郑州"
        };

        allowed_devices = {
            DeviceType::MOBILE,
            DeviceType::DESKTOP,
            DeviceType::TABLET
        };

        allowed_interests = {
            "购物", "游戏", "理财", "教育", "健康", "汽车",
            "旅游", "娱乐", "科技", "美食", "运动", "时尚",
            "母婴", "房产", "家居", "阅读", "音乐", "视频"
        };
    }
};

// 广告数据生成器
class AdDataGenerator {
public:
    explicit AdDataGenerator(const GeneratorConfig& config);

    // 生成所有数据
    void generate_all();

    // 生成广告主
    std::vector<Advertiser> generate_advertisers();

    // 生成广告计划
    std::vector<AdCampaign> generate_campaigns(
        const std::vector<Advertiser>& advertisers);

    // 生成创意素材
    std::vector<Creative> generate_creatives();

    // 生成广告
    std::vector<Ad> generate_ads(
        const std::vector<AdCampaign>& campaigns,
        const std::vector<Creative>& creatives);

    // 保存到文件
    bool save_to_file(const std::string& file_path) const;

    // 从文件加载
    bool load_from_file(const std::string& file_path);

    // 导出为 Protocol Buffers
    bool save_to_protobuf(const std::string& file_path) const;

    // 获取生成的数据
    const std::vector<Ad>& ads() const { return ads_; }
    const std::vector<AdCampaign>& campaigns() const { return campaigns_; }
    const std::vector<Creative>& creatives() const { return creatives_; }
    const std::vector<Advertiser>& advertisers() const { return advertisers_; }

    // 获取配置
    const GeneratorConfig& config() const { return config_; }

private:
    GeneratorConfig config_;
    std::mt19937 rng_;

    // 生成的数据
    std::vector<Advertiser> advertisers_;
    std::vector<AdCampaign> campaigns_;
    std::vector<Creative> creatives_;
    std::vector<Ad> ads_;

    // 辅助函数
    std::string random_string(size_t length);
    std::vector<AdCategory> random_categories(size_t max_count = 3);
    std::vector<std::string> random_geos(size_t max_count = 5);
    std::vector<DeviceType> random_devices(size_t max_count = 3);
    std::vector<std::string> random_interests(size_t max_count = 5);
    double random_double(double min, double max);
    uint64_t random_uint64(uint64_t min, uint64_t max);
    int random_int(int min, int max);
};

} // namespace flow_ad
