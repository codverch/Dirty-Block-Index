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
        unordered_map<Addr, bool> ToyStore;

        void insertIntoToyStore(Addr addr, bool value);

        void satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                            bool deferred_response = false,
                            bool pending_downgrade = false) override;

        void serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt,
                                CacheBlk *blk) override;

        // //   void recvTimingSnoopReq(PacketPtr pkt) override;

        // PacketPtr cleanEvictBlk(CacheBlk *blk);

        // uint32_t handleSnoop(PacketPtr pkt, CacheBlk *blk,
        //                      bool is_timing, bool is_deferred, bool pending_inval);

    public:
        // Instantitates a toy object
        Toy(const ToyParams &p);

        // Prints the toy store
        void printToyStore();
    };
}

#endif // _MEM_CACHE_TOY_HH_