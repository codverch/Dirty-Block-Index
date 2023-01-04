#ifndef _MEM_CACHE_DBI_DBI_TAGS_DBI_ENTRY_HH_
#define _MEM_CACHE_DBI_DBI_TAGS_DBI_ENTRY_HH_

#include <bitset>
#include <cstdint>

#include "base/types.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/cache/dbi/reg_dbi.hh"

namespace gem5
{
    class CacheBlk;
    class RegDBI;

    /** For each entry in the DBI, there is a corresponding row in DRAM.
     *  The entry includes a row tag, a valid bit, and a vector of dirty bits.
     *  The dirty bits are used to indicate which cache blocks within the corresponding DRAM row have been modified.
     */
    class DBIEntry
    {

    public:
        /*Valid bit for a given DBI entry*/
        int validBit;

        /*The address of the DRAM row that corresponds to a specific cache block.*/
        uint64_t RowTag;

        /*The dirty bits that indicate which cache blocks within a DRAM row have been modified*/
        bitset<64> DirtyBits;
        vector<CacheBlk *> cacheBlocks;

        DBIEntry() = default;
        DBIEntry(int validBit, uint64_t RowTag, bitset<64> DirtyBits) : validBit(validBit), RowTag(RowTag), DirtyBits(DirtyBits) {}
        ~DBIEntry() = default;

        // Setter function for the valid bit
        void setValidBit(bool value)
        {
            validBit = value;
        }

        // Check if the valid bit is set for a DBI entry.

        bool isValid(int validBit)
        {
            if (validBit == 1)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    };
}

#endif // _MEM_CACHE_DBI_DBI_TAGS_DBI_ENTRY_HH_