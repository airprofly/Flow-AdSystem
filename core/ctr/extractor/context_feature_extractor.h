#pragma once

#include "feature_extractor.h"
#include <string>
#include <ctime>

namespace ctr {

/**
 * @brief 上下文特征提取器
 *
 * 提取上下文相关的特征:
 * - 时间特征 (小时、星期几、是否周末)
 * - 页面/位置信息 (类别)
 * - 网络类型 (类别)
 * - 运营商 (类别)
 */
class ContextFeatureExtractor : public IFeatureExtractor {
public:
    ContextFeatureExtractor();
    ~ContextFeatureExtractor() override = default;

    /**
     * @brief 从请求中提取上下文特征
     * @param request_data 请求数据 (JSON 字符串)
     * @param feature_context [out] 输出的特征上下文
     * @return 是否成功
     */
    bool extract(const std::string& request_data, FeatureContext& feature_context) override;

    /**
     * @brief 获取支持的特征规范
     */
    FeatureSpec get_feature_spec() const override;

    /**
     * @brief 获取提取器名称
     */
    std::string get_name() const override { return "ContextFeatureExtractor"; }

private:
    /**
     * @brief 提取时间特征
     * @param timestamp Unix 时间戳
     * @param feature_context [out] 特征上下文
     */
    void extract_time_features(int64_t timestamp, FeatureContext& feature_context) const;

    /**
     * @brief 归一化小时到 [0, 1]
     * @param hour 小时 (0-23)
     * @return 归一化后的值
     */
    float normalize_hour(int hour) const;

    FeatureSpec feature_spec_;  // 支持的特征规范
};

} // namespace ctr
