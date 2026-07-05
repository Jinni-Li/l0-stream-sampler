#include "HashBasedL0Sampler.hpp"

#include <cstdint>
#include <iostream>
#include <unordered_set>
#include <stdexcept>

int main() {
    {
        HashBasedL0Sampler sampler(16, 42);

        sampler.update(17, 3);

        SampleResult sample = sampler.sample();

        if (sample.status != SampleStatus::Success || !sample.item.has_value() || sample.item.value() != 17) {
            std::cerr << "Test 1 failed: expected to sample 17\n";
            return 1;
        }
    }

    {
        HashBasedL0Sampler sampler(16, 42);

        sampler.update(17, 3);
        sampler.update(17, -3);

        SampleResult sample = sampler.sample();

        if (sample.status != SampleStatus::EmptySupport || sample.item.has_value()) {
            std::cerr << "Test 2 failed: expected failure on empty support\n";
            return 1;
        }
    }

    {
        std::unordered_set<std::int64_t> support = {5, 17, 44};

        int valid = 0;
        int invalid = 0;
        int failures = 0;

        for (std::uint64_t seed = 1; seed <= 100; ++seed) {
            HashBasedL0Sampler sampler(16, seed);

            sampler.update(5, 1);
            sampler.update(10, 1);
            sampler.update(17, 1);
            sampler.update(30, 1);
            sampler.update(44, 1);
            sampler.update(10, -1);
            sampler.update(30, -1);

            auto sample = sampler.sample();

            if (sample.status != SampleStatus::Success || !sample.item.has_value()) {
                ++failures;
            } else if (support.count(sample.item.value()) > 0) {
                ++valid;
            } else {
                ++invalid;
            }
        }

        if (invalid > 0) {
            std::cerr << "Test 3 failed: got invalid samples\n";
            return 1;
        }

        if (valid == 0) {
            std::cerr << "Test 3 failed: expected at least one valid sample\n";
            return 1;
        }

        std::cout << "Tiny stream hash-based results: "
                  << "valid=" << valid
                  << ", failures=" << failures
                  << ", invalid=" << invalid
                  << "\n";
    }

    {
        bool threw = false;

        try{
            HashBasedL0Sampler sampler(0, 42);
        }catch (const std::invalid_argument&){
            threw = true;
        }

        if(!threw){
            std::cerr << "Test 4 failed: expected invalid_argument for zero levels\n";
            return 1;
        }
    }

    std::cout << "All HashBasedL0Sampler tests passed.\n";
    return 0;
}