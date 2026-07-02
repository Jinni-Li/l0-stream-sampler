#include "SSparseSketch.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

bool contains(const std::vector <std::int64_t>& values, std::int64_t target){
    return std::find(values.begin(), values.end(), target) != values.end();
}

int main(){
    {    
        SSparseSketch sketch(4,4,8,42);

        auto result = sketch.recover();

        if (result.success || !result.empty || !result.candidates.empty())
        {
            std::cerr << "Test 1 failed: expected empty sketch\n";
            return 1;
        }
    }

    {    
        SSparseSketch sketch(4,4,8,42);
        sketch.update(17,3);

        auto result = sketch.recover();

        if (!result.success || result.empty || result.too_dense)
        {
            std::cerr << "Test 2 failed: expected successful recovery\n";
            return 1;
        }

        if(!contains(result.candidates,17)){
            std::cerr << "Test 2 failed: expected to recover item 17\n";
            return 1;
        }
    }
    
    {    
        SSparseSketch sketch(4,4,16,123);
        sketch.update(17,1);
        sketch.update(44,1);

        auto result = sketch.recover();

        if (!result.success) {
            std::cerr << "Test 3 failed: expected successful recovery for two items\n";
            return 1;
        }

        if (!contains(result.candidates, 17) || !contains(result.candidates, 44)) {
            std::cerr << "Test 3 failed: expected to recover 17 and 44\n";
            return 1;
        }
    }

    {    
        SSparseSketch sketch(4,4,16,123);
        sketch.update(1001, 50);
        sketch.update(1001, -20);

        auto result = sketch.recover();

        if (!result.success || !contains(result.candidates, 1001)) {
            std::cerr << "Test 4 failed: expected to recover partially cancelled item 1001\n";
            return 1;
        }

        sketch.update(1001, -30);

        auto empty_result = sketch.recover();

        if (empty_result.success || !empty_result.empty) {
            std::cerr << "Test 4 failed: expected empty sketch after full cancellation\n";
            return 1;
        }
    }

    {    
        SSparseSketch sketch(2, 4, 64, 999);
        sketch.update(17,1);
        sketch.update(44,1);
        sketch.update(1001, 1);

        auto result = sketch.recover();

        if (!result.too_dense) {
            std::cerr << "Test 5 failed: expected too_dense when recovering more than s items\n";
            return 1;
        }

    }

    std::cout << "All SSparseSketch tests passed.\n";
    return 0;
}