#pragma once

#include "feature_spec.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <memory>

namespace ctr {

/**
 * @brief 特征值 - 存储单个特征的值
 */
union FeatureValue {
    int64_t sparse_id;    // 稀疏特征: 类别ID
    float dense_value;    // 连续特征: 浮点数值

    FeatureValue() : sparse_id(0) {}
    FeatureValue(int64_t id) : sparse_id(id) {}
    FeatureValue(float val) : dense_value(val) {}
};

/**
 * @brief 特征上下文 - 存储一次推理所需的所有特征值
 */
class FeatureContext {
public:
    FeatureContext() = default;

    /**
     * @brief 设置稀疏特征值
     * @param feature_name 特征名称
     * @param category_id 类别ID
     */
    void set_sparse_feature(const std::string& feature_name, int64_t category_id) {
        sparse_features_[feature_name] = FeatureValue(category_id);
    }

    /**
     * @brief 设置连续特征值
     * @param feature_name 特征名称
     * @param value 数值
     */
    void set_dense_feature(const std::string& feature_name, float value) {
        dense_features_[feature_name] = FeatureValue(value);
    }

    /**
     * @brief 获取稀疏特征值
     * @param feature_name 特征名称
     * @param default_value 默认值 (如果特征不存在)
     * @return 类别ID
     */
    int64_t get_sparse_feature(const std::string& feature_name, int64_t default_value = 0) const {
        auto it = sparse_features_.find(feature_name);
        if (it != sparse_features_.end()) {
            return it->second.sparse_id;
        }
        return default_value;
    }

    /**
     * @brief 获取连续特征值
     * @param feature_name 特征名称
     * @param default_value 默认值 (如果特征不存在)
     * @return 数值
     */
    float get_dense_feature(const std::string& feature_name, float default_value = 0.0f) const {
        auto it = dense_features_.find(feature_name);
        if (it != dense_features_.end()) {
            return it->second.dense_value;
        }
        return default_value;
    }

    /**
     * @brief 清空所有特征
     */
    void clear() {
        sparse_features_.clear();
        dense_features_.clear();
    }

    /**
     * @brief 获取稀疏特征映射
     */
    const std::unordered_map<std::string, FeatureValue>& get_sparse_features() const {
        return sparse_features_;
    }

    /**
     * @brief 获取连续特征映射
     */
    const std::unordered_map<std::string, FeatureValue>& get_dense_features() const {
        return dense_features_;
    }

    /**
     * @brief 检查是否包含某个特征
     */
    bool has_sparse_feature(const std::string& feature_name) const {
        return sparse_features_.find(feature_name) != sparse_features_.end();
    }

    bool has_dense_feature(const std::string& feature_name) const {
        return dense_features_.find(feature_name) != dense_features_.end();
    }

private:
    std::unordered_map<std::string, FeatureValue> sparse_features_;  // 稀疏特征值
    std::unordered_map<std::string, FeatureValue> dense_features_;   // 连续特征值
};

} // namespace ctr
