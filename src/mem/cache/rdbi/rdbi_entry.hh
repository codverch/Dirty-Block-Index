#ifndef _MEM_CACHE_REGION_DBI_RDBI_ENTRY_HH_
#define _MEM_CACHE_REGION_DBI_RDBI_ENTRY_HH_

#include <cstdint>
#include <bitset>

#include "base/types.hh"

namespace gem5
{
    class RDBIEntry
    {
    public:
        int validBit;
        unsigned int regTag;
        bitset<64> dirtyBits;

        RDBIEntry() = default;
        RDBIEntry(int validBit, unsigned int regTag, bitset<64> dirtyBits) : validBit(validBit), regTag(regTag), dirtyBits(dirtyBits) {}
    }

}

#endif // _MEM_CACHE_REGION_DBI_RDBI_ENTRY_HH_