#ifndef _MEM_CACHE_RDBI_RDBI_HH_
#define _MEM_CACHE_RDBI_RDBI_HH_

#include <cstdint>
#include <vector>

#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/cache/rdbi/rdbi_entry.hh"
#include "mem/cache/dbi.hh"
#include "mem/cache/cache.hh"

using namespace std;

namespace gem5
{

    class RDBI
    {

    public:
        vector<vector<RDBIEntry>> rDBIStore;

        unsigned int numSetBits;
        unsigned int numBlkBits;
        unsigned int numblkIndexBits;
        unsigned int Assoc;
        unsigned int regAddr;
        unsigned int rDBIIndex;
        unsigned int blkIndexInBitset;

        RDBI(unsigned int _numSetBits, unsigned int _numBlkBits, unsigned int _numblkIndexBits, unsigned int _assoc);

        unsigned int getblkIndexInBitset(PacketPtr pkt);

        Addr getRegDBITag(PacketPtr pkt);

        int getRDBIEntryIndex(PacketPtr pkt);

        bool isDirty(PacketPtr pkt);

        void clearDirtyBit(PacketPtr pkt, PacketList &writebacks);

        void setDirtyBit(PacketPtr pkt, CacheBlk *blkPtr, PacketList &writebacks);
    };
}

#endif // _MEM_CACHE_RDBI_RDBI_HH_