#pragma once

#include "core/feature_context.h"
#include "core/model_config.h"
#include "extractor/feature_extractor.h"
#include "engine/model_inference_engine.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <random>

namespace ctr {

/**
 * @brief CTR 预估管理器
 *
 * 负责:
 * - 管理多个特征提取器
 * - 管理多个推理引擎 (支持 A/B 测试)
 * - 流量分配和模型路由
 * - 模型热更新
 */
class CTRManager {
public:
    CTRManager();
    ~CTRManager() = default;

    /**
     * @brief 初始化 CTR 管理器
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 添加特征提取器
     * @param extractor 特征提取器
     */
    void add_feature_extractor(FeatureExtractorPtr extractor);

    /**
     * @brief 添加推理模型
     * @param config 模型配置
     * @return 是否成功
     */
    bool add_model(const ModelConfig& config);

    /**
     * @brief 预估单个广告的 CTR
     * @param request_data 请求数据 (JSON 字符串)
     * @param ad_data 广告数据 (JSON 字符串)
     * @param result [out] 推理结果
     * @return 是否成功
     */
    bool predict_ctr(
        const std::string& request_data,
        const std::string& ad_data,
        InferenceResult& result
    );

    /**
     * @brief 批量预估 CTR
     * @param request_data_list 请求数据列表
     * @param ad_data_list 广告数据列表
     * @param results [out] 推理结果列表
     * @return 是否成功
     */
    bool predict_ctr_batch(
        const std::vector<std::string>& request_data_list,
        const std::vector<std::string>& ad_data_list,
        std::vector<InferenceResult>& results
    );

    /**
     * @brief 重新加载模型
     * @param model_name 模型名称
     * @param new_model_path 新模型路径
     * @return 是否成功
     */
    bool reload_model(const std::string& model_name, const std::string& new_model_path);

    /**
     * @brief 启用/禁用模型
     * @param model_name 模型名称
     * @param enabled 是否启用
     * @return 是否成功
     */
    bool set_model_enabled(const std::string& model_name, bool enabled);

    /**
     * @brief 更新模型流量分配
     * @param model_name 模型名称
     * @param traffic_fraction 流量比例 (0.0 - 1.0)
     * @return 是否成功
     */
    bool update_model_traffic(const std::string& model_name, float traffic_fraction);

    /**
     * @brief 获取模型统计信息
     */
    struct ModelStats {
        std::string model_name;
        int64_t total_predictions;
        double avg_inference_time_ms;
        float current_traffic_fraction;
    };

    std::vector<ModelStats> get_model_stats() const;

private:
    /**
     * @brief 选择推理引擎 (根据流量分配)
     * @return 推理引擎指针
     */
    ModelInferenceEnginePtr select_inference_engine();

    /**
     * @brief 提取特征
     * @param request_data 请求数据
     * @param ad_data 广告数据
     * @param feature_context [out] 特征上下文
     * @return 是否成功
     */
    bool extract_features(
        const std::string& request_data,
        const std::string& ad_data,
        FeatureContext& feature_context
    );

private:
    // 特征提取器列表
    std::vector<FeatureExtractorPtr> feature_extractors_;

    // 推理引擎列表 (支持多个模型)
    struct ModelEntry {
        ModelConfig config;
        ModelInferenceEnginePtr engine;
        int64_t total_predictions;
        double total_inference_time_ms;
    };

    std::unordered_map<std::string, ModelEntry> models_;

    // 流量分配
    float total_traffic_fraction_;

    // 线程安全
    mutable std::mutex mutex_;

    // 随机数生成器 (用于流量分配)
    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<float> dist_;
};

} // namespace ctr
