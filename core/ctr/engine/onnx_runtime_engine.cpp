// Author: airprofly

#include "onnx_runtime_engine.h"
#include "../extractor/feature_extractor.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace ctr
{

    ONNXRuntimeEngine::ONNXRuntimeEngine()
        : initialized_(false), num_sparse_features_(0), num_dense_features_(0)
    {
        try
        {
            // 创建 ONNX Runtime 环境
            env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "FlowAdSystem_CTR");
            memory_info_ = std::make_unique<Ort::MemoryInfo>(
                Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to create ONNX Runtime environment: " << e.what() << std::endl;
        }
    }

    ONNXRuntimeEngine::~ONNXRuntimeEngine()
    {
        // 智能指针自动清理
    }

    bool ONNXRuntimeEngine::initialize(const ModelConfig &model_config)
    {
        model_config_ = model_config;

        try
        {
            if (!env_)
            {
                throw std::runtime_error("ONNX Runtime environment is not available");
            }

            if (!std::filesystem::exists(model_config.model_path))
            {
                throw std::runtime_error("Model file not found: " + model_config.model_path);
            }

            // 创建 ONNX 会话
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(4);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            session_ = std::make_unique<Ort::Session>(
                *env_,
                model_config.model_path.c_str(),
                session_options);

            // 获取输入输出信息
            input_names_.clear();
            output_names_.clear();

            Ort::AllocatorWithDefaultOptions allocator;
            const size_t input_count = session_->GetInputCount();
            const size_t output_count = session_->GetOutputCount();

            for (size_t i = 0; i < input_count; ++i)
            {
                auto input_name = session_->GetInputNameAllocated(i, allocator);
                if (input_name && input_name.get() != nullptr)
                {
                    input_names_.emplace_back(input_name.get());
                }
            }

            for (size_t i = 0; i < output_count; ++i)
            {
                auto output_name = session_->GetOutputNameAllocated(i, allocator);
                if (output_name && output_name.get() != nullptr)
                {
                    output_names_.emplace_back(output_name.get());
                }
            }

            auto has_name = [](const std::vector<std::string> &names, const std::string &name)
            {
                return std::find(names.begin(), names.end(), name) != names.end();
            };

            if (has_name(input_names_, "sparse_features") && has_name(input_names_, "dense_features"))
            {
                input_names_ = {"sparse_features", "dense_features"};
            }

            if (has_name(output_names_, "ctr_prediction"))
            {
                output_names_ = {"ctr_prediction"};
            }

            if (input_names_.empty())
            {
                input_names_ = {"sparse_features", "dense_features"};
            }

            if (output_names_.empty())
            {
                output_names_ = {"ctr_prediction"};
            }

            // 构建稳定的指针列表 (避免临时指针问题)
            input_names_ptrs_.clear();
            output_names_ptrs_.clear();
            for (const auto &name : input_names_)
            {
                input_names_ptrs_.push_back(name.c_str());
            }
            for (const auto &name : output_names_)
            {
                output_names_ptrs_.push_back(name.c_str());
            }

            // 根据 DeepFM 模型结构,初始化特征维度
            // 与 C++ 特征提取器保持一致: 13 稀疏 + 8 连续
            num_sparse_features_ = 13; // 广告5 + 用户3 + 上下文5
            num_dense_features_ = 8;   // 广告3 + 用户3 + 上下文2

            initialized_ = true;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to initialize ONNX Runtime engine: " << e.what() << std::endl;
            initialized_ = false;
            return false;
        }
    }

    bool ONNXRuntimeEngine::predict(const FeatureContext &feature_context, InferenceResult &result)
    {
        if (!initialized_)
        {
            result.success = false;
            result.error_message = "Engine not initialized";
            return false;
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        try
        {
            // 准备输入张量
            std::vector<Ort::Value> input_tensors;
            std::vector<int64_t> sparse_buffer;
            std::vector<float> dense_buffer;
            if (!prepare_input_tensors(feature_context, input_tensors, sparse_buffer, dense_buffer))
            {
                result.success = false;
                result.error_message = "Failed to prepare input tensors";
                return false;
            }

            // 执行推理 (使用类成员中稳定的指针)
            Ort::RunOptions run_options;
            auto outputs = session_->Run(
                run_options,
                input_names_ptrs_.data(), // 使用稳定的指针
                input_tensors.data(),
                input_tensors.size(),
                output_names_ptrs_.data(), // 使用稳定的指针
                output_names_ptrs_.size());

            // 提取结果
            if (outputs.size() > 0 && outputs[0].IsTensor())
            {
                auto &output_tensor = outputs[0];
                float *output_data = output_tensor.GetTensorMutableData<float>();
                result.ctr = std::clamp(output_data[0], 0.0f, 1.0f);
                result.confidence = 1.0;
                result.success = true;
            }
            else
            {
                result.success = false;
                result.error_message = "Invalid output tensor";
                return false;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            result.inference_time_ms = std::chrono::duration<double, std::milli>(
                                           end_time - start_time)
                                           .count();

            return true;
        }
        catch (const std::exception &e)
        {
            result.success = false;
            result.error_message = std::string("Inference error: ") + e.what();
            return false;
        }
    }

    bool ONNXRuntimeEngine::predict_batch(
        const std::vector<FeatureContext> &feature_contexts,
        std::vector<InferenceResult> &results)
    {
        if (!initialized_)
        {
            return false;
        }

        results.clear();
        results.resize(feature_contexts.size());

        auto start_time = std::chrono::high_resolution_clock::now();

        try
        {
            // 准备批量输入张量
            std::vector<Ort::Value> input_tensors;
            std::vector<int64_t> sparse_buffer;
            std::vector<float> dense_buffer;

            if (!prepare_batch_input_tensors(feature_contexts, input_tensors, sparse_buffer, dense_buffer))
            {
                for (auto &result : results)
                {
                    result.success = false;
                    result.error_message = "Failed to prepare batch input tensors";
                }
                return false;
            }

            // 执行推理
            Ort::RunOptions run_options;
            auto outputs = session_->Run(
                run_options,
                input_names_ptrs_.data(),
                input_tensors.data(),
                input_tensors.size(),
                output_names_ptrs_.data(),
                output_names_ptrs_.size());

            // 提取批量结果
            if (outputs.size() > 0 && outputs[0].IsTensor())
            {
                auto &output_tensor = outputs[0];
                float *output_data = output_tensor.GetTensorMutableData<float>();
                size_t batch_size = feature_contexts.size();

                for (size_t i = 0; i < batch_size; ++i)
                {
                    results[i].ctr = std::clamp(output_data[i], 0.0f, 1.0f);
                    results[i].confidence = 1.0;
                    results[i].success = true;
                }
            }
            else
            {
                for (auto &result : results)
                {
                    result.success = false;
                    result.error_message = "Invalid output tensor";
                }
                return false;
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            double total_time = std::chrono::duration<double, std::milli>(
                                    end_time - start_time)
                                    .count();

            for (auto &result : results)
            {
                result.inference_time_ms = total_time / results.size();
            }

            return true;
        }
        catch (const std::exception &e)
        {
            for (auto &result : results)
            {
                result.success = false;
                result.error_message = std::string("Batch inference error: ") + e.what();
            }
            return false;
        }
    }

    bool ONNXRuntimeEngine::prepare_input_tensors(
        const FeatureContext &feature_context,
        std::vector<Ort::Value> &input_tensors,
        std::vector<int64_t> &sparse_buffer,
        std::vector<float> &dense_buffer)
    {
        try
        {
            // 创建稀疏特征张量 (batch_size=1, num_sparse_features)
            auto sparse_tensor = create_sparse_tensor(feature_context, sparse_buffer);
            input_tensors.push_back(std::move(sparse_tensor));

            // 创建连续特征张量 (batch_size=1, num_dense_features)
            auto dense_tensor = create_dense_tensor(feature_context, dense_buffer);
            input_tensors.push_back(std::move(dense_tensor));

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to prepare input tensors: " << e.what() << std::endl;
            return false;
        }
    }

    bool ONNXRuntimeEngine::prepare_batch_input_tensors(
        const std::vector<FeatureContext> &feature_contexts,
        std::vector<Ort::Value> &input_tensors,
        std::vector<int64_t> &sparse_buffer,
        std::vector<float> &dense_buffer)
    {
        try
        {
            // 创建批量稀疏特征张量
            auto sparse_tensor = create_batch_sparse_tensor(feature_contexts, sparse_buffer);
            input_tensors.push_back(std::move(sparse_tensor));

            // 创建批量连续特征张量
            auto dense_tensor = create_batch_dense_tensor(feature_contexts, dense_buffer);
            input_tensors.push_back(std::move(dense_tensor));

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to prepare batch input tensors: " << e.what() << std::endl;
            return false;
        }
    }

    Ort::Value ONNXRuntimeEngine::create_sparse_tensor(
        const FeatureContext &feature_context,
        std::vector<int64_t> &buffer)
    {
        // 创建稀疏特征张量 (1, num_sparse_features)
        std::vector<int64_t> shape = {1, num_sparse_features_};
        buffer.assign(num_sparse_features_, 0);

        // 填充稀疏特征 (按照训练数据集的顺序)
        // 训练数据顺序: 广告(5) + 用户(3) + 上下文(5) = 13
        int feature_index = 0;
        std::vector<std::string> feature_names = {
            // 广告特征 (5个)
            "ad_id", "campaign_id", "industry_id", "creative_type", "ad_size",
            // 用户特征 (3个)
            "user_gender", "user_city", "user_device_type",
            // 上下文特征 (5个)
            "day_of_week", "is_weekend", "page_category", "network_type", "carrier"};

        for (const auto &name : feature_names)
        {
            if (feature_index >= num_sparse_features_)
                break;
            int64_t value = feature_context.get_sparse_feature(name, 0);
            buffer[feature_index++] = std::max<int64_t>(0, value);
        }

        // 填充剩余特征为 0
        while (feature_index < num_sparse_features_)
        {
            buffer[feature_index++] = 0;
        }

        return Ort::Value::CreateTensor<int64_t>(
            *memory_info_,
            buffer.data(),
            buffer.size(),
            shape.data(),
            shape.size());
    }

    Ort::Value ONNXRuntimeEngine::create_batch_sparse_tensor(
        const std::vector<FeatureContext> &feature_contexts,
        std::vector<int64_t> &buffer)
    {
        // 创建批量稀疏特征张量 (batch_size, num_sparse_features)
        size_t batch_size = feature_contexts.size();
        std::vector<int64_t> shape = {static_cast<int64_t>(batch_size), num_sparse_features_};
        buffer.assign(batch_size * num_sparse_features_, 0);

        // 训练数据顺序: 广告(5) + 用户(3) + 上下文(5) = 13
        std::vector<std::string> feature_names = {
            // 广告特征 (5个)
            "ad_id", "campaign_id", "industry_id", "creative_type", "ad_size",
            // 用户特征 (3个)
            "user_gender", "user_city", "user_device_type",
            // 上下文特征 (5个)
            "day_of_week", "is_weekend", "page_category", "network_type", "carrier"};

        for (size_t i = 0; i < batch_size; ++i)
        {
            const auto &feature_context = feature_contexts[i];
            size_t offset = i * num_sparse_features_;

            int feature_index = 0;
            for (const auto &name : feature_names)
            {
                if (feature_index >= num_sparse_features_)
                    break;
                int64_t value = feature_context.get_sparse_feature(name, 0);
                buffer[offset + feature_index++] = std::max<int64_t>(0, value);
            }

            while (feature_index < num_sparse_features_)
            {
                buffer[offset + feature_index++] = 0;
            }
        }

        return Ort::Value::CreateTensor<int64_t>(
            *memory_info_,
            buffer.data(),
            buffer.size(),
            shape.data(),
            shape.size());
    }

    Ort::Value ONNXRuntimeEngine::create_dense_tensor(
        const FeatureContext &feature_context,
        std::vector<float> &buffer)
    {
        // 创建连续特征张量 (1, num_dense_features)
        std::vector<int64_t> shape = {1, num_dense_features_};
        buffer.assign(num_dense_features_, 0.0f);

        // 填充连续特征 (按照训练数据集的顺序)
        // 训练数据顺序: 广告(3) + 用户(3) + 上下文(2) = 8
        std::vector<std::string> feature_names = {
            // 广告特征 (3个)
            "ad_hist_ctr", "ad_hist_cvr", "ad_position",
            // 用户特征 (3个)
            "user_age_norm", "user_history_ctr", "user_history_imp",
            // 上下文特征 (2个)
            "hour_norm", "time_slot"};

        int feature_index = 0;
        for (const auto &name : feature_names)
        {
            if (feature_index >= num_dense_features_)
                break;
            float value = feature_context.get_dense_feature(name, 0.0f);
            buffer[feature_index++] = value;
        }

        while (feature_index < num_dense_features_)
        {
            buffer[feature_index++] = 0.0f;
        }

        return Ort::Value::CreateTensor<float>(
            *memory_info_,
            buffer.data(),
            buffer.size(),
            shape.data(),
            shape.size());
    }

    Ort::Value ONNXRuntimeEngine::create_batch_dense_tensor(
        const std::vector<FeatureContext> &feature_contexts,
        std::vector<float> &buffer)
    {
        // 创建批量连续特征张量 (batch_size, num_dense_features)
        size_t batch_size = feature_contexts.size();
        std::vector<int64_t> shape = {static_cast<int64_t>(batch_size), num_dense_features_};
        buffer.assign(batch_size * num_dense_features_, 0.0f);

        // 训练数据顺序: 广告(3) + 用户(3) + 上下文(2) = 8
        std::vector<std::string> feature_names = {
            // 广告特征 (3个)
            "ad_hist_ctr", "ad_hist_cvr", "ad_position",
            // 用户特征 (3个)
            "user_age_norm", "user_history_ctr", "user_history_imp",
            // 上下文特征 (2个)
            "hour_norm", "time_slot"};

        for (size_t i = 0; i < batch_size; ++i)
        {
            const auto &feature_context = feature_contexts[i];
            size_t offset = i * num_dense_features_;

            int feature_index = 0;
            for (const auto &name : feature_names)
            {
                if (feature_index >= num_dense_features_)
                    break;
                float value = feature_context.get_dense_feature(name, 0.0f);
                buffer[offset + feature_index++] = value;
            }

            while (feature_index < num_dense_features_)
            {
                buffer[offset + feature_index++] = 0.0f;
            }
        }

        return Ort::Value::CreateTensor<float>(
            *memory_info_,
            buffer.data(),
            buffer.size(),
            shape.data(),
            shape.size());
    }

} // namespace ctr
