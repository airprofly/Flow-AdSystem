#include "core/ctr/ctr_manager.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

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

    double Percentile(std::vector<double> values, double p)
    {
        if (values.empty())
        {
            return 0.0;
        }
        std::sort(values.begin(), values.end());
        const double index = (p / 100.0) * static_cast<double>(values.size() - 1);
        const auto lower = static_cast<size_t>(index);
        const auto upper = std::min(lower + 1, values.size() - 1);
        const double frac = index - static_cast<double>(lower);
        return values[lower] + (values[upper] - values[lower]) * frac;
    }

} // namespace

int main(int argc, char** argv)
{
    int warmup_rounds = 50;
    int benchmark_rounds = 2000;
    int threads = 1;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--warmup" && i + 1 < argc)
        {
            warmup_rounds = std::max(0, std::stoi(argv[++i]));
        }
        else if (arg == "--rounds" && i + 1 < argc)
        {
            benchmark_rounds = std::max(1, std::stoi(argv[++i]));
        }
        else if (arg == "--threads" && i + 1 < argc)
        {
            threads = std::max(1, std::stoi(argv[++i]));
        }
    }

    const std::string model_path = std::string(FLOW_ADSYSTEM_SOURCE_DIR) + "/models/deep_fm.onnx";
    if (!std::filesystem::exists(model_path))
    {
        std::cerr << "Model not found: " << model_path << std::endl;
        return 1;
    }

    ctr::CTRManager manager;
    if (!manager.initialize())
    {
        std::cerr << "Failed to initialize CTRManager" << std::endl;
        return 1;
    }

    ctr::ModelConfig config;
    config.model_name = "deep_fm_v1";
    config.model_path = model_path;
    config.traffic_fraction = 1.0f;
    if (!manager.add_model(config))
    {
        std::cerr << "Failed to add model" << std::endl;
        return 1;
    }

    const std::string request = BuildRequestJson();
    const std::string ad = BuildAdJson();

    for (int i = 0; i < warmup_rounds; ++i)
    {
        ctr::InferenceResult warmup_result;
        if (!manager.predict_ctr(request, ad, warmup_result) || !warmup_result.success)
        {
            std::cerr << "Warmup failed: " << warmup_result.error_message << std::endl;
            return 1;
        }
    }

    std::vector<double> latencies_ms;
    latencies_ms.reserve(benchmark_rounds);

    const auto start = std::chrono::steady_clock::now();
    if (threads == 1)
    {
        for (int i = 0; i < benchmark_rounds; ++i)
        {
            ctr::InferenceResult result;
            if (!manager.predict_ctr(request, ad, result) || !result.success)
            {
                std::cerr << "Predict failed: " << result.error_message << std::endl;
                return 1;
            }
            latencies_ms.push_back(result.inference_time_ms);
        }
    }
    else
    {
        std::vector<std::thread> workers;
        std::mutex merge_mutex;
        std::atomic<int> failed{0};
        workers.reserve(threads);

        const int base_rounds = benchmark_rounds / threads;
        const int remainder = benchmark_rounds % threads;

        for (int worker_id = 0; worker_id < threads; ++worker_id)
        {
            const int worker_rounds = base_rounds + (worker_id < remainder ? 1 : 0);
            workers.emplace_back([&, worker_rounds]() {
                std::vector<double> local_latencies;
                local_latencies.reserve(worker_rounds);
                for (int i = 0; i < worker_rounds; ++i)
                {
                    ctr::InferenceResult result;
                    if (!manager.predict_ctr(request, ad, result) || !result.success)
                    {
                        failed.fetch_add(1);
                        return;
                    }
                    local_latencies.push_back(result.inference_time_ms);
                }

                std::lock_guard<std::mutex> lock(merge_mutex);
                latencies_ms.insert(latencies_ms.end(), local_latencies.begin(), local_latencies.end());
            });
        }

        for (auto& worker : workers)
        {
            worker.join();
        }

        if (failed.load() > 0)
        {
            std::cerr << "Predict failed in " << failed.load() << " worker(s)" << std::endl;
            return 1;
        }
    }
    const auto end = std::chrono::steady_clock::now();

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    const double total_ms = static_cast<double>(std::max<int64_t>(elapsed_ms, 1));
    const double qps = (benchmark_rounds * 1000.0) / total_ms;

    const double p50 = Percentile(latencies_ms, 50.0);
    const double p95 = Percentile(latencies_ms, 95.0);
    const double p99 = Percentile(latencies_ms, 99.0);
    const double avg = std::accumulate(latencies_ms.begin(), latencies_ms.end(), 0.0) /
                       static_cast<double>(latencies_ms.size());

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "CTR Latency Benchmark\n";
    std::cout << "rounds=" << benchmark_rounds
              << ", warmup=" << warmup_rounds
              << ", threads=" << threads << "\n";
    std::cout << "avg_ms=" << avg << ", p50_ms=" << p50
              << ", p95_ms=" << p95 << ", p99_ms=" << p99 << "\n";
    std::cout << std::setprecision(2);
    std::cout << "qps=" << qps << "\n";

    return 0;
}
