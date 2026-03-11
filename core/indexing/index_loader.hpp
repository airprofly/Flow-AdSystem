#pragma once

// Author: airprofly

#include "ad_index.hpp"
#include "ad_model.h"
#include <string>
#include <vector>

namespace flow_ad::indexing
{

// 数据加载器 - 从 JSON 文件加载广告数据到索引
class IndexLoader
{
public:
    // 从 JSON 文件加载广告数据到索引
    static bool load_from_json(const std::string& json_file_path, AdIndex& index);

    // 从 JSON 字符串加载广告数据到索引
    static bool load_from_json_string(const std::string& json_string, AdIndex& index);

    // 从广告向量加载到索引
    static void load_from_ads(const std::vector<Ad>& ads, AdIndex& index);
};

} // namespace flow_ad::indexing
