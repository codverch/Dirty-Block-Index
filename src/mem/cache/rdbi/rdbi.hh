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
        vector<vector<RDBIEntry>> rDBIStore;

        unsigned int numSetBits;
        unsigned int numBlkBits;
        unsigned int numblkIndexBits;
        unsigned int regAddr;
        unsigned int rDBIIndex;

    public:
        RDBI() = default;

        RDBI(unsigned int _numSetBits, unsigned int _numBlkBits, unsigned int _numblkIndexBits);

        Addr getRegDBITag(PacketPtr pkt, unsigned int numBlkBits,
                          unsigned int numblkIndexBits);

        unsigned int getRDBIEntryIndex(PacketPtr pkt, unsigned int numBlkBits,
                                       unsigned int numblkIndexBits);

        bool isDirty(PacketPtr pkt, unsigned int numSetBits, unsigned int numBlkBits,
                     unsigned int numblkIndexBits);
    };
}