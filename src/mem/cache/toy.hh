#ifndef _MEM_CACHE_TOY_HH_
#define _MEM_CACHE_TOY_HH_

#include <cstdint>
#include <unordered_map>

#include "base/compiler.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/packet.hh"
#include "mem/cache/cache.hh"
#include "mem/cache/dbi/map_dbi.hh"
#include "mem/cache/dbi/reg_dbi.hh"

using namespace std;

namespace gem5
{

    struct ToyParams;

    class Toy : public Cache
    {
    protected:
        bool useDBI = false;
        // MapDBI dbi;

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
        // Number of sets in the DBI Cache
        unsigned int RegDBISets;

        // Associativity of the DBI Cache
        unsigned int RegDBIAssoc;

        // Number of cacheblocks per DBIEntry in the DBI Cache
        unsigned int RegDBIBlkPerDBIEntry = 64;

        // DBI Size
        /*
         * "DBI Size" is a parameter that represents the desired ratio of the cumulative number of blocks
            tracked by the DBI to the number of blocks tracked by the cache tag store.
         */
        float RegDBISize;

        // DBI augmented caceh size
        unsigned int ToyCacheSize;

        // Instantitates a toy object
        Toy(const ToyParams &p);

        // Prints the toy store
        void printToyStore();
    };
}

#endif // _MEM_CACHE_TOY_HH_
