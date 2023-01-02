#ifndef _MEM_CACHE_DBI_DBI_TAGS_DBI_ENTRY_HH_
#define _MEM_CACHE_DBI_DBI_TAGS_DBI_ENTRY_HH_

#include <bitset>
#include <cstdint>

#include "base/types.hh"
#include "mem/cache/cache_blk.hh"
#include "src/mem/cache/tags/indexing_policies/base.hh"
#include "src/mem/cache/dbi/reg_dbi.hh"

namespace gem5
{
    class CacheBlk;
    class RegDBI;

    /** Each DBI entry corresponds to some row in DRAM. Each entry contains a row tag,
     *  a valid bit, and a vector of dirty bits. The dirty bits are used to track which
     *  cacheblocks are dirty in the corresponding row in DRAM.
     */
    class DBIEntry
    {

    protected:
        /*Valid bit for a given DBI entry*/
        int validBit;

        /*Address of a DRAM row corresponding to a given cache block*/
        uint64_t RowTag;

        /*Dirty bits for each cache block in a DRAM row*/
        bitset<64> DirtyBits;
        vector<CacheBlk *> cacheBlocks;

    public:
        DBIEntry(int validBit, uint64_t RowTag, bitset<64> DirtyBits) : validBit(validBit), RowTag(RowTag), DirtyBits(DirtyBits) {}
        ~DBIEntry() = default;

        bool isValid() const;
        void setValidBit(int validBit);

        Addr getRowTag(CacheBlk *block, Addr address);
    };
}

#endif // _MEM_CACHE_DBI_DBI_TAGS_DBI_ENTRY_HH_