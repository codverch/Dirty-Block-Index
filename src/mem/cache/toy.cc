#include "mem/cache/toy.hh"
#include "mem/cache/cache.hh"

#include <cassert>

#include "base/compiler.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "base/types.hh"
#include "debug/Cache.hh"
#include "debug/Toy.hh"
#include "debug/CacheTags.hh"
#include "debug/CacheVerbose.hh"
#include "enums/Clusivity.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/mshr.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/write_queue_entry.hh"
#include "mem/request.hh"
#include "params/Toy.hh"

using namespace std;

namespace gem5
{
    Toy::Toy(const ToyParams &p)
        : Cache(p)
    {
        // cout << "Hey, I am a toy component" << endl;
        DPRINTF(Toy, "Hey, I am a toy component. Glad to exist in 2022\n");
    }

    void
    Toy::insertIntoToyStore(Addr addr, bool value)
    {
        ToyStore[addr] = value;
    }

    void
    Toy::printToyStore()
    {
        for (auto it = ToyStore.begin(); it != ToyStore.end(); ++it)
        {
            cout << "Key: " << it->first << " Value: " << it->second << endl;
        }
    }

    void
    Toy::satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                        bool deferred_response, bool pending_downgrade)
    {
        BaseCache::satisfyRequest(pkt, blk);

        if (pkt->isRead())
        {
            // determine if this read is from a (coherent) cache or not
            if (pkt->fromCache())
            {
                assert(pkt->getSize() == blkSize);

                if (pkt->needsWritable())
                {
                    // sanity check
                    assert(pkt->cmd == MemCmd::ReadExReq ||
                           pkt->cmd == MemCmd::SCUpgradeFailReq);
                    assert(!pkt->hasSharers());

                    if (blk->isSet(CacheBlk::DirtyBit))
                    {
                        pkt->setCacheResponding();
                        blk->clearCoherenceBits(CacheBlk::DirtyBit);
                        insertIntoToyStore(pkt->getAddr(), false); // Deepanjali
                    }
                }
                else if (blk->isSet(CacheBlk::WritableBit) &&
                         !pending_downgrade && !pkt->hasSharers() &&
                         pkt->cmd != MemCmd::ReadCleanReq)
                {

                    if (blk->isSet(CacheBlk::DirtyBit))
                    {

                        if (!deferred_response)
                        {

                            pkt->setCacheResponding();

                            blk->clearCoherenceBits(CacheBlk::DirtyBit);
                            insertIntoToyStore(pkt->getAddr(), false); // Deepanjali
                        }
                        else
                        {

                            pkt->setHasSharers();
                        }
                    }
                }
                else
                {
                    // otherwise only respond with a shared copy
                    pkt->setHasSharers();
                }
            }
        }
    }

    void
    Toy::serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt, CacheBlk *blk)
    {
        QueueEntry::Target *initial_tgt = mshr->getTarget();
        // First offset for critical word first calculations
        const int initial_offset = initial_tgt->pkt->getOffset(blkSize);

        const bool is_error = pkt->isError();
        // allow invalidation responses originating from write-line
        // requests to be discarded
        bool is_invalidate = pkt->isInvalidate() &&
                             !mshr->wasWholeLineWrite;

        bool from_core = false;
        bool from_pref = false;

        if (pkt->cmd == MemCmd::LockedRMWWriteResp)
        {
            assert(initial_tgt->pkt->cmd == MemCmd::LockedRMWReadReq);
            mshr->popTarget();
            delete initial_tgt->pkt;
            initial_tgt = nullptr;
        }

        MSHR::TargetList targets = mshr->extractServiceableTargets(pkt);
        for (auto &target : targets)
        {
            Packet *tgt_pkt = target.pkt;
            switch (target.source)
            {
            case MSHR::Target::FromCPU:
                from_core = true;

                Tick completion_time;
                // Here we charge on completion_time the delay of the xbar if the
                // packet comes from it, charged on headerDelay.
                completion_time = pkt->headerDelay;

                // Software prefetch handling for cache closest to core
                if (tgt_pkt->cmd.isSWPrefetch())
                {
                    if (tgt_pkt->needsWritable())
                    {

                        assert(blk);
                        blk->setCoherenceBits(CacheBlk::DirtyBit);
                        insertIntoToyStore(pkt->getAddr(), true); // Deepanjali

                        panic_if(isReadOnly, "Prefetch exclusive requests from "
                                             "read-only cache %s\n",
                                 name());
                    }

                    delete tgt_pkt;
                    break; // skip response
                }

                if (tgt_pkt->cmd == MemCmd::WriteLineReq)
                {
                    assert(!is_error);
                    assert(blk);
                    assert(blk->isSet(CacheBlk::WritableBit));
                }

                if (blk && blk->isValid() &&
                    (!mshr->isForward || !pkt->hasData()))
                {
                    satisfyRequest(tgt_pkt, blk, true, mshr->hasPostDowngrade());

                    // How many bytes past the first request is this one
                    int transfer_offset =
                        tgt_pkt->getOffset(blkSize) - initial_offset;
                    if (transfer_offset < 0)
                    {
                        transfer_offset += blkSize;
                    }

                    // If not critical word (offset) return payloadDelay.
                    // responseLatency is the latency of the return path
                    // from lower level caches/memory to an upper level cache or
                    // the core.
                    completion_time += clockEdge(responseLatency) +
                                       (transfer_offset ? pkt->payloadDelay : 0);

                    assert(!tgt_pkt->req->isUncacheable());

                    assert(tgt_pkt->req->requestorId() < system->maxRequestors());
                    stats.cmdStats(tgt_pkt)
                        .missLatency[tgt_pkt->req->requestorId()] +=
                        completion_time - target.recvTime;

                    if (tgt_pkt->cmd == MemCmd::LockedRMWReadReq)
                    {
                        // We're going to leave a target in the MSHR until the
                        // write half of the RMW occurs (see comments above in
                        // recvTimingReq()).  Since we'll be using the current
                        // request packet (which has the allocated data pointer)
                        // to form the response, we have to allocate a new dummy
                        // packet to save in the MSHR target.
                        mshr->updateLockedRMWReadTarget(tgt_pkt);
                        // skip the rest of target processing after we
                        // send the response
                        // Mark block inaccessible until write arrives
                        blk->clearCoherenceBits(CacheBlk::WritableBit);
                        blk->clearCoherenceBits(CacheBlk::ReadableBit);
                    }
                }
                else if (pkt->cmd == MemCmd::UpgradeFailResp)
                {
                    // failed StoreCond upgrade
                    assert(tgt_pkt->cmd == MemCmd::StoreCondReq ||
                           tgt_pkt->cmd == MemCmd::StoreCondFailReq ||
                           tgt_pkt->cmd == MemCmd::SCUpgradeFailReq);
                    // responseLatency is the latency of the return path
                    // from lower level caches/memory to an upper level cache or
                    // the core.
                    completion_time += clockEdge(responseLatency) +
                                       pkt->payloadDelay;
                    tgt_pkt->req->setExtraData(0);
                }
                else if (pkt->cmd == MemCmd::LockedRMWWriteResp)
                {
                    // Fake response on LockedRMW completion, see above.
                    // Since the data is already in the cache, we just use
                    // responseLatency with no extra penalties.
                    completion_time = clockEdge(responseLatency);
                }
                else
                {
                    if (is_invalidate && blk && blk->isValid())
                    {
                        // We are about to send a response to a cache above
                        // that asked for an invalidation; we need to
                        // invalidate our copy immediately as the most
                        // up-to-date copy of the block will now be in the
                        // cache above. It will also prevent this cache from
                        // responding (if the block was previously dirty) to
                        // snoops as they should snoop the caches above where
                        // they will get the response from.
                        invalidateBlock(blk);
                    }
                    // not a cache fill, just forwarding response
                    // responseLatency is the latency of the return path
                    // from lower level cahces/memory to the core.
                    completion_time += clockEdge(responseLatency) +
                                       pkt->payloadDelay;
                    if (!is_error)
                    {
                        if (pkt->isRead())
                        {
                            // sanity check
                            assert(pkt->matchAddr(tgt_pkt));
                            assert(pkt->getSize() >= tgt_pkt->getSize());

                            tgt_pkt->setData(pkt->getConstPtr<uint8_t>());
                        }
                        else
                        {
                            // MSHR targets can read data either from the
                            // block or the response pkt. If we can't get data
                            // from the block (i.e., invalid or has old data)
                            // or the response (did not bring in any data)
                            // then make sure that the target didn't expect
                            // any.
                            assert(!tgt_pkt->hasRespData());
                        }
                    }

                    // this response did not allocate here and therefore
                    // it was not consumed, make sure that any flags are
                    // carried over to cache above
                    tgt_pkt->copyResponderFlags(pkt);
                }
                tgt_pkt->makeTimingResponse();
                // if this packet is an error copy that to the new packet
                if (is_error)
                    tgt_pkt->copyError(pkt);
                if (tgt_pkt->cmd == MemCmd::ReadResp &&
                    (is_invalidate || mshr->hasPostInvalidate()))
                {
                    // If intermediate cache got ReadRespWithInvalidate,
                    // propagate that.  Response should not have
                    // isInvalidate() set otherwise.
                    tgt_pkt->cmd = MemCmd::ReadRespWithInvalidate;
                    DPRINTF(Cache, "%s: updated cmd to %s\n", __func__,
                            tgt_pkt->print());
                }
                // Reset the bus additional time as it is now accounted for
                tgt_pkt->headerDelay = tgt_pkt->payloadDelay = 0;
                cpuSidePort.schedTimingResp(tgt_pkt, completion_time);
                break;

            case MSHR::Target::FromPrefetcher:
                assert(tgt_pkt->cmd == MemCmd::HardPFReq);
                from_pref = true;

                delete tgt_pkt;
                break;

            case MSHR::Target::FromSnoop:
                // I don't believe that a snoop can be in an error state
                assert(!is_error);
                // response to snoop request
                DPRINTF(Cache, "processing deferred snoop...\n");
                // If the response is invalidating, a snooping target can
                // be satisfied if it is also invalidating. If the reponse is, not
                // only invalidating, but more specifically an InvalidateResp and
                // the MSHR was created due to an InvalidateReq then a cache above
                // is waiting to satisfy a WriteLineReq. In this case even an
                // non-invalidating snoop is added as a target here since this is
                // the ordering point. When the InvalidateResp reaches this cache,
                // the snooping target will snoop further the cache above with the
                // WriteLineReq.
                assert(!is_invalidate || pkt->cmd == MemCmd::InvalidateResp ||
                       pkt->req->isCacheMaintenance() ||
                       mshr->hasPostInvalidate());
                handleSnoop(tgt_pkt, blk, true, true, mshr->hasPostInvalidate());
                break;

            default:
                panic("Illegal target->source enum %d\n", target.source);
            }
        }

        if (blk && !from_core && from_pref)
        {
            blk->setPrefetched();
        }

        if (!mshr->hasLockedRMWReadTarget())
        {
            maintainClusivity(targets.hasFromCache, blk);

            if (blk && blk->isValid())
            {
                // an invalidate response stemming from a write line request
                // should not invalidate the block, so check if the
                // invalidation should be discarded
                if (is_invalidate || mshr->hasPostInvalidate())
                {
                    invalidateBlock(blk);
                }
                else if (mshr->hasPostDowngrade())
                {
                    blk->clearCoherenceBits(CacheBlk::WritableBit);
                }
            }
        }
    }
}
