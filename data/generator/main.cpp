// Author: airprofly

#include "ad_generator.h"
#include <chrono>
#include <iostream>
#include <string>

using namespace flow_ad;

void print_usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [options]\n"
              << "\nOptions:\n"
              << "  --config <file>                Load configuration from JSON file\n"
              << "  -n, --num-ads <N>              Number of ads to generate (default: 100000)\n"
              << "  -c, --num-campaigns <N>         Number of campaigns (default: 10000)\n"
              << "  -r, --num-creatives <N>         Number of creatives (default: 50000)\n"
              << "  -a, --num-advertisers <N>       Number of advertisers (default: 1000)\n"
              << "  -o, --output <file>             Output file path (default: ads_data.json)\n"
              << "  -s, --seed <N>                  Random seed for reproducibility (default: 42)\n"
              << "  -h, --help                      Show this help message\n"
              << "\nExample:\n"
              << "  " << program_name << " --config config/generator_config.json\n"
              << "  " << program_name << " -n 10000 -s 123 -o test_ads.json\n"
              << std::endl;
}

int main(int argc, char *argv[])
{
    try
    {
        // 默认配置
        GeneratorConfig config;
        std::string output_file = "./data/data/ads_data.json";

        // 解析命令行参数
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help")
            {
                print_usage(argv[0]);
                return 0;
            }
            else if (arg == "-n" || arg == "--num-ads")
            {
                if (i + 1 < argc)
                {
                    config.num_ads = std::stoull(argv[++i]);
                }
            }
            else if (arg == "-c" || arg == "--num-campaigns")
            {
                if (i + 1 < argc)
                {
                    config.num_campaigns = std::stoull(argv[++i]);
                }
            }
            else if (arg == "-r" || arg == "--num-creatives")
            {
                if (i + 1 < argc)
                {
                    config.num_creatives = std::stoull(argv[++i]);
                }
            }
            else if (arg == "-a" || arg == "--num-advertisers")
            {
                if (i + 1 < argc)
                {
                    config.num_advertisers = std::stoull(argv[++i]);
                }
            }
            else if (arg == "-o" || arg == "--output")
            {
                if (i + 1 < argc)
                {
                    output_file = argv[++i];
                }
            }
            else if (arg == "-s" || arg == "--seed")
            {
                if (i + 1 < argc)
                {
                    config.random_seed = static_cast<unsigned int>(std::stoul(argv[++i]));
                }
            }
            else if (arg == "--min-bid")
            {
                if (i + 1 < argc)
                {
                    config.min_bid_price = std::stod(argv[++i]);
                }
            }
            else if (arg == "--max-bid")
            {
                if (i + 1 < argc)
                {
                    config.max_bid_price = std::stod(argv[++i]);
                }
            }
            else if (arg == "--min-budget")
            {
                if (i + 1 < argc)
                {
                    config.min_budget = std::stod(argv[++i]);
                }
            }
            else if (arg == "--max-budget")
            {
                if (i + 1 < argc)
                {
                    config.max_budget = std::stod(argv[++i]);
                }
            }
        }

        // 打印配置
        std::cout << "========================================" << std::endl;
        std::cout << "   Ad Data Generator" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Advertisers:   " << config.num_advertisers << std::endl;
        std::cout << "  Campaigns:     " << config.num_campaigns << std::endl;
        std::cout << "  Creatives:     " << config.num_creatives << std::endl;
        std::cout << "  Ads:           " << config.num_ads << std::endl;
        std::cout << "  Bid range:     " << config.min_bid_price
                  << " - " << config.max_bid_price << std::endl;
        std::cout << "  Budget range:  " << config.min_budget
                  << " - " << config.max_budget << std::endl;
        std::cout << "  Random seed:   " << config.random_seed << std::endl;
        std::cout << "  Output file:   " << output_file << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;

        // 创建生成器
        AdDataGenerator generator(config);

        // 生成数据
        std::cout << "Generating data..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        generator.generate_all();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Data generation completed in " << duration.count() << " ms" << std::endl;
        std::cout << std::endl;

        // 保存到文件
        std::cout << "Saving to file: " << output_file << std::endl;
        if (generator.save_to_file(output_file))
        {
            std::cout << "Successfully saved!" << std::endl;
        }
        else
        {
            std::cerr << "Failed to save file" << std::endl;
            return 1;
        }

        // 打印统计信息
        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "   Statistics" << std::endl;
        std::cout << "========================================" << std::endl;

        const auto &ads = generator.ads();
        const auto &campaigns = generator.campaigns();
        const auto &creatives = generator.creatives();

        // 计算出价分布
        double total_bid = 0.0;
        double min_bid = 1e9;
        double max_bid = 0.0;
        for (const auto &ad : ads)
        {
            total_bid += ad.bid_price;
            min_bid = std::min(min_bid, ad.bid_price);
            max_bid = std::max(max_bid, ad.bid_price);
        }

        // 计算预算分布
        double total_budget = 0.0;
        for (const auto &campaign : campaigns)
        {
            total_budget += campaign.budget;
        }

        std::cout << "Total ads:         " << ads.size() << std::endl;
        std::cout << "Total campaigns:   " << campaigns.size() << std::endl;
        std::cout << "Total creatives:   " << creatives.size() << std::endl;
        std::cout << std::endl;
        std::cout << "Bid price stats:" << std::endl;
        std::cout << "  Average:   " << (total_bid / ads.size()) << std::endl;
        std::cout << "  Min:       " << min_bid << std::endl;
        std::cout << "  Max:       " << max_bid << std::endl;
        std::cout << std::endl;
        std::cout << "Budget stats:" << std::endl;
        std::cout << "  Total:     " << total_budget << std::endl;
        std::cout << "  Average:   " << (total_budget / campaigns.size()) << std::endl;
        std::cout << "========================================" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
