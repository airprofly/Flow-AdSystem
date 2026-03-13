// Author: airprofly

#include "data_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace flow_ad {

// bool DataLoader::load_from_json(
//     const std::string& file_path,
//     AdService& service) {
//
//     try {
//         // 加载广告
//         auto ads = load_ads_from_json(file_path);
//         if (ads.empty()) {
//             std::cerr << "No ads found in file: " << file_path << std::endl;
//             return false;
//         }
//
//         // 加载计划
//         auto campaigns = load_campaigns_from_json(file_path);
//         for (const auto& campaign : campaigns) {
//             service.add_campaign(campaign);
//         }
//
//         // 加载创意
//         auto creatives = load_creatives_from_json(file_path);
//         for (const auto& creative : creatives) {
//             service.add_creative(creative);
//         }
//
//         // 批量添加广告
//         service.add_ads(ads);
//
//         std::cout << "Loaded " << ads.size() << " ads, "
//                   << campaigns.size() << " campaigns, "
//                   << creatives.size() << " creatives" << std::endl;
//
//         return true;
//
//     } catch (const std::exception& e) {
//         std::cerr << "Error loading data: " << e.what() << std::endl;
//         return false;
//     }
// }

std::vector<Ad> DataLoader::load_ads_from_json(
    const std::string& file_path) {

    std::vector<Ad> ads;

    // 读取文件
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    // 提取 ads 数组
    std::string ads_array = extract_json_array(content, "ads");
    if (ads_array.empty()) {
        return ads;
    }

    // 简化的 JSON 解析 (仅用于演示)
    // 实际生产环境应使用 nlohmann/json 或其他库

    // 这里返回空向量，因为完整解析比较复杂
    // 实际使用时应该使用专门的 JSON 库

    std::cerr << "Warning: JSON parsing not fully implemented" << std::endl;
    std::cerr << "Please use the generator's direct API instead" << std::endl;

    return ads;
}

std::vector<AdCampaign> DataLoader::load_campaigns_from_json(
    const std::string& file_path) {
    (void)file_path;
    std::vector<AdCampaign> campaigns;
    // TODO: 实现完整的 JSON 解析
    return campaigns;
}

std::vector<Creative> DataLoader::load_creatives_from_json(
    const std::string& file_path) {
    (void)file_path;
    std::vector<Creative> creatives;
    // TODO: 实现完整的 JSON 解析
    return creatives;
}

// bool DataLoader::load_from_protobuf(
//     const std::string& file_path,
//     AdService& service) {
//     // TODO: 实现 Protocol Buffers 加载
//     std::cerr << "Protobuf loading not yet implemented" << std::endl;
//     return false;
// }
//
// bool DataLoader::batch_import(
//     const std::string& file_path,
//     AdService& service,
//     size_t batch_size,
//     std::function<void(size_t, size_t)> progress_callback) {
//
//     // 使用生成器直接加载
//     GeneratorConfig config;
//     config.num_ads = 100000;
//
//     AdDataGenerator generator(config);
//     generator.generate_all();
//
//     const auto& ads = generator.ads();
//     const auto& campaigns = generator.campaigns();
//     const auto& creatives = generator.creatives();
//
//     // 批量导入
//     size_t total = ads.size();
//     for (size_t i = 0; i < ads.size(); i += batch_size) {
//         size_t end = std::min(i + batch_size, ads.size());
//
//         std::vector<Ad> batch(ads.begin() + i, ads.begin() + end);
//         service.add_ads(batch);
//
//         if (progress_callback) {
//             progress_callback(end, total);
//         }
//     }
//
//     // 导入计划和创意
//     for (const auto& campaign : campaigns) {
//         service.add_campaign(campaign);
//     }
//
//     for (const auto& creative : creatives) {
//         service.add_creative(creative);
//     }
//
//     return true;
// }

std::string DataLoader::extract_json_array(
    const std::string& json_content,
    const std::string& array_name) {

    std::string search = "\"" + array_name + "\"";
    size_t pos = json_content.find(search);

    if (pos == std::string::npos) {
        return "";
    }

    // 找到数组开始 [
    pos = json_content.find("[", pos);
    if (pos == std::string::npos) {
        return "";
    }

    // 找到匹配的 ]
    int depth = 0;
    size_t end = pos;
    for (; end < json_content.size(); ++end) {
        if (json_content[end] == '[') {
            depth++;
        } else if (json_content[end] == ']') {
            depth--;
            if (depth == 0) {
                break;
            }
        }
    }

    if (depth != 0) {
        return "";
    }

    return json_content.substr(pos, end - pos + 1);
}

std::vector<std::string> DataLoader::split_json_objects(
    const std::string& array_content) {

    std::vector<std::string> objects;

    // 简化实现，假设格式正确
    // 实际应该使用状态机解析

    size_t start = array_content.find('{');
    while (start != std::string::npos) {
        int depth = 0;
        size_t end = start;

        for (; end < array_content.size(); ++end) {
            if (array_content[end] == '{') {
                depth++;
            } else if (array_content[end] == '}') {
                depth--;
                if (depth == 0) {
                    objects.push_back(array_content.substr(start, end - start + 1));
                    break;
                }
            }
        }

        start = array_content.find('{', end + 1);
    }

    return objects;
}

} // namespace flow_ad
