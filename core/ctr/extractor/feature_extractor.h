#pragma once

#include "../core/feature_context.h"
#include "../core/feature_spec.h"
#include <memory>

namespace ctr {

/**
 * @brief 特征提取器接口
 *
 * 从 OpenRTB 请求中提取特征,用于 CTR 预估
 */
class IFeatureExtractor {
public:
    virtual ~IFeatureExtractor() = default;

    /**
     * @brief 从请求中提取特征
     * @param request_data 请求数据 (JSON 字符串或自定义结构)
     * @param feature_context [out] 输出的特征上下文
     * @return 是否成功
     */
    virtual bool extract(const std::string& request_data, FeatureContext& feature_context) = 0;

    /**
     * @brief 获取该提取器支持的特征规范
     * @return 特征规范
     */
    virtual FeatureSpec get_feature_spec() const = 0;

    /**
     * @brief 获取提取器名称
     */
    virtual std::string get_name() const = 0;
};

using FeatureExtractorPtr = std::shared_ptr<IFeatureExtractor>;

} // namespace ctr
