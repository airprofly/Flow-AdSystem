// Author: airprofly

#include "index_loader.hpp"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include <cstdio>
#include <iostream>

namespace flow_ad::indexing
{

namespace
{
    // 解析 JSON 中的单个广告
    std::optional<std::pair<uint64_t, Ad>> parse_ad_from_json(const rapidjson::Value& ad_value)
    {
        if (!ad_value.IsObject())
        {
            return std::nullopt;
        }

        Ad ad;

        // 解析广告 ID
        if (ad_value.HasMember("id") && ad_value["id"].IsUint64())
        {
            ad.id = ad_value["id"].GetUint64();
        }
        else
        {
            return std::nullopt;
        }

        // 解析 campaign_id
        if (ad_value.HasMember("campaign_id") && ad_value["campaign_id"].IsUint64())
        {
            ad.campaign_id = ad_value["campaign_id"].GetUint64();
        }

        // 解析 creative_id
        if (ad_value.HasMember("creative_id") && ad_value["creative_id"].IsUint64())
        {
            ad.creative_id = ad_value["creative_id"].GetUint64();
        }

        // 解析 bid_price
        if (ad_value.HasMember("bid_price") && ad_value["bid_price"].IsDouble())
        {
            ad.bid_price = ad_value["bid_price"].GetDouble();
        }

        // 解析 categories
        if (ad_value.HasMember("categories") && ad_value["categories"].IsArray())
        {
            const auto& categories_array = ad_value["categories"].GetArray();
            for (const auto& category_value : categories_array)
            {
                if (category_value.IsInt())
                {
                    int category_int = category_value.GetInt();
                    if (category_int >= 0 && category_int <= 9)
                    {
                        ad.categories.push_back(static_cast<AdCategory>(category_int));
                    }
                }
            }
        }

        // 解析 targeting_geo
        if (ad_value.HasMember("targeting_geo") && ad_value["targeting_geo"].IsArray())
        {
            const auto& geo_array = ad_value["targeting_geo"].GetArray();
            for (const auto& geo_value : geo_array)
            {
                if (geo_value.IsString())
                {
                    ad.targeting_geo.push_back(geo_value.GetString());
                }
            }
        }

        // 解析 targeting_devices
        if (ad_value.HasMember("targeting_devices") && ad_value["targeting_devices"].IsArray())
        {
            const auto& devices_array = ad_value["targeting_devices"].GetArray();
            for (const auto& device_value : devices_array)
            {
                if (device_value.IsInt())
                {
                    int device_int = device_value.GetInt();
                    if (device_int >= 0 && device_int <= 2)
                    {
                        ad.targeting_devices.push_back(static_cast<DeviceType>(device_int));
                    }
                }
            }
        }

        // 解析 targeting_interests
        if (ad_value.HasMember("targeting_interests") && ad_value["targeting_interests"].IsArray())
        {
            const auto& interests_array = ad_value["targeting_interests"].GetArray();
            for (const auto& interest_value : interests_array)
            {
                if (interest_value.IsString())
                {
                    ad.targeting_interests.push_back(interest_value.GetString());
                }
            }
        }

        return std::make_pair(ad.id, ad);
    }
}

bool IndexLoader::load_from_json(const std::string& json_file_path, AdIndex& index)
{
    // 打开文件
    FILE* fp = std::fopen(json_file_path.c_str(), "rb");
    if (!fp)
    {
        std::cerr << "Error: Failed to open file: " << json_file_path << std::endl;
        return false;
    }

    // 读取文件内容
    char read_buffer[65536];
    rapidjson::FileReadStream stream(fp, read_buffer, sizeof(read_buffer));

    // 解析 JSON
    rapidjson::Document document;
    document.ParseStream(stream);

    std::fclose(fp);

    if (document.HasParseError())
    {
        std::cerr << "Error: JSON parse error at offset " << document.GetErrorOffset()
                  << ": " << document.GetParseError() << std::endl;
        return false;
    }

    // 检查是否为数组
    if (!document.IsArray())
    {
        std::cerr << "Error: JSON root is not an array" << std::endl;
        return false;
    }

    size_t success_count = 0;
    size_t error_count = 0;

    // 遍历广告数组
    for (const auto& ad_value : document.GetArray())
    {
        auto ad_result = parse_ad_from_json(ad_value);
        if (ad_result)
        {
            const auto& [ad_id, ad] = *ad_result;
            index.add_ad(ad_id, ad);
            success_count++;
        }
        else
        {
            error_count++;
        }
    }

    std::cout << "Loaded " << success_count << " ads to index";
    if (error_count > 0)
    {
        std::cout << " (" << error_count << " errors)";
    }
    std::cout << std::endl;

    return true;
}

bool IndexLoader::load_from_json_string(const std::string& json_string, AdIndex& index)
{
    rapidjson::Document document;
    document.Parse(json_string.c_str());

    if (document.HasParseError())
    {
        std::cerr << "Error: JSON parse error at offset " << document.GetErrorOffset()
                  << ": " << document.GetParseError() << std::endl;
        return false;
    }

    // 检查是否为数组
    if (!document.IsArray())
    {
        std::cerr << "Error: JSON root is not an array" << std::endl;
        return false;
    }

    size_t success_count = 0;
    size_t error_count = 0;

    // 遍历广告数组
    for (const auto& ad_value : document.GetArray())
    {
        auto ad_result = parse_ad_from_json(ad_value);
        if (ad_result)
        {
            const auto& [ad_id, ad] = *ad_result;
            index.add_ad(ad_id, ad);
            success_count++;
        }
        else
        {
            error_count++;
        }
    }

    std::cout << "Loaded " << success_count << " ads to index";
    if (error_count > 0)
    {
        std::cout << " (" << error_count << " errors)";
    }
    std::cout << std::endl;

    return true;
}

void IndexLoader::load_from_ads(const std::vector<Ad>& ads, AdIndex& index)
{
    for (const auto& ad : ads)
    {
        index.add_ad(ad.id, ad);
    }

    std::cout << "Loaded " << ads.size() << " ads to index" << std::endl;
}

} // namespace flow_ad::indexing
