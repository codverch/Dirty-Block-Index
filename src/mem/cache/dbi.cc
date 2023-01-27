
#include <cmath>

#include "mem/cache/dbi.hh"
#include "base/compiler.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/DBICache.hh"
#include "enums/Clusivity.hh"
#include "debug/CacheTags.hh"
#include "debug/CacheVerbose.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/mshr.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/write_queue_entry.hh"
#include "mem/cache/rdbi/rdbi.hh"
#include "mem/cache/rdbi/rdbi_entry.hh"
#include "params/DBICache.hh"
#include "mem/packet.hh"
#include "mem/cache/cache.hh"
#include "mem/cache/base.hh"

using namespace std;

namespace gem5
{

    DBICache::DBICache(const DBICacheParams &p)
        : Cache(p),
          cacheSize(p.size),
          alpha(p.alpha),
          dbiAssoc(p.dbi_assoc),
          blkSize(p.blkSize),
          numBlksInRegion(p.blk_per_dbi_entry),
          useAggressiveWriteback(p.aggr_writeback)

    {
        cout << "Hey, I am a DBICache component + Deepanjali" << endl;
        numBlksInCache = cacheSize / blkSize;
        numBlksInDBI = numBlksInCache * alpha;
        numDBIEntries = numBlksInDBI / numBlksInRegion;
        numDBISets = numDBIEntries / dbiAssoc;
        //  numDBISetsBits = log2(numDBISets);
        numBlockSizeBits = log2(blkSize);
        numBlockIndexBits = log2(numBlksInRegion);
        rdbi = new RDBI(numDBISets, numBlockSizeBits, numBlockIndexBits, dbiAssoc, numBlksInRegion, blkSize, useAggressiveWriteback);
        DPRINTF(DBICache, "Hey, I am a DBICache object");
    }

    void
    DBICache::cmpAndSwap(CacheBlk *blk, PacketPtr pkt)
    {
        assert(pkt->isRequest());

        uint64_t overwrite_val;
        bool overwrite_mem;
        uint64_t condition_val64;
        uint32_t condition_val32;

        int offset = pkt->getOffset(blkSize);
        uint8_t *blk_data = blk->data + offset;

        assert(sizeof(uint64_t) >= pkt->getSize());

        // Get a copy of the old block's contents for the probe before the update
        DataUpdate data_update(regenerateBlkAddr(blk), blk->isSecure());
        if (ppDataUpdate->hasListeners())
        {
            data_update.oldData = std::vector<uint64_t>(blk->data,
                                                        blk->data + (blkSize / sizeof(uint64_t)));
        }

        overwrite_mem = true;
        // keep a copy of our possible write value, and copy what is at the
        // memory address into the packet
        pkt->writeData((uint8_t *)&overwrite_val);
        pkt->setData(blk_data);

        if (pkt->req->isCondSwap())
        {
            if (pkt->getSize() == sizeof(uint64_t))
            {
                condition_val64 = pkt->req->getExtraData();
                overwrite_mem = !std::memcmp(&condition_val64, blk_data,
                                             sizeof(uint64_t));
            }
            else if (pkt->getSize() == sizeof(uint32_t))
            {
                condition_val32 = (uint32_t)pkt->req->getExtraData();
                overwrite_mem = !std::memcmp(&condition_val32, blk_data,
                                             sizeof(uint32_t));
            }
            else
                panic("Invalid size for conditional read/write\n");
        }

        PacketList writebacks;
        if (overwrite_mem)
        {
            std::memcpy(blk_data, &overwrite_val, pkt->getSize());

            rdbi->setDirtyBit(pkt, blk, writebacks);

            if (rdbi->isDirty(pkt))
            {
                cout << pkt->getAddr() << endl;
            }
            if (ppDataUpdate->hasListeners())
            {
                data_update.newData = std::vector<uint64_t>(blk->data,
                                                            blk->data + (blkSize / sizeof(uint64_t)));
                ppDataUpdate->notify(data_update);
            }
        }
    }

