#pragma once

#include "../core/feature_spec.h"
#include "model_inference_engine.h"
#include <memory>
#include <onnxruntime_cxx_api.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace ctr
{

    /**
     * @brief ONNX Runtime 推理引擎实现
     *
     * 使用 ONNX Runtime 进行高性能模型推理
     */
    class ONNXRuntimeEngine : public IModelInferenceEngine
    {
    public:
        ONNXRuntimeEngine();
        ~ONNXRuntimeEngine() override;

        /**
         * @brief 初始化推理引擎
         * @param model_config 模型配置
         * @return 是否成功
         */
        bool initialize(const ModelConfig &model_config) override;

        /**
         * @brief 单次推理
         * @param feature_context 特征上下文
         * @param result [out] 推理结果
         * @return 是否成功
         */
        bool predict(const FeatureContext &feature_context, InferenceResult &result) override;

        /**
         * @brief 批量推理
         * @param feature_contexts 特征上下文列表
         * @param results [out] 推理结果列表
         * @return 是否成功
         */
        bool predict_batch(
            const std::vector<FeatureContext> &feature_contexts,
            std::vector<InferenceResult> &results) override;

        /**
         * @brief 获取模型配置
         */
        const ModelConfig &get_model_config() const override
        {
            return model_config_;
        }

        /**
         * @brief 检查引擎是否已初始化
         */
        bool is_initialized() const override
        {
            return initialized_;
        }

        /**
         * @brief 获取引擎名称
         */
        std::string get_name() const override
        {
            return "ONNXRuntimeEngine";
        }

    private:
        /**
         * @brief 准备输入张量
         * @param feature_context 特征上下文
         * @param input_tensors [out] 输入张量列表
         * @param input_names [out] 输入名称列表
         * @return 是否成功
         */
        bool prepare_input_tensors(
            const FeatureContext &feature_context,
            std::vector<Ort::Value> &input_tensors,
            std::vector<int64_t> &sparse_buffer,
            std::vector<float> &dense_buffer);

        /**
         * @brief 准备批量输入张量
         * @param feature_contexts 特征上下文列表
         * @param input_tensors [out] 输入张量列表
         * @param input_names [out] 输入名称列表
         * @return 是否成功
         */
        bool prepare_batch_input_tensors(
            const std::vector<FeatureContext> &feature_contexts,
            std::vector<Ort::Value> &input_tensors,
            std::vector<int64_t> &sparse_buffer,
            std::vector<float> &dense_buffer);

        /**
         * @brief 创建稀疏特征张量
         * @param feature_context 特征上下文
         * @return 稀疏特征张量
         */
        Ort::Value create_sparse_tensor(
            const FeatureContext &feature_context,
            std::vector<int64_t> &buffer);

        /**
         * @brief 创建批量稀疏特征张量
         * @param feature_contexts 特征上下文列表
         * @return 批量稀疏特征张量
         */
        Ort::Value create_batch_sparse_tensor(
            const std::vector<FeatureContext> &feature_contexts,
            std::vector<int64_t> &buffer);

        /**
         * @brief 创建连续特征张量
         * @param feature_context 特征上下文
         * @return 连续特征张量
         */
        Ort::Value create_dense_tensor(
            const FeatureContext &feature_context,
            std::vector<float> &buffer);

        /**
         * @brief 创建批量连续特征张量
         * @param feature_contexts 特征上下文列表
         * @return 批量连续特征张量
         */
        Ort::Value create_batch_dense_tensor(
            const std::vector<FeatureContext> &feature_contexts,
            std::vector<float> &buffer);

    private:
        bool initialized_;         // 是否已初始化
        ModelConfig model_config_; // 模型配置
        FeatureSpec feature_spec_; // 特征规范

        // ONNX Runtime 相关
        std::unique_ptr<Ort::Env> env_;                // ONNX 环境
        std::unique_ptr<Ort::Session> session_;        // ONNX 会话
        std::unique_ptr<Ort::MemoryInfo> memory_info_; // 内存信息

        // 模型输入输出信息
        std::vector<std::string> input_names_;        // 输入名称列表 (字符串存储)
        std::vector<std::string> output_names_;       // 输出名称列表 (字符串存储)
        std::vector<const char *> input_names_ptrs_;  // 输入名称指针列表 (稳定的指针)
        std::vector<const char *> output_names_ptrs_; // 输出名称指针列表 (稳定的指针)

        // 特征维度信息
        int64_t num_sparse_features_; // 稀疏特征数量
        int64_t num_dense_features_;  // 连续特征数量
    };

} // namespace ctr
