/*
 * MIT License
 * 
 * Copyright (c) 2022 Robin E. R. Davies
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "includes/WifiP2pDnsSdServiceInfo.h"
#include <iostream>
#include <cassert>
using namespace std;

using namespace p2p;

int main(void)
{

    // example from README-P2P
    // # IP Printing over TCP (PTR) (RDATA=MyPrinter._ipp._tcp.local.)
    // p2p_service_add bonjour 045f697070c00c000c01 094d795072696e746572c027
    // # IP Printing over TCP (TXT) (RDATA=txtvers=1,pdl=application/postscript)
    // p2p_service_add bonjour 096d797072696e746572045f697070c00c001001 09747874766572733d311a70646c3d6170706c69636174696f6e2f706f7374736372797074
    std::vector<std::pair<std::string,std::string>> txtRecords;
    txtRecords.push_back({"txtvers", "1"});
    txtRecords.push_back({"pdl", "application/postscrypt"});
    WifiP2pDnsSdServiceInfo info {
        "MyPrinter","_ipp._tcp",txtRecords
    };

    const auto& queries = info.GetQueryList();
    for (auto query: queries)
    {
        cout << query << endl;
    }
    assert(queries.size() == 2);
    assert(queries[0] == "bonjour 045f697070c00c000c01 094d795072696e746572c027");
    assert(queries[1] == "bonjour 096d797072696e746572045f697070c00c001001 09747874766572733d311a70646c3d6170706c69636174696f6e2f706f7374736372797074");

    return 0;
}