    void
    DBICache::satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                             bool deferred_response, bool pending_downgrade)
    {
        BaseCache::satisfyRequest(pkt, blk);

        PacketList writebacks;

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

                    if (rdbi->isDirty(pkt))
                    {
                        pkt->setCacheResponding();
                        rdbi->clearDirtyBit(pkt, writebacks);
                    }
                }
                else if (blk->isSet(CacheBlk::WritableBit) &&
                         !pending_downgrade && !pkt->hasSharers() &&
                         pkt->cmd != MemCmd::ReadCleanReq)
                {

                    if (rdbi->isDirty(pkt))
                    {
                        cout << pkt->getAddr() << endl;
                        if (!deferred_response)
                        {
                            pkt->setCacheResponding();
                            rdbi->clearDirtyBit(pkt, writebacks);
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
    DBICache::serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt, CacheBlk *blk)
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
            // This is the fake response generated by the write half of the RMW;
            // see comments in recvTimingReq().  The first target on the list
            // should be the LockedRMWReadReq which has already been satisfied,
            // either because it was a hit (and the MSHR was allocated in
            // recvTimingReq()) or because it was left there after the inital
            // response in extractServiceableTargets. In either case, we
            // don't need to respond now, so pop it off to prevent the loop
            // below from generating another response.
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
                        // All other copies of the block were invalidated and we
                        // have an exclusive copy.

                        // The coherence protocol assumes that if we fetched an
                        // exclusive copy of the block, we have the intention to
                        // modify it. Therefore the MSHR for the PrefetchExReq has
                        // been the point of ordering and this cache has commited
                        // to respond to snoops for the block.
                        //
                        // In most cases this is true anyway - a PrefetchExReq
                        // will be followed by a WriteReq. However, if that
                        // doesn't happen, the block is not marked as dirty and
                        // the cache doesn't respond to snoops that has committed
                        // to do so.
                        //
                        // To avoid deadlocks in cases where there is a snoop
                        // between the PrefetchExReq and the expected WriteReq, we
                        // proactively mark the block as Dirty.
                        assert(blk);

                        // blk->setCoherenceBits(CacheBlk::DirtyBit);
                        // setBlkCoherenceBits(pkt->getAddr(), CacheBlk::DirtyBit);
                        // setBlkCoherenceBits(blk, CacheBlk::DirtyBit);
                        PacketList writebacks;

                        rdbi->setDirtyBit(pkt, blk, writebacks);

                        // do_writebacks(writebacks); Edited by Vivek

                        if (rdbi->isDirty(pkt))
                        {
                            cout << pkt->getAddr() << endl;
                        }

                        panic_if(isReadOnly, "Prefetch exclusive requests from "
                                             "read-only cache %s\n",
                                 name());
                    }

                    // a software prefetch would have already been ack'd
                    // immediately with dummy data so the core would be able to
                    // retire it. This request completes right here, so we
                    // deallocate it.
                    delete tgt_pkt;
                    break; // skip response
                }

                // unlike the other packet flows, where data is found in other
                // caches or memory and brought back, write-line requests always
                // have the data right away, so the above check for "is fill?"
                // cannot actually be determined until examining the stored MSHR
                // state. We "catch up" with that logic here, which is duplicated
                // from above.
                if (tgt_pkt->cmd == MemCmd::WriteLineReq)
                {
                    assert(!is_error);
                    assert(blk);
                    assert(blk->isSet(CacheBlk::WritableBit));
                }

                // Here we decide whether we will satisfy the target using
                // data from the block or from the response. We use the
                // block data to satisfy the request when the block is
                // present and valid and in addition the response in not
                // forwarding data to the cache above (we didn't fill
                // either); otherwise we use the packet data.
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

    CacheBlk *
    DBICache::handleFill(PacketPtr pkt, CacheBlk *blk, PacketList &writebacks,
                         bool allocate)
    {
        assert(pkt->isResponse());
        Addr addr = pkt->getAddr();
        bool is_secure = pkt->isSecure();
        const bool has_old_data = blk && blk->isValid();
        const std::string old_state = (debug::Cache && blk) ? blk->print() : "";

        // When handling a fill, we should have no writes to this line.
        assert(addr == pkt->getBlockAddr(blkSize));
        assert(!writeBuffer.findMatch(addr, is_secure));

        if (!blk)
        {
            // better have read new data...
            assert(pkt->hasData() || pkt->cmd == MemCmd::InvalidateResp);

            // need to do a replacement if allocating, otherwise we stick
            // with the temporary storage
            blk = allocate ? allocateBlock(pkt, writebacks) : nullptr;

            if (!blk)
            {
                // No replaceable block or a mostly exclusive
                // cache... just use temporary storage to complete the
                // current request and then get rid of it
                blk = tempBlock;
                tempBlock->insert(addr, is_secure);
                DPRINTF(Cache, "using temp block for %#llx (%s)\n", addr,
                        is_secure ? "s" : "ns");
            }
        }
        else
        {
            // existing block... probably an upgrade
            // don't clear block status... if block is already dirty we
            // don't want to lose that
        }

        // Block is guaranteed to be valid at this point
        assert(blk->isValid());
        assert(blk->isSecure() == is_secure);
        assert(regenerateBlkAddr(blk) == addr);

        // blk->setCoherenceBits(CacheBlk::ReadableBit);
        // setBlkCoherenceBits(pkt->getAddr(), CacheBlk::DirtyBit);

        rdbi->setDirtyBit(pkt, blk, writebacks);

        // sanity check for whole-line writes, which should always be
        // marked as writable as part of the fill, and then later marked
        // dirty as part of satisfyRequest
        if (pkt->cmd == MemCmd::InvalidateResp)
        {
            assert(!pkt->hasSharers());
        }

        // here we deal with setting the appropriate state of the line,
        // and we start by looking at the hasSharers flag, and ignore the
        // cacheResponding flag (normally signalling dirty data) if the
        // packet has sharers, thus the line is never allocated as Owned
        // (dirty but not writable), and always ends up being either
        // Shared, Exclusive or Modified, see Packet::setCacheResponding
        // for more details
        if (!pkt->hasSharers())
        {
            // we could get a writable line from memory (rather than a
            // cache) even in a read-only cache, note that we set this bit
            // even for a read-only cache, possibly revisit this decision
            blk->setCoherenceBits(CacheBlk::WritableBit);

            // check if we got this via cache-to-cache transfer (i.e., from a
            // cache that had the block in Modified or Owned state)
            if (pkt->cacheResponding())
            {
                // we got the block in Modified state, and invalidated the
                // owners copy

                // blk->setCoherenceBits(CacheBlk::DirtyBit);
                // setBlkCoherenceBits(pkt->getAddr(), CacheBlk::DirtyBit);
                rdbi->setDirtyBit(pkt, blk, writebacks);

                // insertIntoToyStore(pkt->getAddr(), true);
                // if (blk->isSet(CacheBlk::DirtyBit))
                // {
                //     cout << pkt->getAddr() << endl;
                // }

                if (rdbi->isDirty(pkt))
                {
                    cout << pkt->getAddr() << endl;
                }

                gem5_assert(!isReadOnly, "Should never see dirty snoop response "
                                         "in read-only cache %s\n",
                            name());
            }
        }

        DPRINTF(Cache, "Block addr %#llx (%s) moving from %s to %s\n",
                addr, is_secure ? "s" : "ns", old_state, blk->print());

        // if we got new data, copy it in (checking for a read response
        // and a response that has data is the same in the end)
        if (pkt->isRead())
        {
            // sanity checks
            assert(pkt->hasData());
            assert(pkt->getSize() == blkSize);

            updateBlockData(blk, pkt, has_old_data);
        }
        // The block will be ready when the payload arrives and the fill is done
        blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
                          pkt->payloadDelay);

        return blk;
    }
}
