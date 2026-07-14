#include "CsvReader.hpp"
#include "ExactSupportTracker.hpp"
#include "L0Sampler.hpp"
#include "HashBasedL0Sampler.hpp"
#include "SamplerStatus.hpp"
#include "SamplerConfig.hpp"


#include <iostream>
#include <string>
#include <algorithm>
#include <exception>
#include <vector>
#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <optional>


int main(int argc, char* argv[]){
    if(argc < 2){
        std::cerr
            << "Usage: l0_sampler <path_to_csv> [trials] [output_csv] "
            << "[baseline|hash] "
            << "[--levels N] "
            << "[--sparsity N] "
            << "[--rows N] "
            << "[--buckets N] "
            << "[--degree N] "
            << "[--seed N] "
            << "[--recovery greedy|fixed] "
            << "[--fixed-level N]\n";
        return 1;
    }

    std::string path = argv[1];

    int trials = 1;
    if (argc >= 3)
    {
        trials = std::stoi(argv[2]);
    }
    
    if (trials <= 0)
    {
        std::cerr << "Error: trials must be positive. \n";
        return 1;
    }

    std::string output_path;

    if (argc >= 4)
    {
        output_path = argv[3];
    }
    
    std::string sampler_type = "baseline";

    if (argc >= 5) {
        sampler_type = argv[4];
    }

    if (sampler_type != "baseline" && sampler_type != "hash") {
        std::cerr << "Error: sampler type must be either 'baseline' or 'hash'.\n";
        return 1;
    }

    try
    {
        SamplerConfig base_config;

        for (int arg_index = 5; arg_index < argc; ++arg_index) {
            const std::string argument = argv[arg_index];

            auto require_value = [&](const std::string& option) -> std::string {
                if (arg_index + 1 >= argc) {
                    throw std::invalid_argument(
                        "Missing value for argument: " + option
                    );
                }

                ++arg_index;
                return argv[arg_index];
            };

            if (argument == "--levels") {
                base_config.num_levels =
                    static_cast<std::size_t>(
                        std::stoull(require_value(argument))
                    );
            } else if (argument == "--sparsity") {
                base_config.sparsity =
                    static_cast<std::size_t>(
                        std::stoull(require_value(argument))
                    );
            } else if (argument == "--rows") {
                base_config.recovery_rows =
                    static_cast<std::size_t>(
                        std::stoull(require_value(argument))
                    );
            } else if (argument == "--buckets") {
                base_config.recovery_buckets =
                    static_cast<std::size_t>(
                        std::stoull(require_value(argument))
                    );
            } else if (argument == "--degree") {
                base_config.polynomial_degree =
                    static_cast<std::size_t>(
                        std::stoull(require_value(argument))
                    );
            } else if (argument == "--seed") {
                base_config.seed =
                    static_cast<std::uint64_t>(
                        std::stoull(require_value(argument))
                    );
            } else if (argument == "--recovery") {
                const std::string mode = require_value(argument);

                if (mode == "greedy") {
                    base_config.recovery_mode = RecoveryMode::Greedy;
                } else if (mode == "fixed") {
                    base_config.recovery_mode = RecoveryMode::FixedLevel;
                } else {
                    throw std::invalid_argument(
                        "Recovery mode must be either 'greedy' or 'fixed'."
                    );
                }
            } else if (argument == "--fixed-level") {
                base_config.fixed_level =
                    static_cast<std::size_t>(
                        std::stoull(require_value(argument))
                    );
            } else {
                throw std::invalid_argument(
                    "Unknown argument: " + argument
                );
            };
        }

        base_config.validate();

        const std::string recovery_mode_string =
            base_config.recovery_mode == RecoveryMode::Greedy
                ? "greedy"
                : "fixed";

        std::vector<StreamUpdate> updates =  read_updates_from_csv(path);

        ExactSupportTracker tracker;

        for(const auto& update : updates){
            tracker.update(update);
        }


        std::vector<std::int64_t> support = tracker.final_support();
        std::sort(support.begin(), support.end());
                
        std::ofstream output_file;
        if (!output_path.empty())
        {
            output_file.open(output_path);

            if (!output_file.is_open())
            {
                throw std::runtime_error("Could not open output file: " + output_path);
            }
            
            output_file
                << "trial,sample,status,"
                << "num_levels,sparsity,recovery_rows,recovery_buckets,"
                << "polynomial_degree,base_seed,trial_seed,"
                << "recovery_mode,fixed_level\n";
        }
        
        std::unordered_map<std::int64_t, int> sampler_counts;
        int invalid_samples = 0;
        int valid_sample = 0;
        int failures = 0;

        for (int i = 0; i < trials; i++)
        {
            std::optional<std::int64_t> sample_item;
            SampleStatus sample_status = SampleStatus::NoRecoverableLevel;

            const std::uint64_t trial_seed =
                base_config.seed + static_cast<std::uint64_t>(i);

            if (sampler_type == "hash") {
                SamplerConfig trial_config = base_config;
                trial_config.seed = trial_seed;

                HashBasedL0Sampler sampler(trial_config);

                for (const auto& update : updates) {
                    sampler.update(update.item_id, update.delta);
                }

                SampleResult sample = sampler.sample();
                sample_item = sample.item;
                sample_status = sample.status;
            } else {
                L0Sampler sampler(trial_seed);

                for (const auto& update : updates) {
                    sampler.update(update);
                }

                sample_item = sampler.sample();

                if (sample_item.has_value())
                {
                    sample_status = SampleStatus::Success;
                }else{
                    sample_status = SampleStatus::EmptySupport;
                }
                
            }

            if (!sample_item.has_value())
            {
                failures ++;

                if (output_file.is_open()){
                    output_file
                        << i << ",,"
                        << to_string(sample_status) << ","
                        << base_config.num_levels << ","
                        << base_config.sparsity << ","
                        << base_config.recovery_rows << ","
                        << base_config.recovery_buckets << ","
                        << base_config.polynomial_degree << ","
                        << base_config.seed << ","
                        << trial_seed << ","
                        << recovery_mode_string << ","
                        << base_config.fixed_level <<"\n";
                }
                continue;
            }

            std::int64_t item = sample_item.value();

            if (tracker.contains(item))
            {
                valid_sample++;
                sampler_counts[item]++;

                if (output_file.is_open())
                {
                    output_file
                        << i << ","
                        << item << ","
                        << to_string(sample_status) << ","
                        << base_config.num_levels << ","
                        << base_config.sparsity << ","
                        << base_config.recovery_rows << ","
                        << base_config.recovery_buckets << ","
                        << base_config.polynomial_degree << ","
                        << base_config.seed << ","
                        << trial_seed << ","
                        << recovery_mode_string << ","
                        << base_config.fixed_level <<"\n";
                }
                

            } else{
                invalid_samples ++;
                sample_status = SampleStatus::InvalidSample;

                if (output_file.is_open())
                {
                    output_file
                        << i << ","
                        << item << ","
                        << to_string(sample_status) << ","
                        << base_config.num_levels << ","
                        << base_config.sparsity << ","
                        << base_config.recovery_rows << ","
                        << base_config.recovery_buckets << ","
                        << base_config.polynomial_degree << ","
                        << base_config.seed << ","
                        << trial_seed << ","
                        << recovery_mode_string << ","
                        << base_config.fixed_level <<"\n";
                }
                
            }

        }

        std::cout << "Input file: " << path << "\n";
        std::cout << "Sampler type: " << sampler_type << "\n";

        if (sampler_type == "hash") {
            std::cout << "Sampler configuration:\n";
            std::cout
                << " Levels: "
                << base_config.num_levels
                << "\n";
            std::cout
                << " Sparsity: "
                << base_config.sparsity
                << "\n";
            std::cout
                << " Recovery rows: "
                << base_config.recovery_rows
                << "\n";
            std::cout
                << " Recovery buckets: "
                << base_config.recovery_buckets
                << "\n";
            std::cout
                << " Polynomial degree: "
                << base_config.polynomial_degree
                << "\n";
            std::cout
                << " Base seed: "
                << base_config.seed
                << "\n";
            std::cout
                << " Recovery mode: "
                << (
                    base_config.recovery_mode == RecoveryMode::Greedy
                        ? "greedy"
                        : "fixed"
                )
                << "\n";

            if (base_config.recovery_mode == RecoveryMode::FixedLevel) {
                std::cout
                    << " Fixed level: "
                    << base_config.fixed_level
                    << "\n";
            }
        }
        std::cout << "Number of Updates: " << updates.size() << "\n";
        std::cout << "Final support size: " << tracker.support_size() << "\n";

        std::cout << "Final support items: \n";
        for (int64_t item_id : support)
        {
            std::cout << " " << item_id << "\n";
        }
        
        
        std::cout << "\nTrials: " << trials << "\n";
        std::cout << "Valid samples: " << valid_sample << "\n";
        std::cout << "Invalid samples: " << invalid_samples << "\n";
        std::cout << "Failures: " << failures << "\n";
        

        std::cout << "\nSample counts:\n";
        for(std::int64_t item_id : support){
            std::cout << " " << item_id << ": " << sampler_counts[item_id] << "\n";
        }

        if(!output_path.empty()){
            std::cout << "\nTrial-level results written to: " << output_path << "\n";
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    
    return 0;
}
