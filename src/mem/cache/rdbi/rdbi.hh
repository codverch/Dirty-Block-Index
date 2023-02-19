#ifndef _MEM_CACHE_RDBI_RDBI_HH_
#define _MEM_CACHE_RDBI_RDBI_HH_

#include "base/types.hh"      // For Addr
#include "base/statistics.hh" // For Stats::Group
#include "base/stats/units.hh"
#include "base/stats/group.hh"
#include "mem/packet.hh"      // For PacketPtr and PacketList
#include "mem/cache/base.hh"  // For CacheBlk
#include "mem/cache/cache.hh" // For Cache::CacheStats
#include "mem/cache/dbi_cache_stats.hh"
#include "mem/cache/dbi.hh"             // For DBICache
#include "mem/cache/rdbi/rdbi_entry.hh" // For RDBIEntry
#include "sim/stats.hh"                 // For Stats::Scalar
#include "sim/stat_control.hh"          // For SimStatControl

#include <cstdint> // For uint64_t
#include <vector>  // For std::vector

using namespace std;

namespace gem5
{

    class RDBI
    {

    protected:
        // RDBI store
        vector<vector<RDBIEntry>>
            rDBIStore;

        // Number of bits required to store the number of sets in RDBI
        unsigned int numSetBits;
        // Number of bits required to store the cache block size
        unsigned int numBlkBits;
        // Number of bits required to store the number of cache blocks per region(i.e., cache blocks per RDBI entry)
        unsigned int numblkIndexBits;
        // Associativity of the RDBI
        unsigned int Assoc;
        // Region address in the memory of the RDBI entry
        unsigned int regAddr;
        // Set index of the RDBI entry
        unsigned int rDBIIndex;
        // Number of cache blocks per region
        unsigned int numBlksInRegion;
        // Cache block size
        unsigned int blkSize;
        // Use aggressive writeback mechanism
        bool useAggressiveWriteback;

    public:
        // Constructor
        RDBI(unsigned int _numSetBits, unsigned int _numBlkBits, unsigned int _numblkIndexBits, unsigned int _assoc, unsigned int numBlksInRegion, unsigned int blkSize, bool _useAggressiveWriteback, DBICacheStats &dbistats); // Updated the type of dbistats

        // Variable to store instance of a structure, overcoming the invalid type error
        DBICacheStats *dbiCacheStats;

        // Get the cache block index from the bitset
        unsigned int getblkIndexInBitset(PacketPtr pkt);

        // Get the region address of the RDBI entry
        Addr getRegDBITag(PacketPtr pkt);

        // Calculate the set index of the RDBI entry
        int getRDBIEntryIndex(PacketPtr pkt);

        // Return the DBIEntry if there is a regTag match
        RDBIEntry *getRDBIEntry(PacketPtr pkt);

        // Pick a replacement RDBI entry, by calling the RDBI replacement policy
        RDBIEntry *pickRDBIEntry(vector<RDBIEntry> &rDBIEntries);

        // Random replacement policy
        RDBIEntry *randomReplacementPolicy(vector<RDBIEntry> &rDBIEntries);

        // Check if the cache block is dirty
        bool isDirty(PacketPtr pkt);

        // Clear the dirty bit of the cache block and generate a writeback
        void clearDirtyBit(PacketPtr pkt, PacketList &writebacks);
        // cleardirty(CacheBlk *blkPtr) - only clears the dirty bit (No writebacks)

        // Clear the dirty bit of the cache block
        void clearDirty(PacketPtr pkt, CacheBlk *blkPtr);

        // Set the dirty bit of the cache block
        void setDirtyBit(PacketPtr pkt, CacheBlk *blkPtr, PacketList &writebacks);
        // blk->addr and custom regenerated address
        // Create a new RDBI entry
        void createRDBIEntry(PacketList &writebacks, PacketPtr pkt, CacheBlk *blkPtr);

        // Writeback the dirty cache blocks in the RDBI entry
        void writebackRDBIEntry(PacketList &writebacks, RDBIEntry *rDBIEntry);

        // evictDBIEntry function that takes PacketList and pointer to the rDBIEntries as arguments
        void evictRDBIEntry(PacketList &writebacks, vector<RDBIEntry> &rDBIEntries);
    };
}

#endif // _MEM_CACHE_RDBI_RDBI_HH_