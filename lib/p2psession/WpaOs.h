#pragma once 

#include <filesystem>
#include <string>

namespace p2psession {
    std::filesystem::path SearchPath(const std::string & filename);
}