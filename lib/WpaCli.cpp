#include "p2psession/WpaCli.h"

#include <filesystem>
#include "os.h"




using namespace p2psession;

static void SearchPath(const std::string &fileName)
{

}

void WpaCli::Run(const std::string &deviceName)
{
    std::string wpaCliPath = SearchPath("wpa_cli");
}