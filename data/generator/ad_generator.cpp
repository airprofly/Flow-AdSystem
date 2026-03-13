// Author: airprofly

#include "ad_generator.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace flow_ad
{

    // 广告主名称列表
    static const std::vector<std::string> kAdvertiserNames = {
        "阿里巴巴", "腾讯", "百度", "字节跳动", "京东", "美团",
        "网易", "小米", "华为", "OPPO", "vivo", "携程",
        "滴滴", "快手", "拼多多", "苏宁", "国美", "唯品会"};

    // 创意标题模板
    static const std::vector<std::string> kCreativeTitles = {
        "限时特惠，不容错过！",
        "品质生活，从这里开始",
        "新品上市，抢先体验",
        "超值优惠，立即抢购",
        "独家专属，尊贵享受",
        "智能科技，改变生活",
        "精选推荐，值得信赖",
        "热门爆款，好评如潮"};

    // 创意描述模板
    static const std::vector<std::string> kCreativeDescriptions = {
        "这是一个精心打造的产品，为您带来极致体验。",
        "优质品质，实惠价格，让生活更美好。",
        "创新设计，卓越性能，满足您的各种需求。",
        "热销产品，用户口碑推荐，值得拥有。",
        "专业团队，严格品控，为您提供最优质的服务。"};

    AdDataGenerator::AdDataGenerator(const GeneratorConfig &config)
        : config_(config), rng_(config.random_seed)
    {
        advertisers_.reserve(config_.num_advertisers);
        campaigns_.reserve(config_.num_campaigns);
        creatives_.reserve(config_.num_creatives);
        ads_.reserve(config_.num_ads);
    }

    void AdDataGenerator::generate_all()
    {
        // 1. 生成广告主
        advertisers_ = generate_advertisers();
        std::cout << "Generated " << advertisers_.size() << " advertisers" << std::endl;

        // 2. 生成广告计划
        campaigns_ = generate_campaigns(advertisers_);
        std::cout << "Generated " << campaigns_.size() << " campaigns" << std::endl;

        // 3. 生成创意素材
        creatives_ = generate_creatives();
        std::cout << "Generated " << creatives_.size() << " creatives" << std::endl;

        // 4. 生成广告
        ads_ = generate_ads(campaigns_, creatives_);
        std::cout << "Generated " << ads_.size() << " ads" << std::endl;
    }

    std::vector<Advertiser> AdDataGenerator::generate_advertisers()
    {
        std::vector<Advertiser> advertisers;
        advertisers.reserve(config_.num_advertisers);

        for (size_t i = 0; i < config_.num_advertisers; ++i)
        {
            Advertiser advertiser;
            advertiser.id = 10000 + i;

            // 随机选择名称
            if (!kAdvertiserNames.empty())
            {
                advertiser.name = kAdvertiserNames[i % kAdvertiserNames.size()];
                if (i >= kAdvertiserNames.size())
                {
                    advertiser.name += " " + std::to_string(i / kAdvertiserNames.size() + 1);
                }
            }
            else
            {
                advertiser.name = "Advertiser_" + std::to_string(i);
            }

            // 随机选择行业分类
            if (!config_.allowed_categories.empty())
            {
                size_t idx = random_int(0, config_.allowed_categories.size() - 1);
                advertiser.category = ad_category_to_string(config_.allowed_categories[idx]);
            }
            else
            {
                advertiser.category = "General";
            }

            advertisers.push_back(advertiser);
        }

        return advertisers;
    }

    std::vector<AdCampaign> AdDataGenerator::generate_campaigns(
        const std::vector<Advertiser> &advertisers)
    {

        std::vector<AdCampaign> campaigns;
        campaigns.reserve(config_.num_campaigns);

        auto now = std::chrono::system_clock::now();

        for (size_t i = 0; i < config_.num_campaigns; ++i)
        {
            AdCampaign campaign;
            campaign.id = 100000 + i;

            // 随机分配广告主
            if (!advertisers.empty())
            {
                size_t adv_idx = random_int(0, advertisers.size() - 1);
                campaign.advertiser_id = advertisers[adv_idx].id;
            }
            else
            {
                campaign.advertiser_id = 10000;
            }

            campaign.name = "Campaign_" + std::to_string(campaign.id);

            // 随机预算
            campaign.budget = random_double(config_.min_budget, config_.max_budget);
            campaign.spent = 0.0;

            // 随机出价
            campaign.bid_price = random_double(config_.min_bid_price, config_.max_bid_price);

            // 随机状态
            int status_roll = random_int(1, 100);
            if (status_roll <= 80)
            {
                campaign.status = AdStatus::ACTIVE;
            }
            else if (status_roll <= 95)
            {
                campaign.status = AdStatus::PAUSED;
            }
            else
            {
                campaign.status = AdStatus::PENDING;
            }

            // 随机时间范围 (未来30天内)
            int start_days = random_int(0, 10);
            int duration_days = random_int(7, 30);

            campaign.start_time = now + std::chrono::hours(24 * start_days);
            campaign.end_time = campaign.start_time + std::chrono::hours(24 * duration_days);

            campaigns.push_back(campaign);
        }

        return campaigns;
    }

    std::vector<Creative> AdDataGenerator::generate_creatives()
    {
        std::vector<Creative> creatives;
        creatives.reserve(config_.num_creatives);

        // 创意尺寸
        std::vector<std::pair<uint32_t, uint32_t>> sizes = {
            {320, 50},  // Mobile banner
            {728, 90},  // Desktop leaderboard
            {300, 250}, // Medium rectangle
            {160, 600}, // Wide skyscraper
            {300, 600}, // Half page
            {320, 480}, // Mobile interstitial
            {480, 320}  // Mobile landscape
        };

        // 创意类型
        std::vector<std::string> types = {"banner", "video", "native"};

        for (size_t i = 0; i < config_.num_creatives; ++i)
        {
            Creative creative;
            creative.id = 500000 + i;

            // 随机标题和描述
            if (!kCreativeTitles.empty())
            {
                creative.title = kCreativeTitles[random_int(0, kCreativeTitles.size() - 1)];
            }
            else
            {
                creative.title = "Creative " + std::to_string(i);
            }

            if (!kCreativeDescriptions.empty())
            {
                creative.description = kCreativeDescriptions[random_int(0, kCreativeDescriptions.size() - 1)];
            }
            else
            {
                creative.description = "Description for creative " + std::to_string(i);
            }

            // 随机 URL
            std::ostringstream url;
            url << "https://cdn.example.com/creative/" << creative.id << ".jpg";
            creative.url = url.str();

            // 随机类型
            creative.type = types[random_int(0, types.size() - 1)];

            // 随机尺寸
            auto size = sizes[random_int(0, sizes.size() - 1)];
            creative.width = size.first;
            creative.height = size.second;

            creatives.push_back(creative);
        }

        return creatives;
    }

    std::vector<Ad> AdDataGenerator::generate_ads(
        const std::vector<AdCampaign> &campaigns,
        const std::vector<Creative> &creatives)
    {

        std::vector<Ad> ads;
        ads.reserve(config_.num_ads);

        for (size_t i = 0; i < config_.num_ads; ++i)
        {
            Ad ad;
            ad.id = 1000000 + i;

            // 随机分配广告计划
            if (!campaigns.empty())
            {
                size_t camp_idx = random_int(0, campaigns.size() - 1);
                ad.campaign_id = campaigns[camp_idx].id;
                ad.bid_price = campaigns[camp_idx].bid_price;
            }
            else
            {
                ad.campaign_id = 100000;
                ad.bid_price = random_double(config_.min_bid_price, config_.max_bid_price);
            }

            // 随机分配创意
            if (!creatives.empty())
            {
                size_t creative_idx = random_int(0, creatives.size() - 1);
                ad.creative_id = creatives[creative_idx].id;
            }
            else
            {
                ad.creative_id = 500000;
            }

            // 随机分类 (1-3个)
            ad.categories = random_categories(random_int(1, 3));

            // 随机地域定向 (1-5个)
            ad.targeting_geo = random_geos(random_int(1, 5));

            // 随机设备定向 (1-3个)
            auto devices = random_devices(random_int(1, 3));
            ad.targeting_devices.assign(devices.begin(), devices.end());

            // 随机兴趣定向 (1-5个)
            ad.targeting_interests = random_interests(random_int(1, 5));

            ads.push_back(ad);
        }

        return ads;
    }

    bool AdDataGenerator::save_to_file(const std::string &file_path) const
    {
        std::ofstream file(file_path);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            return false;
        }

        // 写入广告数据 (JSON 格式)
        file << "{\n";
        file << "  \"advertisers\": [\n";

        for (size_t i = 0; i < advertisers_.size(); ++i)
        {
            const auto &adv = advertisers_[i];
            file << "    {\n";
            file << "      \"id\": " << adv.id << ",\n";
            file << "      \"name\": \"" << adv.name << "\",\n";
            file << "      \"category\": \"" << adv.category << "\"\n";
            file << "    }";
            if (i < advertisers_.size() - 1)
                file << ",";
            file << "\n";
        }

        file << "  ],\n";
        file << "  \"campaigns\": [\n";

        for (size_t i = 0; i < campaigns_.size(); ++i)
        {
            const auto &camp = campaigns_[i];
            file << "    {\n";
            file << "      \"id\": " << camp.id << ",\n";
            file << "      \"advertiser_id\": " << camp.advertiser_id << ",\n";
            file << "      \"name\": \"" << camp.name << "\",\n";
            file << "      \"budget\": " << camp.budget << ",\n";
            file << "      \"bid_price\": " << camp.bid_price << ",\n";
            file << "      \"status\": " << static_cast<int>(camp.status) << "\n";
            file << "    }";
            if (i < campaigns_.size() - 1)
                file << ",";
            file << "\n";
        }

        file << "  ],\n";
        file << "  \"creatives\": [\n";

        for (size_t i = 0; i < creatives_.size(); ++i)
        {
            const auto &creative = creatives_[i];
            file << "    {\n";
            file << "      \"id\": " << creative.id << ",\n";
            file << "      \"title\": \"" << creative.title << "\",\n";
            file << "      \"type\": \"" << creative.type << "\",\n";
            file << "      \"width\": " << creative.width << ",\n";
            file << "      \"height\": " << creative.height << "\n";
            file << "    }";
            if (i < creatives_.size() - 1)
                file << ",";
            file << "\n";
        }

        file << "  ],\n";
        file << "  \"ads\": [\n";

        for (size_t i = 0; i < ads_.size(); ++i)
        {
            const auto &ad = ads_[i];
            file << "    {\n";
            file << "      \"id\": " << ad.id << ",\n";
            file << "      \"campaign_id\": " << ad.campaign_id << ",\n";
            file << "      \"creative_id\": " << ad.creative_id << ",\n";
            file << "      \"bid_price\": " << ad.bid_price << ",\n";
            file << "      \"categories\": [";

            for (size_t j = 0; j < ad.categories.size(); ++j)
            {
                file << static_cast<int>(ad.categories[j]);
                if (j < ad.categories.size() - 1)
                    file << ", ";
            }

            file << "],\n";
            file << "      \"targeting_geo\": [";

            for (size_t j = 0; j < ad.targeting_geo.size(); ++j)
            {
                file << "\"" << ad.targeting_geo[j] << "\"";
                if (j < ad.targeting_geo.size() - 1)
                    file << ", ";
            }

            file << "]\n";
            file << "    }";
            if (i < ads_.size() - 1)
                file << ",";
            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        std::cout << "Saved " << ads_.size() << " ads to " << file_path << std::endl;
        return true;
    }

    bool AdDataGenerator::load_from_file(const std::string &file_path)
    {
        (void)file_path;
        // TODO: 实现 JSON 解析
        std::cerr << "Load from file not yet implemented" << std::endl;
        return false;
    }

    bool AdDataGenerator::save_to_protobuf(const std::string &file_path) const
    {
        (void)file_path;
        // TODO: 实现 Protocol Buffers 序列化
        std::cerr << "Protobuf export not yet implemented" << std::endl;
        return false;
    }

    // 辅助函数实现

    std::string AdDataGenerator::random_string(size_t length)
    {
        static const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        std::string result;
        result.reserve(length);

        for (size_t i = 0; i < length; ++i)
        {
            result += charset[random_int(0, sizeof(charset) - 2)];
        }

        return result;
    }

    std::vector<AdCategory> AdDataGenerator::random_categories(size_t max_count)
    {
        std::vector<AdCategory> categories;

        if (config_.allowed_categories.empty())
        {
            return categories;
        }

        size_t count = std::min(max_count, config_.allowed_categories.size());
        count = random_int(1, count);

        std::vector<size_t> indices(config_.allowed_categories.size());
        for (size_t i = 0; i < indices.size(); ++i)
        {
            indices[i] = i;
        }

        std::shuffle(indices.begin(), indices.end(), rng_);

        for (size_t i = 0; i < count; ++i)
        {
            categories.push_back(config_.allowed_categories[indices[i]]);
        }

        return categories;
    }

    std::vector<std::string> AdDataGenerator::random_geos(size_t max_count)
    {
        std::vector<std::string> geos;

        if (config_.allowed_geos.empty())
        {
            return geos;
        }

        size_t count = std::min(max_count, config_.allowed_geos.size());
        count = random_int(1, count);

        std::vector<size_t> indices(config_.allowed_geos.size());
        for (size_t i = 0; i < indices.size(); ++i)
        {
            indices[i] = i;
        }

        std::shuffle(indices.begin(), indices.end(), rng_);

        for (size_t i = 0; i < count; ++i)
        {
            geos.push_back(config_.allowed_geos[indices[i]]);
        }

        return geos;
    }

    std::vector<DeviceType> AdDataGenerator::random_devices(size_t max_count)
    {
        std::vector<DeviceType> devices;

        if (config_.allowed_devices.empty())
        {
            return devices;
        }

        size_t count = std::min(max_count, config_.allowed_devices.size());
        count = random_int(1, count);

        std::vector<size_t> indices(config_.allowed_devices.size());
        for (size_t i = 0; i < indices.size(); ++i)
        {
            indices[i] = i;
        }

        std::shuffle(indices.begin(), indices.end(), rng_);

        for (size_t i = 0; i < count; ++i)
        {
            devices.push_back(config_.allowed_devices[indices[i]]);
        }

        return devices;
    }

    std::vector<std::string> AdDataGenerator::random_interests(size_t max_count)
    {
        std::vector<std::string> interests;

        if (config_.allowed_interests.empty())
        {
            return interests;
        }

        size_t count = std::min(max_count, config_.allowed_interests.size());
        count = random_int(1, count);

        std::vector<size_t> indices(config_.allowed_interests.size());
        for (size_t i = 0; i < indices.size(); ++i)
        {
            indices[i] = i;
        }

        std::shuffle(indices.begin(), indices.end(), rng_);

        for (size_t i = 0; i < count; ++i)
        {
            interests.push_back(config_.allowed_interests[indices[i]]);
        }

        return interests;
    }

    double AdDataGenerator::random_double(double min, double max)
    {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng_);
    }

    uint64_t AdDataGenerator::random_uint64(uint64_t min, uint64_t max)
    {
        std::uniform_int_distribution<uint64_t> dist(min, max);
        return dist(rng_);
    }

    int AdDataGenerator::random_int(int min, int max)
    {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng_);
    }

} // namespace flow_ad
