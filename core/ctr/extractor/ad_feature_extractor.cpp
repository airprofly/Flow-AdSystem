// Author: airprofly

#include "ad_feature_extractor.h"
#include "rapidjson/document.h"
#include <functional>

namespace ctr {

AdFeatureExtractor::AdFeatureExtractor() {
    // 定义支持的特征规范
    // 稀疏特征 (类别型) - 使用 Hash 编码
    feature_spec_.add_sparse_feature("ad_id", 1000000, 32);
    feature_spec_.add_sparse_feature("campaign_id", 100000, 32);
    feature_spec_.add_sparse_feature("industry_id", 1000, 16);
    feature_spec_.add_sparse_feature("creative_type", 100, 16);
    feature_spec_.add_sparse_feature("ad_size", 50, 16);

    // 连续特征 (数值型)
    feature_spec_.add_dense_feature("ad_hist_ctr");      // 广告历史 CTR
    feature_spec_.add_dense_feature("ad_hist_cvr");      // 广告历史 CVR
    feature_spec_.add_dense_feature("ad_position");      // 广告位置 (归一化)
}

bool AdFeatureExtractor::extract(const std::string& ad_data, FeatureContext& feature_context) {
    rapidjson::Document doc;
    if (doc.Parse(ad_data.c_str()).HasParseError()) {
        return false;
    }

    // 提取广告ID (稀疏特征 - Hash)
    if (doc.HasMember("id") && doc["id"].IsString()) {
        std::string ad_id = doc["id"].GetString();
        int64_t ad_id_hash = hash_string(ad_id) % 1000000;
        feature_context.set_sparse_feature("ad_id", ad_id_hash);
    } else {
        feature_context.set_sparse_feature("ad_id", 0);
    }

    // 提取活动ID (稀疏特征 - Hash)
    if (doc.HasMember("campaign_id") && doc["campaign_id"].IsString()) {
        std::string campaign_id = doc["campaign_id"].GetString();
        int64_t campaign_id_hash = hash_string(campaign_id) % 100000;
        feature_context.set_sparse_feature("campaign_id", campaign_id_hash);
    } else {
        feature_context.set_sparse_feature("campaign_id", 0);
    }

    // 提取行业ID (稀疏特征)
    if (doc.HasMember("industry") && doc["industry"].IsInt()) {
        int64_t industry_id = doc["industry"].GetInt();
        feature_context.set_sparse_feature("industry_id", industry_id);
    } else {
        feature_context.set_sparse_feature("industry_id", 0);
    }

    // 提取创意类型 (稀疏特征)
    if (doc.HasMember("creative_type") && doc["creative_type"].IsInt()) {
        int64_t creative_type = doc["creative_type"].GetInt();
        feature_context.set_sparse_feature("creative_type", creative_type);
    } else {
        feature_context.set_sparse_feature("creative_type", 0);
    }

    // 提取广告尺寸 (稀疏特征 - Hash)
    if (doc.HasMember("size") && doc["size"].IsString()) {
        std::string size = doc["size"].GetString();
        int64_t size_hash = hash_string(size) % 50;
        feature_context.set_sparse_feature("ad_size", size_hash);
    } else {
        feature_context.set_sparse_feature("ad_size", 0);
    }

    // 提取广告历史 CTR (连续特征)
    if (doc.HasMember("hist_ctr") && doc["hist_ctr"].IsFloat()) {
        float ctr = doc["hist_ctr"].GetFloat();
        feature_context.set_dense_feature("ad_hist_ctr", ctr);
    } else {
        feature_context.set_dense_feature("ad_hist_ctr", 0.0f);
    }

    // 提取广告历史 CVR (连续特征)
    if (doc.HasMember("hist_cvr") && doc["hist_cvr"].IsFloat()) {
        float cvr = doc["hist_cvr"].GetFloat();
        feature_context.set_dense_feature("ad_hist_cvr", cvr);
    } else {
        feature_context.set_dense_feature("ad_hist_cvr", 0.0f);
    }

    // 提取广告位置 (连续特征 - 归一化)
    if (doc.HasMember("position") && doc["position"].IsInt()) {
        int position = doc["position"].GetInt();
        float position_norm = std::min(1.0f, static_cast<float>(position) / 10.0f);
        feature_context.set_dense_feature("ad_position", position_norm);
    } else {
        feature_context.set_dense_feature("ad_position", 0.0f);
    }

    return true;
}

FeatureSpec AdFeatureExtractor::get_feature_spec() const {
    return feature_spec_;
}

int64_t AdFeatureExtractor::hash_string(const std::string& str) const {
    // 使用 FNV-1a Hash 算法
    const uint64_t FNV_offset_basis = 0xcbf29ce484222325ULL;
    const uint64_t FNV_prime = 0x100000001b3ULL;

    uint64_t hash = FNV_offset_basis;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= FNV_prime;
    }

    return static_cast<int64_t>(hash & 0x7FFFFFFFFFFFFFFF); // 确保为正数
}

} // namespace ctr
