#include "CsvReader.hpp"
#include "ExactSupportTracker.hpp"
#include "L0Sampler.hpp"
#include "HashBasedL0Sampler.hpp"
#include "SamplerStatus.hpp"

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
        std::cerr << "Usage: l0_sampler <path_to_csv> [trials] [output_csv] [baseline|hash]\n";
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
            
            output_file << "trial,sample,status\n";
        }
        
        std::unordered_map<std::int64_t, int> sampler_counts;
        int invalid_samples = 0;
        int valid_sample = 0;
        int failures = 0;

        for (int i = 0; i < trials; i++)
        {
            std::optional<std::int64_t> sample_item;
            SampleStatus sample_status = SampleStatus::NoRecoverableLevel;

            if (sampler_type == "hash") {
                HashBasedL0Sampler sampler(32, static_cast<std::uint64_t>(123 + i));

                for (const auto& update : updates) {
                    sampler.update(update.item_id, update.delta);
                }

                SampleResult sample = sampler.sample();
                sample_item = sample.item;
                sample_status = sample.status;
            } else {
                L0Sampler sampler(123 + i);

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
                    output_file << i << ",," << to_string(sample_status) << "\n";
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
                    output_file << i << "," << item << "," << to_string(sample_status) << "\n";
                }
                

            } else{
                invalid_samples ++;
                sample_status = SampleStatus::InvalidSample;

                if (output_file.is_open())
                {
                    output_file << i << "," << item << "," << to_string(sample_status) << "\n";
                }
                
            }

        }

        std::cout << "Input file: " << path << "\n";
        std::cout << "Sampler type: " << sampler_type << "\n";
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
