#ifndef _MEM_CACHE_DBI_HH_
#define _MEM_CACHE_DBI_HH_

#include <cstdint>

#include "base/types.hh"
#include "mem/cache/cache.hh"
#include "mem/packet.hh"
#include "mem/cache/rdbi/rdbi.hh"
#include "mem/cache/cache.hh"

using namespace std;

namespace gem5
{
    struct DBICacheParams;
    class RDBI;

    class DBICache : public Cache
    {

    protected:
        RDBI *rdbi;

        uint32_t numBlksInCache;
        uint32_t numBlksInDBI;
        uint32_t numBlksInRegion;
        uint32_t numDBIEntries;
        uint32_t numDBISets;
        uint32_t numDBISetsBits;
        uint32_t numBlockSizeBits;
        uint32_t numBlockIndexBits;

        void cmpAndSwap(CacheBlk *blk, PacketPtr pkt);
        void satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                            bool deferred_response = false,
                            bool pending_downgrade = false) override;

        void serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt,
                                CacheBlk *blk) override;

        CacheBlk *handleFill(PacketPtr pkt, CacheBlk *blk,
                             PacketList &writebacks, bool allocate);

    public:
        // Also, don't keep these as public
        // Alpha needs to be re-defined as type ALPHA
        float alpha;
        unsigned int dbiAssoc;
        unsigned int numBlksInRegion;
        unsigned int cacheSize;
        unsigned int blkSize;

        DBICache(const DBICacheParams &p);
    };
}

#endif // _MEM_CACHE_DBI_HH_