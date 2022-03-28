#include "p2psession/WpaOs.h"
#include <vector>
#include <stdexcept>
#include "ss.h"


using namespace p2psession;
using namespace std;

static vector<string> split(const std::string &value, char splitChar)
{
    vector<string> result;
    
    size_t start =  0;

    while (start < value.size())
    {
        auto next = value.find_first_of(splitChar);
        if (next == string::npos)
        {
            result.push_back(value.substr(start));
            break;
        }
        result.push_back(value.substr(start,next-start));
        start = next;
    }
    return result;
}

std::filesystem::path SearchPath(const std::string &filename)
{
    vector<string> paths = split(getenv("PATH"),':');

    for (auto path : paths)
    {
        filesystem::path file = std::filesystem::path(path) / filename;
        if (filesystem::exists(file))
        {
            return file;
        }
    }
    throw std::invalid_argument(SS("File not found: " << filename));
}