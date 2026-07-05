#include "PairwiseHash.hpp"

#include <iostream>
#include <stdexcept>

int main(){
    {
        PairwiseHash hash(42,8);

        for (std::int64_t item = 0; item < 1000; ++item)
        {
            std::uint64_t bucket = hash(item);

            if (bucket >= 8)
            {
                std::cerr << "Test 1 failed: bucket out of range\n";
                return 1;    
            }   
        }
    }

    {
        PairwiseHash hash_a(123,16);
        PairwiseHash hash_b(123,16);

        for (std::int64_t item = 0; item < 1000; ++item)
        {

            if (hash_a(item) != hash_b(item))
            {
                std::cerr << "Test 2 failed: same seed should be deterministic\n";
                return 1;    
            }   
        }
    }

    {
        PairwiseHash hash(999,32);

        std::uint64_t bucket_positive = hash(17);
        std::uint64_t bucket_negative = hash(-17);

        if (bucket_positive >= 32 || bucket_negative >= 32)
        {
            std::cerr << "Test 3 failed: negative or positive item out of range\n";
            return 1;    
        }   

    }

    {
        bool threw = false;

        try{
            PairwiseHash hash(42,0);
        } catch(const std::invalid_argument&){
            threw = true;
        }

        if (!threw)
        {
            std::cerr << "Test 4 failed: expected invalid_argument for zero range\n";
            return 1;    
        }   
    }

    std::cout << "All PairwiseHash test passed.\n";
    return 0;
}