#ifndef _MEM_CACHE_TOY_HH_
#define _MEM_CACHE_TOY_HH_

#include <cstdint>
#include <unordered_map>

#include "base/compiler.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/packet.hh"
#include "mem/cache/cache.hh"

using namespace std;

namespace gem5
{
    struct ToyParams;

    class Toy : public Cache
    {
    protected:
        bool useDBI = false;
<<<<<<< HEAD
        
=======
>>>>>>> 16fdf635c0a52408c75411451eb7e0e4061eb1c4

        unordered_map<Addr, bool> ToyStore;

        void insertIntoToyStore(Addr addr, bool value);

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
        // Instantitates a toy object
        Toy(const ToyParams &p);

        // Prints the toy store
        void printToyStore();
    };
}

#endif // _MEM_CACHE_TOY_HH_
