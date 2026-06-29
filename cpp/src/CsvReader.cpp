#include "CsvReader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<StreamUpdate> read_updates_from_csv(const std::string& path){
    std::ifstream file(path);

    if (!file.is_open())
    {
        throw std::runtime_error("Could not open file: " + path);
    }
    
    std::vector<StreamUpdate> updates;
    std::string line;

    //skip headers

    std::getline(file,line);

    while(std::getline(file,line)){
        if (line.empty())
        {
            continue;
        }

        std::stringstream ss(line);
        std::string item_str;
        std::string delta_str;

        std::getline(ss, item_str, ',');
        std::getline(ss, delta_str,',');
        
        StreamUpdate update;

        update.item_id = std::stoll(item_str);
        update.delta = std::stoll(delta_str);

        updates.push_back(update);
    }

    return updates;
}
