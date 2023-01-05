#ifndef _MEM_CACHE_RDBI_RDBI_HH_
#define _MEM_CACHE_RDBI_RDBI_HH_

#include <cstdint>
#include <vector>

#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/cache/rdbi/rdbi_entry.hh"

using namespace std;

namespace gem5
{

    class RDBI
    {
    private:
        vector<vector<RDBIEntry>> RDBIStore;

    public:
        RDBI()
    };
}