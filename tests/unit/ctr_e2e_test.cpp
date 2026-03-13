#include <gtest/gtest.h>

#include "core/ctr/ctr_manager.h"

#include <filesystem>
#include <string>

#ifndef FLOW_ADSYSTEM_SOURCE_DIR
#define FLOW_ADSYSTEM_SOURCE_DIR "."
#endif

namespace
{

    std::string BuildRequestJson()
    {
        return R"({
        "timestamp": 1678886400,
        "user": {
            "id": "user_12345",
            "gender": "M",
            "age": 35,
            "city": 1001,
            "history_ctr": 0.05,
            "history_impressions": 1000,
            "device": {
                "type": "mobile"
            }
        },
        "device": {
            "network_type": 4,
            "carrier": 1
        },
        "site": {
            "category": 101
        }
    })";
    }

    std::string BuildAdJson()
    {
        return R"({
        "id": "ad_67890",
        "campaign_id": "campaign_100",
        "industry": 25,
        "creative_type": 1,
        "size": "320x50",
        "hist_ctr": 0.08,
        "hist_cvr": 0.02,
        "position": 1
    })";
    }

} // namespace

TEST(CTRE2ETest, PredictEndToEnd)
{
    const std::string model_path = std::string(FLOW_ADSYSTEM_SOURCE_DIR) + "/models/deep_fm.onnx";
    if (!std::filesystem::exists(model_path))
    {
        return;
    }

    ctr::CTRManager manager;
    ASSERT_TRUE(manager.initialize());

    ctr::ModelConfig config;
    config.model_name = "deep_fm_v1";
    config.model_path = model_path;
    config.traffic_fraction = 1.0f;

    ASSERT_TRUE(manager.add_model(config));

    ctr::InferenceResult result;
    ASSERT_TRUE(manager.predict_ctr(BuildRequestJson(), BuildAdJson(), result));
    ASSERT_TRUE(result.success);
    EXPECT_GE(result.ctr, 0.0f);
    EXPECT_LE(result.ctr, 1.0f);
    EXPECT_GT(result.inference_time_ms, 0.0);
}
