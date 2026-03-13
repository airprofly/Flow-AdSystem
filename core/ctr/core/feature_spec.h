#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace ctr {

/**
 * @brief 特征类型枚举
 */
enum class FeatureType {
    SPARSE,  // 稀疏特征 (类别型, 需要嵌入)
    DENSE    // 连续特征 (数值型, 直接使用)
};

/**
 * @brief 单个特征的定义
 */
struct FeatureDef {
    std::string name;           // 特征名称
    FeatureType type;           // 特征类型
    int64_t cardinality;        // 基数 (仅稀疏特征有效)
    int64_t embedding_dim;      // 嵌入维度 (仅稀疏特征有效)

    FeatureDef(const std::string& n, FeatureType t, int64_t card = 0, int64_t emb_dim = 32)
        : name(n), type(t), cardinality(card), embedding_dim(emb_dim) {}
};

/**
 * @brief 特征规范 - 定义模型所需的所有特征
 */
class FeatureSpec {
public:
    FeatureSpec() = default;

    /**
     * @brief 添加稀疏特征
     * @param name 特征名称
     * @param cardinality 基数 (类别数量)
     * @param embedding_dim 嵌入维度
     */
    void add_sparse_feature(const std::string& name, int64_t cardinality, int64_t embedding_dim = 32) {
        sparse_features_.emplace_back(name, FeatureType::SPARSE, cardinality, embedding_dim);
        feature_name_to_index_[name] = sparse_features_.size() - 1;
    }

    /**
     * @brief 添加连续特征
     * @param name 特征名称
     */
    void add_dense_feature(const std::string& name) {
        dense_features_.emplace_back(name, FeatureType::DENSE, 0, 0);
        feature_name_to_index_[name] = dense_features_.size() - 1;
    }

    /**
     * @brief 获取所有稀疏特征定义
     */
    const std::vector<FeatureDef>& get_sparse_features() const {
        return sparse_features_;
    }

    /**
     * @brief 获取所有连续特征定义
     */
    const std::vector<FeatureDef>& get_dense_features() const {
        return dense_features_;
    }

    /**
     * @brief 获取稀疏特征数量
     */
    size_t get_num_sparse_features() const {
        return sparse_features_.size();
    }

    /**
     * @brief 获取连续特征数量
     */
    size_t get_num_dense_features() const {
        return dense_features_.size();
    }

    /**
     * @brief 根据名称查找特征索引
     * @param name 特征名称
     * @param is_sparse 是否为稀疏特征
     * @return 特征索引,如果不存在返回 -1
     */
    int get_feature_index(const std::string& name, bool is_sparse) const {
        auto it = feature_name_to_index_.find(name);
        if (it != feature_name_to_index_.end()) {
            return it->second;
        }
        return -1;
    }

    /**
     * @brief 获取所有特征的嵌入维度总和
     */
    int64_t get_total_embedding_dim() const {
        int64_t total = 0;
        for (const auto& feat : sparse_features_) {
            total += feat.embedding_dim;
        }
        return total;
    }

private:
    std::vector<FeatureDef> sparse_features_;   // 稀疏特征定义列表
    std::vector<FeatureDef> dense_features_;     // 连续特征定义列表
    std::unordered_map<std::string, int> feature_name_to_index_;  // 特征名称到索引的映射
};

} // namespace ctr
