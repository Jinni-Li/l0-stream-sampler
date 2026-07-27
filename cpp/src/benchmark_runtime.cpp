#include "CsvReader.hpp"
#include "ExactSupportTracker.hpp"
#include "HashBasedL0Sampler.hpp"
#include "L0Sampler.hpp"
#include "SamplerConfig.hpp"
#include "SamplerStatus.hpp"

#include <filesystem>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct TimingRow {
    std::size_t trial;
    std::string sampler;
    std::uint64_t seed;
    std::size_t num_updates;
    std::size_t support_size;
    std::string sample_status;
    std::int64_t update_time_ns;
    double update_time_per_update_ns;
    std::int64_t recovery_time_ns;
};

std::int64_t elapsed_nanoseconds(const Clock::time_point& start,const Clock::time_point& end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

TimingRow benchmark_baseline(const std::vector<StreamUpdate>& updates, std::size_t support_size,
    std::size_t trial, std::uint64_t seed) {
    // Sampler construction is intentionally excluded.
    L0Sampler sampler(seed);

    const auto update_start = Clock::now();

    for (const StreamUpdate& update : updates) {
        sampler.update(update);
    }

    const auto update_end = Clock::now();

    const auto recovery_start = Clock::now();
    const std::optional<std::int64_t> sample = sampler.sample();
    const auto recovery_end = Clock::now();

    const std::int64_t update_time_ns = elapsed_nanoseconds(update_start, update_end);

    const std::int64_t recovery_time_ns = elapsed_nanoseconds(recovery_start, recovery_end);

    const double update_time_per_update_ns = updates.empty() ? 0.0 
    : static_cast<double>(update_time_ns) / static_cast<double>(updates.size());

    return TimingRow{
        trial,
        "baseline",
        seed,
        updates.size(),
        support_size,
        sample.has_value() ? "success" : "empty_support",
        update_time_ns,
        update_time_per_update_ns,
        recovery_time_ns
    };
}

TimingRow benchmark_hash_sampler(const std::vector<StreamUpdate>& updates,
    std::size_t support_size, std::size_t trial, std::uint64_t seed,
    const SamplerConfig& base_config) {
    SamplerConfig trial_config = base_config;
    trial_config.seed = seed;

    // Sampler construction is intentionally excluded.
    HashBasedL0Sampler sampler(trial_config);

    const auto update_start = Clock::now();

    for (const StreamUpdate& update : updates) {
        sampler.update(update.item_id, update.delta);
    }

    const auto update_end = Clock::now();

    const auto recovery_start = Clock::now();
    const SampleResult sample = sampler.sample();
    const auto recovery_end = Clock::now();

    const std::int64_t update_time_ns = elapsed_nanoseconds(update_start, update_end);

    const std::int64_t recovery_time_ns = elapsed_nanoseconds(recovery_start, recovery_end);

    const double update_time_per_update_ns = updates.empty() ? 0.0
    : static_cast<double>(update_time_ns) / static_cast<double>(updates.size());

    return TimingRow{
        trial,
        "hash_greedy",
        seed,
        updates.size(),
        support_size,
        to_string(sample.status),
        update_time_ns,
        update_time_per_update_ns,
        recovery_time_ns
    };
}

void write_results(const std::string& output_path, const std::vector<TimingRow>& rows,
    const SamplerConfig& hash_config, std::size_t warmup_trials) {
    std::ofstream output(output_path);

    if (!output.is_open()) {
        throw std::runtime_error(
            "Could not open benchmark output file: " + output_path
        );
    }

    output
        << "trial,sampler,seed,num_updates,support_size,"
        << "sample_status,update_time_ns,"
        << "update_time_per_update_ns,recovery_time_ns,"
        << "warmup_trials,num_levels,sparsity,"
        << "recovery_rows,recovery_buckets,"
        << "hash_independence_k,polynomial_degree\n";

    output << std::setprecision(17);

    for (const TimingRow& row : rows) {
        output
            << row.trial << ","
            << row.sampler << ","
            << row.seed << ","
            << row.num_updates << ","
            << row.support_size << ","
            << row.sample_status << ","
            << row.update_time_ns << ","
            << row.update_time_per_update_ns << ","
            << row.recovery_time_ns << ","
            << warmup_trials << ","
            << hash_config.num_levels << ","
            << hash_config.sparsity << ","
            << hash_config.recovery_rows << ","
            << hash_config.recovery_buckets << ","
            << hash_config.hash_independence_k << ","
            << hash_config.hash_independence_k - 1
            << "\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr
            << "Usage: benchmark_runtime "
            << "<path_to_csv> "
            << "<warmup_trials> "
            << "<measured_trials> "
            << "<output_csv>\n";

        return 1;
    }

    try {
        const std::string dataset_path = argv[1];

        const std::size_t warmup_trials = static_cast<std::size_t>( std::stoull(argv[2]));

        const std::size_t measured_trials = static_cast<std::size_t>(std::stoull(argv[3]));

        const std::string output_path = argv[4];

        if (std::filesystem::exists(output_path)) {
            throw std::runtime_error(
                "Benchmark output already exists: " + output_path
            );
        }

        if (measured_trials == 0) {
            throw std::invalid_argument("measured_trials must be positive.");
        }

        const std::vector<StreamUpdate> updates =
            read_updates_from_csv(dataset_path);

        ExactSupportTracker tracker;

        for (const StreamUpdate& update : updates) {
            tracker.update(update);
        }

        SamplerConfig hash_config;
        hash_config.recovery_mode = RecoveryMode::Greedy;
        hash_config.validate();

        const std::size_t total_trials = warmup_trials + measured_trials;

        std::vector<TimingRow> measured_rows;
        measured_rows.reserve(measured_trials * 2);

        constexpr std::uint64_t base_seed = 123;

        std::cout
            << "Dataset: " << dataset_path << "\n"
            << "Updates per trial: " << updates.size() << "\n"
            << "Final support size: "
            << tracker.support_size() << "\n"
            << "Warm-up trials: " << warmup_trials << "\n"
            << "Measured trials: " << measured_trials << "\n";

        for (std::size_t index = 0; index < total_trials; ++index) {
            const std::uint64_t seed =
                base_seed + static_cast<std::uint64_t>(index);

            const bool is_warmup = index < warmup_trials;

            TimingRow baseline_row;
            TimingRow hash_row;

            // Alternate the execution order to reduce systematic
            // bias from temperature, caches, or background activity.
            if (index % 2 == 0) {
                baseline_row = benchmark_baseline(
                    updates,
                    tracker.support_size(),
                    index,
                    seed
                );

                hash_row = benchmark_hash_sampler(
                    updates,
                    tracker.support_size(),
                    index,
                    seed,
                    hash_config
                );
            } else {
                hash_row = benchmark_hash_sampler(
                    updates,
                    tracker.support_size(),
                    index,
                    seed,
                    hash_config
                );

                baseline_row = benchmark_baseline(
                    updates,
                    tracker.support_size(),
                    index,
                    seed
                );
            }

            if (!is_warmup) {
                const std::size_t measured_trial = index - warmup_trials;

                baseline_row.trial = measured_trial;
                hash_row.trial = measured_trial;

                measured_rows.push_back(std::move(baseline_row));

                measured_rows.push_back(std::move(hash_row));
            }

            if ((index + 1) % 100 == 0 ||index + 1 == total_trials) {
                std::cout
                    << "Completed "
                    << index + 1
                    << " / "
                    << total_trials
                    << " trial pairs\n";
            }
        }

        write_results(
            output_path,
            measured_rows,
            hash_config,
            warmup_trials
        );

        std::cout
            << "Benchmark completed.\n"
            << "Measured rows: "
            << measured_rows.size() << "\n"
            << "Results written to: "
            << output_path << "\n";
    }
    catch (const std::exception& error) {
        std::cerr
            << "Error: "
            << error.what()
            << "\n";

        return 1;
    }

    return 0;
}