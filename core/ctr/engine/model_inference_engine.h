#pragma once

#include "../core/feature_context.h"
#include "../core/model_config.h"
#include <vector>
#include <memory>

namespace ctr {

/**
 * @brief 模型推理引擎接口
 *
 * 定义模型推理的统一接口
 */
class IModelInferenceEngine {
public:
    virtual ~IModelInferenceEngine() = default;

    /**
     * @brief 初始化推理引擎
     * @param model_config 模型配置
     * @return 是否成功
     */
    virtual bool initialize(const ModelConfig& model_config) = 0;

    /**
     * @brief 单次推理
     * @param feature_context 特征上下文
     * @param result [out] 推理结果
     * @return 是否成功
     */
    virtual bool predict(const FeatureContext& feature_context, InferenceResult& result) = 0;

    /**
     * @brief 批量推理
     * @param feature_contexts 特征上下文列表
     * @param results [out] 推理结果列表
     * @return 是否成功
     */
    virtual bool predict_batch(
        const std::vector<FeatureContext>& feature_contexts,
        std::vector<InferenceResult>& results
    ) = 0;

    /**
     * @brief 获取模型配置
     */
    virtual const ModelConfig& get_model_config() const = 0;

    /**
     * @brief 检查引擎是否已初始化
     */
    virtual bool is_initialized() const = 0;

    /**
     * @brief 获取引擎名称
     */
    virtual std::string get_name() const = 0;
};

using ModelInferenceEnginePtr = std::shared_ptr<IModelInferenceEngine>;

} // namespace ctr
