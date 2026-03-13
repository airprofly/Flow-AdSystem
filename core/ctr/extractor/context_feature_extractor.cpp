// Author: airprofly

#include "context_feature_extractor.h"
#include "rapidjson/document.h"
#include <ctime>
#include <cmath>

namespace ctr {

ContextFeatureExtractor::ContextFeatureExtractor() {
    // 定义支持的特征规范
    // 稀疏特征 (类别型)
    feature_spec_.add_sparse_feature("day_of_week", 7, 8);          // 星期几 (0-6)
    feature_spec_.add_sparse_feature("is_weekend", 2, 4);           // 是否周末
    feature_spec_.add_sparse_feature("page_category", 1000, 16);    // 页面类别
    feature_spec_.add_sparse_feature("network_type", 10, 8);        // 网络类型
    feature_spec_.add_sparse_feature("carrier", 100, 16);           // 运营商

    // 连续特征 (数值型)
    feature_spec_.add_dense_feature("hour_norm");          // 小时 (归一化)
    feature_spec_.add_dense_feature("time_slot");          // 时段 (0-5)
}

bool ContextFeatureExtractor::extract(const std::string& request_data, FeatureContext& feature_context) {
    rapidjson::Document doc;
    if (doc.Parse(request_data.c_str()).HasParseError()) {
        return false;
    }

    // 提取时间戳
    int64_t timestamp = 0;
    if (doc.HasMember("timestamp") && doc["timestamp"].IsInt64()) {
        timestamp = doc["timestamp"].GetInt64();
    } else {
        // 使用当前时间
        timestamp = static_cast<int64_t>(std::time(nullptr));
    }

    extract_time_features(timestamp, feature_context);

    // 提取页面类别 (稀疏特征)
    if (doc.HasMember("site") && doc["site"].IsObject()) {
        const auto& site = doc["site"];
        if (site.HasMember("category") && site["category"].IsInt()) {
            int64_t category = site["category"].GetInt();
            feature_context.set_sparse_feature("page_category", category);
        } else {
            feature_context.set_sparse_feature("page_category", 0);
        }
    } else {
        feature_context.set_sparse_feature("page_category", 0);
    }

    // 提取网络类型 (稀疏特征)
    if (doc.HasMember("device") && doc["device"].IsObject()) {
        const auto& device = doc["device"];
        if (device.HasMember("network_type") && device["network_type"].IsInt()) {
            int64_t network_type = device["network_type"].GetInt();
            feature_context.set_sparse_feature("network_type", network_type);
        } else {
            feature_context.set_sparse_feature("network_type", 0);
        }

        // 提取运营商 (稀疏特征)
        if (device.HasMember("carrier") && device["carrier"].IsInt()) {
            int64_t carrier = device["carrier"].GetInt();
            feature_context.set_sparse_feature("carrier", carrier);
        } else {
            feature_context.set_sparse_feature("carrier", 0);
        }
    } else {
        feature_context.set_sparse_feature("network_type", 0);
        feature_context.set_sparse_feature("carrier", 0);
    }

    return true;
}

FeatureSpec ContextFeatureExtractor::get_feature_spec() const {
    return feature_spec_;
}

void ContextFeatureExtractor::extract_time_features(int64_t timestamp, FeatureContext& feature_context) const {
    // 转换为本地时间
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm* tm = std::localtime(&time);

    int hour = tm->tm_hour;
    int wday = tm->tm_wday;  // 0 = Sunday, 6 = Saturday

    // 小时 (连续特征 - 归一化)
    float hour_norm = normalize_hour(hour);
    feature_context.set_dense_feature("hour_norm", hour_norm);

    // 时段 (连续特征): 0-5
    // 0: 凌晨 (0-6), 1: 上午 (7-12), 2: 下午 (13-18), 3: 晚上 (19-23)
    int time_slot = 0;
    if (hour >= 7 && hour <= 12) {
        time_slot = 1;
    } else if (hour >= 13 && hour <= 18) {
        time_slot = 2;
    } else if (hour >= 19 && hour <= 23) {
        time_slot = 3;
    }
    feature_context.set_dense_feature("time_slot", static_cast<float>(time_slot));

    // 星期几 (稀疏特征)
    feature_context.set_sparse_feature("day_of_week", wday);

    // 是否周末 (稀疏特征)
    int is_weekend = (wday == 0 || wday == 6) ? 1 : 0;
    feature_context.set_sparse_feature("is_weekend", is_weekend);
}

float ContextFeatureExtractor::normalize_hour(int hour) const {
    // 将小时归一化到 [0, 1]
    return static_cast<float>(hour) / 23.0f;
}

} // namespace ctr
