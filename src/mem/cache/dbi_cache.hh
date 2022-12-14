#ifndef _MEM_CACHE_DBI_CACHE_HH_
#define _MEM_CACHE_DBI_CACHE_HH_

#include <unordered_map>

#include "base/compiler.hh"
#include "base/logging.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/packet.hh"

using namespace std;

namespace gem5
{
    class CacheBlk;
    class MSHR;
    struct DBICache;

    // A cache augmented with: Dirty Block Index(DBI)

    class DBICache : public NoncoherentCache
    {
    protected:
        bool access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                    PacketList &writebacks);

        void handleTimingReqMiss(PacketPtr pkt, CacheBlk *blk,
                                 Tick forward_time,
                                 Tick request_time);
        void recvTimingReq(PacketPtr pkt);

        void doWritebacks(PacketList &writebacks,
                          Tick forward_time);

        void doWritebacksAtomic(PacketList &writebacks);

        void serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt,
                                CacheBlk *blk);

        void recvTimingResp(PacketPtr pkt);

        void recvTimingSnoopReq(PacketPtr pkt)
        {
            panic("Unexpected timing snoop request %s", pkt->print());
        }

        void recvTimingSnoopResp(PacketPtr pkt)
        {
            panic("Unexpected timing snoop response %s", pkt->print());
        }

        Cycles handleAtomicReqMiss(PacketPtr pkt, CacheBlk *&blk,
                                   PacketList &writebacks);

        Tick recvAtomic(PacketPtr pkt);

        Tick recvAtomicSnoop(PacketPtr pkt)
        {
            panic("Unexpected atomic snoop request %s", pkt->print());
        }

        void functionalAccess(PacketPtr pkt, bool from_cpu_side);

        void satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                            bool deferred_response = false,
                            bool pending_downgrade = false);

        /*
         * Creates a new packet with the request to be send to the memory
         * below. The noncoherent cache is below the point of coherence
         * and therefore all fills bring in writable, therefore the
         * needs_writeble parameter is ignored.
         */
        PacketPtr createMissPacket(PacketPtr cpu_pkt, CacheBlk *blk,
                                   bool needs_writable,
                                   bool is_whole_line_write) const;

        [[nodiscard]] PacketPtr evictBlock(CacheBlk *blk);

        unordered_map<Addr tag, bool DirtyBit> DBIStore;

    public:
        NoncoherentCache(const NoncoherentCacheParams &p);
    };
}

#endif // _MEM_CACHE_DBI_CACHE_HH_