#pragma once

#include "feature_extractor.h"
#include <string>

namespace ctr {

/**
 * @brief 广告特征提取器
 *
 * 提取广告相关的特征:
 * - 广告ID (类别)
 * - 广告活动ID (类别)
 * - 广告行业/类别 (类别)
 * - 创意类型 (类别)
 * - 广告尺寸 (类别)
 * - 广告历史表现 (数值: CTR, CVR等)
 */
class AdFeatureExtractor : public IFeatureExtractor {
public:
    AdFeatureExtractor();
    ~AdFeatureExtractor() override = default;

    /**
     * @brief 从广告信息中提取特征
     * @param ad_data 广告数据 (JSON 字符串,包含 ad_id, campaign_id 等)
     * @param feature_context [out] 输出的特征上下文
     * @return 是否成功
     */
    bool extract(const std::string& ad_data, FeatureContext& feature_context) override;

    /**
     * @brief 获取支持的特征规范
     */
    FeatureSpec get_feature_spec() const override;

    /**
     * @brief 获取提取器名称
     */
    std::string get_name() const override { return "AdFeatureExtractor"; }

private:
    /**
     * @brief Hash 函数: 将字符串转换为整数ID
     * @param str 输入字符串
     * @return Hash 值 (正整数)
     */
    int64_t hash_string(const std::string& str) const;

    FeatureSpec feature_spec_;  // 支持的特征规范
};

} // namespace ctr
