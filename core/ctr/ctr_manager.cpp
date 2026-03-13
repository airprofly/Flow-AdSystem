// Author: airprofly

#include "ctr_manager.h"
#include "engine/onnx_runtime_engine.h"
#include "extractor/ad_feature_extractor.h"
#include "extractor/context_feature_extractor.h"
#include "extractor/user_feature_extractor.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace ctr
{

    CTRManager::CTRManager()
        : total_traffic_fraction_(0.0f), rng_(std::random_device{}()), dist_(0.0f, 1.0f)
    {
    }

    bool CTRManager::initialize()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        try
        {
            // 添加默认的特征提取器
            add_feature_extractor(std::make_shared<UserFeatureExtractor>());
            add_feature_extractor(std::make_shared<AdFeatureExtractor>());
            add_feature_extractor(std::make_shared<ContextFeatureExtractor>());

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to initialize CTRManager: " << e.what() << std::endl;
            return false;
        }
    }

    void CTRManager::add_feature_extractor(FeatureExtractorPtr extractor)
    {
        if (extractor)
        {
            feature_extractors_.push_back(extractor);
        }
    }

    bool CTRManager::add_model(const ModelConfig &config)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        try
        {
            // 创建 ONNX Runtime 引擎
            auto engine = std::make_shared<ONNXRuntimeEngine>();

            if (!engine->initialize(config))
            {
                std::cerr << "Failed to initialize engine for model: " << config.model_name << std::endl;
                return false;
            }

            // 添加模型
            ModelEntry entry;
            entry.config = config;
            entry.engine = engine;
            entry.total_predictions = 0;
            entry.total_inference_time_ms = 0.0;

            auto it = models_.find(config.model_name);
            if (it != models_.end() && it->second.config.enabled)
            {
                total_traffic_fraction_ -= it->second.config.traffic_fraction;
                if (total_traffic_fraction_ < 0.0f)
                {
                    total_traffic_fraction_ = 0.0f;
                }
            }

            models_[config.model_name] = entry;
            if (config.enabled)
            {
                total_traffic_fraction_ += config.traffic_fraction;
            }

            std::cout << "Successfully added model: " << config.model_name
                      << " (traffic: " << config.traffic_fraction << ")" << std::endl;

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to add model: " << e.what() << std::endl;
            return false;
        }
    }

    bool CTRManager::predict_ctr(
        const std::string &request_data,
        const std::string &ad_data,
        InferenceResult &result)
    {
        // 提取特征
        FeatureContext feature_context;
        if (!extract_features(request_data, ad_data, feature_context))
        {
            result.success = false;
            result.error_message = "Failed to extract features";
            return false;
        }

        // 选择推理引擎
        auto engine = select_inference_engine();
        if (!engine)
        {
            result.success = false;
            result.error_message = "No available inference engine";
            return false;
        }

        // 执行推理
        if (!engine->predict(feature_context, result))
        {
            return false;
        }

        // 更新统计信息
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string &model_name = engine->get_model_config().model_name;
        if (models_.find(model_name) != models_.end())
        {
            auto &entry = models_[model_name];
            entry.total_predictions++;
            entry.total_inference_time_ms += result.inference_time_ms;
        }

        return true;
    }

    bool CTRManager::predict_ctr_batch(
        const std::vector<std::string> &request_data_list,
        const std::vector<std::string> &ad_data_list,
        std::vector<InferenceResult> &results)
    {
        if (request_data_list.size() != ad_data_list.size())
        {
            return false;
        }

        results.clear();
        results.resize(request_data_list.size());

        // 提取批量特征
        std::vector<FeatureContext> feature_contexts(request_data_list.size());
        for (size_t i = 0; i < request_data_list.size(); ++i)
        {
            if (!extract_features(request_data_list[i], ad_data_list[i], feature_contexts[i]))
            {
                results[i].success = false;
                results[i].error_message = "Failed to extract features";
                return false;
            }
        }

        // 选择推理引擎
        auto engine = select_inference_engine();
        if (!engine)
        {
            for (auto &result : results)
            {
                result.success = false;
                result.error_message = "No available inference engine";
            }
            return false;
        }

        // 执行批量推理
        if (!engine->predict_batch(feature_contexts, results))
        {
            return false;
        }

        // 更新统计信息
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string &model_name = engine->get_model_config().model_name;
        if (models_.find(model_name) != models_.end())
        {
            auto &entry = models_[model_name];
            entry.total_predictions += results.size();
            for (const auto &result : results)
            {
                entry.total_inference_time_ms += result.inference_time_ms;
            }
        }

        return true;
    }

    bool CTRManager::reload_model(const std::string &model_name, const std::string &new_model_path)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = models_.find(model_name);
        if (it == models_.end())
        {
            std::cerr << "Model not found: " << model_name << std::endl;
            return false;
        }

        try
        {
            // 创建新引擎
            auto new_engine = std::make_shared<ONNXRuntimeEngine>();
            ModelConfig new_config = it->second.config;
            new_config.model_path = new_model_path;

            if (!new_engine->initialize(new_config))
            {
                std::cerr << "Failed to initialize new model: " << new_model_path << std::endl;
                return false;
            }

            // 替换引擎
            it->second.engine = new_engine;
            it->second.config = new_config;

            std::cout << "Successfully reloaded model: " << model_name << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to reload model: " << e.what() << std::endl;
            return false;
        }
    }

    bool CTRManager::set_model_enabled(const std::string &model_name, bool enabled)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = models_.find(model_name);
        if (it == models_.end())
        {
            return false;
        }

        it->second.config.enabled = enabled;

        // 重新计算总流量
        total_traffic_fraction_ = 0.0f;
        for (const auto &kv : models_)
        {
            if (kv.second.config.enabled)
            {
                total_traffic_fraction_ += kv.second.config.traffic_fraction;
            }
        }

        return true;
    }

    bool CTRManager::update_model_traffic(const std::string &model_name, float traffic_fraction)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = models_.find(model_name);
        if (it == models_.end())
        {
            return false;
        }

        it->second.config.traffic_fraction = traffic_fraction;

        // 重新计算总流量
        total_traffic_fraction_ = 0.0f;
        for (const auto &kv : models_)
        {
            if (kv.second.config.enabled)
            {
                total_traffic_fraction_ += kv.second.config.traffic_fraction;
            }
        }

        return true;
    }

    std::vector<CTRManager::ModelStats> CTRManager::get_model_stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<ModelStats> stats;
        for (const auto &kv : models_)
        {
            ModelStats stat;
            stat.model_name = kv.second.config.model_name;
            stat.total_predictions = kv.second.total_predictions;
            stat.current_traffic_fraction = kv.second.config.traffic_fraction;

            if (kv.second.total_predictions > 0)
            {
                stat.avg_inference_time_ms = kv.second.total_inference_time_ms / kv.second.total_predictions;
            }
            else
            {
                stat.avg_inference_time_ms = 0.0;
            }

            stats.push_back(stat);
        }

        return stats;
    }

    ModelInferenceEnginePtr CTRManager::select_inference_engine()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (models_.empty())
        {
            return nullptr;
        }

        // 如果总流量不可用，回退到第一个启用模型
        if (total_traffic_fraction_ <= 0.0f)
        {
            for (auto &kv : models_)
            {
                if (kv.second.config.enabled)
                {
                    return kv.second.engine;
                }
            }
            return nullptr;
        }

        // 流量分配
        float random_value = dist_(rng_);
        float cumulative_traffic = 0.0f;

        for (auto &kv : models_)
        {
            if (!kv.second.config.enabled)
            {
                continue;
            }

            cumulative_traffic += kv.second.config.traffic_fraction;
            if (random_value <= cumulative_traffic / total_traffic_fraction_)
            {
                return kv.second.engine;
            }
        }

        // 如果没有匹配的,返回第一个启用的模型
        for (auto &kv : models_)
        {
            if (kv.second.config.enabled)
            {
                return kv.second.engine;
            }
        }

        return nullptr;
    }

    bool CTRManager::extract_features(
        const std::string &request_data,
        const std::string &ad_data,
        FeatureContext &feature_context)
    {
        try
        {
            bool has_successful_extraction = false;

            // 使用所有特征提取器提取特征
            for (const auto &extractor : feature_extractors_)
            {
                // 根据 extractor 类型选择提取的数据源
                std::string data_source;
                if (extractor->get_name() == "AdFeatureExtractor")
                {
                    data_source = ad_data;
                }
                else
                {
                    data_source = request_data;
                }

                if (!extractor->extract(data_source, feature_context))
                {
                    std::cerr << "Warning: " << extractor->get_name()
                              << " failed to extract features" << std::endl;
                    // 继续尝试其他提取器
                }
                else
                {
                    has_successful_extraction = true;
                }
            }

            return has_successful_extraction;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to extract features: " << e.what() << std::endl;
            return false;
        }
    }

} // namespace ctr
