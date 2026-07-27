#include "CsvReader.hpp"
#include "ExactSupportTracker.hpp"
#include "L0Sampler.hpp"
#include "StreamUpdate.hpp"

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    // L0Sampler must reject negative item IDs without modifying its state.
    {
        L0Sampler sampler(42);

        bool threw = false;

        try
        {
            sampler.update(StreamUpdate{-1, 1});
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        if (!threw || sampler.support_size() != 0 || sampler.sample().has_value())
        {
            std::cerr
                << "Test 1 failed: L0Sampler negative ID\n";
            return 1;
        }
    }

    // ExactSupportTracker must reject negative IDs without modifying its state.
    {
        ExactSupportTracker tracker;

        bool threw = false;

        try
        {
            tracker.update(StreamUpdate{-1, 1});
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        if (!threw || tracker.support_size() != 0 || !tracker.final_support().empty())
        {
            std::cerr
                << "Test 2 failed: tracker negative ID\n";
            return 1;
        }
    }

    // Item ID zero is valid.
    {
        L0Sampler sampler(42);
        ExactSupportTracker tracker;

        const StreamUpdate update{0, 3};

        sampler.update(update);
        tracker.update(update);

        const auto sampled = sampler.sample();

        if (!sampled.has_value() || sampled.value() != 0 || !tracker.contains(0))
        {
            std::cerr
                << "Test 3 failed: item ID zero\n";
            return 1;
        }
    }

    // INT64_MAX is valid.
    {
        const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();

        L0Sampler sampler(42);
        ExactSupportTracker tracker;

        const StreamUpdate update{maximum, 1};

        sampler.update(update);
        tracker.update(update);

        const auto sampled = sampler.sample();

        if (!sampled.has_value() || sampled.value() != maximum || !tracker.contains(maximum))
        {
            std::cerr
                << "Test 4 failed: INT64_MAX item ID\n";
            return 1;
        }
    }

    // Negative deltas remain valid for a valid item ID.
    {
        L0Sampler sampler(42);
        ExactSupportTracker tracker;

        sampler.update(StreamUpdate{17, 3});
        sampler.update(StreamUpdate{17, -3});

        tracker.update(StreamUpdate{17, 3});
        tracker.update(StreamUpdate{17, -3});

        if (sampler.support_size() != 0 ||sampler.sample().has_value() ||
            tracker.support_size() != 0 ||tracker.contains(17))
        {
            std::cerr
                << "Test 5 failed: negative delta handling\n";
            return 1;
        }
    }

    // CSV input must reject a negative item ID.
    {
        const char* path = "item_domain_invalid_test.csv";

        {
            std::ofstream output(path);

            output << "item_id,delta\n";
            output << "-1,3\n";
        }

        bool threw = false;

        try
        {
            const auto updates = read_updates_from_csv(path);

            static_cast<void>(updates);
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        std::remove(path);

        if (!threw)
        {
            std::cerr
                << "Test 6 failed: CSV accepted "
                << "a negative item ID\n";
            return 1;
        }
    }

    std::cout
        << "All item-domain tests passed.\n";

    return 0;
}