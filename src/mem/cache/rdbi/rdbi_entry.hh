#ifndef _MEM_CACHE_REGION_DBI_RDBI_ENTRY_HH_
#define _MEM_CACHE_REGION_DBI_RDBI_ENTRY_HH_

#include <bitset>
#include <cstdint>

#include "base/types.hh"
#include "mem/cache/cache_blk.hh"

using namespace std;

namespace gem5
{
    class RDBIEntry
    {
    public:
        int validBit;
        Addr regTag;
        bitset<128> dirtyBits;
        vector<CacheBlk *> blkPtrs;

        RDBIEntry(int numBlksPerRegion)
        {
            validBit = 0;
            regTag = 0;
            dirtyBits = bitset<128>(0);
            blkPtrs = vector<CacheBlk *>(numBlksPerRegion, nullptr);
        }
    };
}

#endif // _MEM_CACHE_REGION_DBI_RDBI_ENTRY_HH_