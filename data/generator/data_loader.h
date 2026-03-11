#pragma once

// Author: airprofly

#include <string>
#include <vector>
#include <functional>
#include "ad_model.h"
// #include "services/ad_service/ad_service.h"  // TODO: 启用 AdService

namespace flow_ad {

// 前向声明 (AdService 尚未实现)
// class AdService;

// 数据加载器
class DataLoader {
public:
    // 从 JSON 文件加载广告数据到服务
    // static bool load_from_json(
    //     const std::string& file_path,
    //     AdService& service);

    // 从 JSON 文件加载广告数据
    static std::vector<Ad> load_ads_from_json(
        const std::string& file_path);

    static std::vector<AdCampaign> load_campaigns_from_json(
        const std::string& file_path);

    static std::vector<Creative> load_creatives_from_json(
        const std::string& file_path);

    // 从 Protocol Buffers 文件加载
    // static bool load_from_protobuf(
    //     const std::string& file_path,
    //     AdService& service);

    // 批量导入 (带进度报告)
    // static bool batch_import(
    //     const std::string& file_path,
    //     AdService& service,
    //     size_t batch_size = 1000,
    //     std::function<void(size_t, size_t)> progress_callback = nullptr);

private:
    // 简单的 JSON 解析器 (简化实现)
    static std::string extract_json_array(
        const std::string& json_content,
        const std::string& array_name);

    static std::vector<std::string> split_json_objects(
        const std::string& array_content);
};

} // namespace flow_ad
