#ifndef _MEM_CACHE_DBI_HH_
#define _MEM_CACHE_DBI_HH_

#include <cstdint>

#include "base/types.hh"
#include "mem/cache/cache.hh"
#include "base/statistics.hh"
#include "mem/packet.hh"
#include "mem/cache/rdbi/rdbi.hh"
#include "mem/cache/base.hh"

using namespace std;

namespace gem5
{
    // The DBI augmented cache will take certain parameters.
    struct DBICacheParams;
    // A class named RDBI, which stands for region-level DBI,
    // is responsible for maintaining the dirty bit for a region in memory.
    class RDBI;

    class DBICache : public Cache
    {

    protected:
        // The RDBI object is responsible for maintaining the dirty bit for a region in memory.
        RDBI *rdbi;

        // The size of the DBI augmented cache.
        unsigned int cacheSize;

        // Also, don't keep these as public
        // Alpha needs to be re-defined as type ALPHA
        float alpha;

        // The associativity of DBI, or the number of DBI entries contained within a set.
        unsigned int dbiAssoc;
        // The cache block size of DBI augmented cache.
        unsigned int blkSize;

        // The total number of blocks contained within the cache.
        uint32_t numBlksInCache;
        // The total number of blocks contained within the DBI.
        uint32_t numBlksInDBI;
        // The total number of blocks contained within a region.
        uint32_t numBlksInRegion;
        // The total number of regions tracked by the DBI i.e., RDBI entries.
        uint32_t numDBIEntries;
        // The total number of sets in the DBI.
        uint32_t numDBISets;
        // The number of bits required to represent the number of sets in the DBI.
        uint32_t numDBISetsBits;
        // The number of bits required to represent the cache block size
        uint32_t numBlockSizeBits;
        // The number of bits required to index into a set in the DBI.
        uint32_t numBlockIndexBits;

        // Use aggressive writeback mechanism.
        bool useAggressiveWriteback;

        // A structure called RDBIStats inheriting from statistics, group to
        // keep track of the statistics of the DBI.Specifically, number of
        // valid DBI entries at the end of the simulation.

        void
        cmpAndSwap(CacheBlk *blk, PacketPtr pkt);
        void satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                            bool deferred_response = false,
                            bool pending_downgrade = false) override;

        void serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt,
                                CacheBlk *blk) override;

        CacheBlk *handleFill(PacketPtr pkt, CacheBlk *blk,
                             PacketList &writebacks, bool allocate);

    public:
        // Structure to keep track of the stats for different cache commands
        struct DBICmdStats : public statistics::Group
        {
            DBICmdStats(DBICache &c, const std::string &name);

            void regStatsFromParentDBI();

            // Create an object of DBICache
            const DBICache &dbiCache;
        };

        // Structure to keep track of the stats for the DBICache
        struct DBICacheStats : public statistics::Group
        {
            // Constructor for DBICacheStats
            DBICacheStats(DBICache &c);

            // Override the regStats() from the parent
            void regStats() override;

            DBICmdStats &cmdStats(const PacketPtr p)
            {
                return *cmd[p->cmdToIndex()];
            }

            // Create an object of DBICache
            const DBICache &dbiCache;

            /** Number of blocks written back due to aggressive writebacks */
            statistics::Vector agrWritebacks;

            /** Per-command statistics i.e., stats for different cache commands */
            std::vector<std::unique_ptr<DBICmdStats>> cmd;

        } dbistats;

        // A constructor for the DBI augmented cache.
        DBICache(const DBICacheParams &p);
    };
}

#endif // _MEM_CACHE_DBI_HH_