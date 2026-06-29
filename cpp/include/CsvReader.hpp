#pragma once

#include "StreamUpdate.hpp"

#include <string>
#include <vector>

std::vector<StreamUpdate> read_updates_from_csv(const std::string& path);