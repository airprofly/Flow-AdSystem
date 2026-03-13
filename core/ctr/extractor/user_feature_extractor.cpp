// Author: airprofly

#include "user_feature_extractor.h"
#include "rapidjson/document.h"
#include <algorithm>
#include <unordered_map>
#include <cmath>

namespace ctr {

UserFeatureExtractor::UserFeatureExtractor() {
    // 定义支持的特征规范
    // 稀疏特征 (类别型)
    feature_spec_.add_sparse_feature("user_gender", 4, 16);      // M/F/O/U
    feature_spec_.add_sparse_feature("user_city", 10000, 32);     // 城市ID
    feature_spec_.add_sparse_feature("user_device_type", 10, 16); // 设备类型

    // 连续特征 (数值型)
    feature_spec_.add_dense_feature("user_age_norm");       // 归一化年龄
    feature_spec_.add_dense_feature("user_history_ctr");    // 历史 CTR
    feature_spec_.add_dense_feature("user_history_imp");    // 历史展示次数
}

bool UserFeatureExtractor::extract(const std::string& request_data, FeatureContext& feature_context) {
    rapidjson::Document doc;
    if (doc.Parse(request_data.c_str()).HasParseError()) {
        return false;
    }

    // 解析 User 对象
    if (!doc.HasMember("user") || !doc["user"].IsObject()) {
        return false;
    }

    const auto& user = doc["user"];

    // 提取性别 (稀疏特征)
    if (user.HasMember("gender") && user["gender"].IsString()) {
        std::string gender = user["gender"].GetString();
        int64_t gender_id = encode_gender(gender);
        feature_context.set_sparse_feature("user_gender", gender_id);
    } else {
        // 默认值: Unknown
        feature_context.set_sparse_feature("user_gender", 3);
    }

    // 提取城市ID (稀疏特征)
    if (user.HasMember("city") && user["city"].IsInt()) {
        int64_t city_id = user["city"].GetInt();
        feature_context.set_sparse_feature("user_city", city_id);
    } else {
        // 默认值: 0
        feature_context.set_sparse_feature("user_city", 0);
    }

    // 提取设备类型 (稀疏特征)
    if (user.HasMember("device") && user["device"].IsObject()) {
        const auto& device = user["device"];
        if (device.HasMember("type") && device["type"].IsString()) {
            std::string device_type = device["type"].GetString();
            int64_t device_id = encode_device_type(device_type);
            feature_context.set_sparse_feature("user_device_type", device_id);
        } else {
            feature_context.set_sparse_feature("user_device_type", 0);
        }
    } else {
        feature_context.set_sparse_feature("user_device_type", 0);
    }

    // 提取年龄 (连续特征 - 归一化)
    if (user.HasMember("age") && user["age"].IsInt()) {
        int age = user["age"].GetInt();
        float age_norm = normalize_age(age);
        feature_context.set_dense_feature("user_age_norm", age_norm);
    } else {
        // 默认值: 0.5 (中年)
        feature_context.set_dense_feature("user_age_norm", 0.5f);
    }

    // 提取历史 CTR (连续特征)
    if (user.HasMember("history_ctr") && user["history_ctr"].IsFloat()) {
        float ctr = user["history_ctr"].GetFloat();
        feature_context.set_dense_feature("user_history_ctr", ctr);
    } else {
        // 默认值: 0.0
        feature_context.set_dense_feature("user_history_ctr", 0.0f);
    }

    // 提取历史展示次数 (连续特征 - 对数归一化)
    if (user.HasMember("history_impressions") && user["history_impressions"].IsInt()) {
        int imps = user["history_impressions"].GetInt();
        float log_imps = std::log(1.0f + static_cast<float>(imps));
        feature_context.set_dense_feature("user_history_imp", log_imps);
    } else {
        feature_context.set_dense_feature("user_history_imp", 0.0f);
    }

    return true;
}

FeatureSpec UserFeatureExtractor::get_feature_spec() const {
    return feature_spec_;
}

float UserFeatureExtractor::normalize_age(int age) const {
    // 将年龄归一化到 [0, 1]
    // 假设年龄范围: 0 - 100
    const int min_age = 0;
    const int max_age = 100;
    float normalized = static_cast<float>(age - min_age) / (max_age - min_age);
    return std::max(0.0f, std::min(1.0f, normalized));
}

int64_t UserFeatureExtractor::encode_gender(const std::string& gender) const {
    static const std::unordered_map<std::string, int64_t> gender_map = {
        {"M", 0},   // Male
        {"F", 1},   // Female
        {"O", 2},   // Other
        {"U", 3}    // Unknown
    };

    auto it = gender_map.find(gender);
    if (it != gender_map.end()) {
        return it->second;
    }
    return 3; // Unknown
}

int64_t UserFeatureExtractor::encode_device_type(const std::string& device_type) const {
    static const std::unordered_map<std::string, int64_t> device_map = {
        {"mobile", 0},
        {"tablet", 1},
        {"desktop", 2},
        {"connected_tv", 3},
        {"unknown", 4}
    };

    auto it = device_map.find(device_type);
    if (it != device_map.end()) {
        return it->second;
    }
    return 4; // unknown
}

} // namespace ctr
