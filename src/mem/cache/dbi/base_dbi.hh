#ifndef _MEM_CACHE_DBI_BASE_DBI_HH_
#define _MEM_CACHE_DBI_BASE_DBI_HH_

#include "base/types.hh"
#include "mem/packet.hh"

using namespace std;

namespace gem5
{
    class Packet;
    class BaseDBI
    {

    public:
        // Abstract functions
        // setDirtyBit might need to add a new entry, if there isn't space might need to evict a DBI entry
        // But, before evicting might need to check if the dirty bit is set, if it is set, then need to write back the data
        // virtual void setDirtyBit(Addr, PacketList &writebacks) = 0;
        // virtual void clearDirtyBit(Addr, PacketList &writebacks) = 0; // Do writeback if dirty bit is set and clear it after writeback
        virtual bool isDirty(Packet *pkt, int) = 0;
        virtual void setDirtyBit(Packet *pkt, int) = 0;
        virtual void clearDirtyBit(Packet *pkt, int) = 0;
    };
}

#endif // _MEM_CACHE_DBI_BASE_DBI_HH_