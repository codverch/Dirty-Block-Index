#ifndef _MEM_CACHE_DBI_HH_
#define _MEM_CACHE_DBI_HH_

#include <cstdint>

#include "base/types.hh"
#include "mem/cache/cache.hh"
#include "mem/packet.hh"
#include "mem/cache/rdbi/rdbi.hh"

using namespace std;

namespace gem5
{
    struct DBICacheParams;

    class DBICache : public Cache
    {

    protected:
        RDBI RDBIStore;

        unsigned int numBlksInCache(unsigned int cacheSize, unsigned int blkSize);
        unsigned int numBlksInDBI(unsigned int cacheSize, unsigned int blkSize, unsigned int alpha);
        unsigned int numDBIEntries(unsigned int cacheSize, unsigned int blkSize, unsigned int alpha, unsigned int blkEntry);
        unsigned int numDBISets(unsigned int cacheSize, unsigned int blkSize, unsigned int alpha, unsigned int blkEntry, unsigned int dbiAssoc);
        unsigned int numDBISetsBits(unsigned int cacheSize, unsigned int blkSize, unsigned int alpha, unsigned int blkEntry, unsigned int dbiAssoc);
        unsigned int numBlockSizeBits(unsigned int blkSize);
        unsigned int numBlockIndexBits(unsigned int blkEntry);

        void cmpAndSwap(CacheBlk *blk, PacketPtr pkt);
        void satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                            bool deferred_response = false,
                            bool pending_downgrade = false) override;

        void serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt,
                                CacheBlk *blk) override;

        CacheBlk *handleFill(PacketPtr pkt, CacheBlk *blk,
                             PacketList &writebacks, bool allocate);

        bool isBlkSet(CacheBlk *blk, unsigned bits);

        void setBlkCoherenceBits(CacheBlk *blk, unsigned bits);

        void clearBlkCoherenceBits(CacheBlk *blk, unsigned bits);

    public:
        // Alpha needs to be re-defined as type ALPHA
        float alpha;
        unsigned int dbiAssoc;
        unsigned int blkEntry;
        unsigned int cacheSize;
        unsigned int blkSize;

        DBICache(const DBICacheParams &p);
    }

}

#endif // _MEM_CACHE_DBI_HH_