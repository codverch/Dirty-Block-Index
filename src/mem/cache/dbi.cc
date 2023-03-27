
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

    // Constructor for the DBICache class
    DBICache::DBICache(const DBICacheParams &p)
        : Cache(p),
          cacheSize(p.size),                        // DBI Cache Size
          alpha(p.alpha),                           // Size of DBI in terms of cache size
          dbiAssoc(p.dbi_assoc),                    // DBI Associativity
          blkSize(p.blkSize),                       // Block Size
          numBlksInRegion(p.blk_per_dbi_entry),     // Number of blocks in a DBI entry
          useAggressiveWriteback(p.aggr_writeback), // Aggressive Writeback
          dbistats(*this, &stats)                   // DBI Cache Stats

    {
        // Print a message to the console to show that the constructor is called
        cout << "Hey, I am a DBICache component + Deepanjali" << endl;
        // Number of blocks in the cache
        numBlksInCache = cacheSize / blkSize;
        // Number of blocks in DBI
        numBlksInDBI = numBlksInCache * alpha;
        // Number of DBI entries
        numDBIEntries = (numBlksInCache * alpha) / numBlksInRegion;
        // Number of sets in DBI
        numDBISets = numDBIEntries / dbiAssoc;
        //  numDBISetsBits = log2(numDBISets);
        // Number of bits required to store the block size
        numBlockSizeBits = log2(blkSize);
        // Number of bits required to store the number of blocks in a region
        numBlockIndexBits = log2(numBlksInRegion);
        // Call the constructor of the RDBI class
        rdbi = new RDBI(numDBISets, numBlockSizeBits, numBlockIndexBits, dbiAssoc, numBlksInRegion, blkSize, useAggressiveWriteback, dbistats, *this);
    }

    // cmpAndSwap function
    void
    DBICache::cmpAndSwap(CacheBlk *blk, PacketPtr pkt)

    {
        PacketList writebacks; // TODO
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

            if (overwrite_mem)
            {
            std::memcpy(blk_data, &overwrite_val, pkt->getSize());
            // blk->setCoherenceBits(CacheBlk::DirtyBit);
            rdbi->setDirtyBit(pkt, blk, writebacks);
            // doWritebacks(writebacks, 0);

            if (ppDataUpdate->hasListeners())
            {
                data_update.newData = std::vector<uint64_t>(blk->data,
                                                            blk->data + (blkSize / sizeof(uint64_t)));
                ppDataUpdate->notify(data_update);
            }
            }
    }

    // satisfyRequest function
    void
    DBICache::satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                             bool deferred_response, bool pending_downgrade)
    {
            PacketList writebacks; // TODO
            assert(pkt->isRequest());

            assert(blk && blk->isValid());
            // Occasionally this is not true... if we are a lower-level cache
            // satisfying a string of Read and ReadEx requests from
            // upper-level caches, a Read will mark the block as shared but we
            // can satisfy a following ReadEx anyway since we can rely on the
            // Read requestor(s) to have buffered the ReadEx snoop and to
            // invalidate their blocks after receiving them.
            // assert(!pkt->needsWritable() || blk->isSet(CacheBlk::WritableBit));
            assert(pkt->getOffset(blkSize) + pkt->getSize() <= blkSize);

            // Check RMW operations first since both isRead() and
            // isWrite() will be true for them
            if (pkt->cmd == MemCmd::SwapReq)
            {
            if (pkt->isAtomicOp())
            {
                // Get a copy of the old block's contents for the probe before
                // the update
                DataUpdate data_update(regenerateBlkAddr(blk), blk->isSecure());
                if (ppDataUpdate->hasListeners())
                {
                    data_update.oldData = std::vector<uint64_t>(blk->data,
                                                                blk->data + (blkSize / sizeof(uint64_t)));
                }

                // extract data from cache and save it into the data field in
                // the packet as a return value from this atomic op
                int offset = tags->extractBlkOffset(pkt->getAddr());
                uint8_t *blk_data = blk->data + offset;
                pkt->setData(blk_data);

                // execute AMO operation
                (*(pkt->getAtomicOp()))(blk_data);

                // Inform of this block's data contents update
                if (ppDataUpdate->hasListeners())
                {
                    data_update.newData = std::vector<uint64_t>(blk->data,
                                                                blk->data + (blkSize / sizeof(uint64_t)));
                    ppDataUpdate->notify(data_update);
                }

                // set block status to dirty
                // blk->setCoherenceBits(CacheBlk::DirtyBit);
                // blk->setCoherenceBits(CacheBlk::DirtyBit);
                rdbi->setDirtyBit(pkt, blk, writebacks);
                //  doWritebacks(writebacks, 0); print warning
            }
            else
            {
                cmpAndSwap(blk, pkt);
            }
            }
            else if (pkt->isWrite())
            {
            // we have the block in a writable state and can go ahead,
            // note that the line may be also be considered writable in
            // downstream caches along the path to memory, but always
            // Exclusive, and never Modified
            assert(blk->isSet(CacheBlk::WritableBit));
            // Write or WriteLine at the first cache with block in writable state
            if (blk->checkWrite(pkt))
            {
                updateBlockData(blk, pkt, true);
            }
            // Always mark the line as dirty (and thus transition to the
            // Modified state) even if we are a failed StoreCond so we
            // supply data to any snoops that have appended themselves to
            // this cache before knowing the store will fail.

            // blk->setCoherenceBits(CacheBlk::DirtyBit);
            // blk->setCoherenceBits(CacheBlk::DirtyBit);
            rdbi->setDirtyBit(pkt, blk, writebacks);
            // doWritebacks(writebacks, 0);

            DPRINTF(CacheVerbose, "%s for %s (write)\n", __func__, pkt->print());
            }
            else if (pkt->isRead())
            {
            if (pkt->isLLSC())
            {
                blk->trackLoadLocked(pkt);
            }

            // all read responses have a data payload
            assert(pkt->hasRespData());
            pkt->setDataFromBlock(blk->data, blkSize);
            }
            else if (pkt->isUpgrade())
            {
            // sanity check
            assert(!pkt->hasSharers());

            // if (blk->isSet(CacheBlk::DirtyBit))
            // Replacing the above line with the following lines
            if (rdbi->isDirty(pkt))
            {
                // we were in the Owned state, and a cache above us that
                // has the line in Shared state needs to be made aware
                // that the data it already has is in fact dirty
                pkt->setCacheResponding();

                // blk->clearCoherenceBits(CacheBlk::DirtyBit);
                rdbi->clearDirtyBit(pkt, writebacks);
            }
            }
            else if (pkt->isClean())
            {
            // blk->clearCoherenceBits(CacheBlk::DirtyBit);
            rdbi->clearDirtyBit(pkt, writebacks);
            }
            else
            {
            assert(pkt->isInvalidate());
            invalidateBlock(blk);
            DPRINTF(CacheVerbose, "%s for %s (invalidation)\n", __func__,
                    pkt->print());
            }

            if (pkt->isRead())
            {
            // determine if this read is from a (coherent) cache or not
            if (pkt->fromCache())
            {
                assert(pkt->getSize() == blkSize);
                // special handling for coherent block requests from
                // upper-level caches
                if (pkt->needsWritable())
                {
                    // sanity check
                    assert(pkt->cmd == MemCmd::ReadExReq ||
                           pkt->cmd == MemCmd::SCUpgradeFailReq);
                    assert(!pkt->hasSharers());

                    // if we have a dirty copy, make sure the recipient
                    // keeps it marked dirty (in the modified state)

                    // if (blk->isSet(CacheBlk::DirtyBit))
                    //  Replace the above line with the following line
                    if (rdbi->isDirty(pkt))
                    {
                        pkt->setCacheResponding();
                        // blk->clearCoherenceBits(CacheBlk::DirtyBit);
                        rdbi->clearDirtyBit(pkt, writebacks);
                    }
                }
                else if (blk->isSet(CacheBlk::WritableBit) &&
                         !pending_downgrade && !pkt->hasSharers() &&
                         pkt->cmd != MemCmd::ReadCleanReq)
                {
                    // we can give the requestor a writable copy on a read
                    // request if:
                    // - we have a writable copy at this level (& below)
                    // - we don't have a pending snoop from below
                    //   signaling another read request
                    // - no other cache above has a copy (otherwise it
                    //   would have set hasSharers flag when
                    //   snooping the packet)
                    // - the read has explicitly asked for a clean
                    //   copy of the line

                    // if (blk->isSet(CacheBlk::DirtyBit))
                    //  Replace the above line with the following line
                    if (rdbi->isDirty(pkt))
                    {
                        // special considerations if we're owner:
                        if (!deferred_response)
                        {
                            // respond with the line in Modified state
                            // (cacheResponding set, hasSharers not set)
                            pkt->setCacheResponding();

                            // if this cache is mostly inclusive, we
                            // keep the block in the Exclusive state,
                            // and pass it upwards as Modified
                            // (writable and dirty), hence we have
                            // multiple caches, all on the same path
                            // towards memory, all considering the
                            // same block writable, but only one
                            // considering it Modified

                            // we get away with multiple caches (on
                            // the same path to memory) considering
                            // the block writeable as we always enter
                            // the cache hierarchy through a cache,
                            // and first snoop upwards in all other
                            // branches

                            // blk->clearCoherenceBits(CacheBlk::DirtyBit);
                            rdbi->clearDirtyBit(pkt, writebacks);
                        }
                        else
                        {
                            // if we're responding after our own miss,
                            // there's a window where the recipient didn't
                            // know it was getting ownership and may not
                            // have responded to snoops correctly, so we
                            // have to respond with a shared line
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
    DBICache::serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt,
                                 CacheBlk *blk)
    {
            PacketList writebacks; // ToDo
            // Print statement for debugging
            // cout << "Service MSHR Targets" << endl;
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
                        // blk->setCoherenceBits(CacheBlk::DirtyBit);
                        rdbi->setDirtyBit(pkt, blk, writebacks);
                        // doWritebacks(writebacks, 0);

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
    DBICache::handleFill(PacketPtr pkt, CacheBlk *blk,
                         PacketList &writebacks, bool allocate)

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

            blk->setCoherenceBits(CacheBlk::ReadableBit);

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
                // Replace the above line with the following
                rdbi->setDirtyBit(pkt, blk, writebacks);

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

    uint32_t
    DBICache::handleSnoop(PacketPtr pkt, CacheBlk *blk, bool is_timing,
                          bool is_deferred, bool pending_inval)

    {
            // Print statement for debugging
            // cout << "DBICache::handleSnoop" << endl;
            DPRINTF(CacheVerbose, "%s: for %s\n", __func__, pkt->print());
            // deferred snoops can only happen in timing mode
            assert(!(is_deferred && !is_timing));
            // pending_inval only makes sense on deferred snoops
            assert(!(pending_inval && !is_deferred));
            assert(pkt->isRequest());

            // the packet may get modified if we or a forwarded snooper
            // responds in atomic mode, so remember a few things about the
            // original packet up front
            bool invalidate = pkt->isInvalidate();
            [[maybe_unused]] bool needs_writable = pkt->needsWritable();

            // at the moment we could get an uncacheable write which does not
            // have the invalidate flag, and we need a suitable way of dealing
            // with this case
            panic_if(invalidate && pkt->req->isUncacheable(),
                     "%s got an invalidating uncacheable snoop request %s",
                     name(), pkt->print());

            uint32_t snoop_delay = 0;

            if (forwardSnoops)
            {
            // first propagate snoop upward to see if anyone above us wants to
            // handle it.  save & restore packet src since it will get
            // rewritten to be relative to CPU-side bus (if any)
            if (is_timing)
            {
                // copy the packet so that we can clear any flags before
                // forwarding it upwards, we also allocate data (passing
                // the pointer along in case of static data), in case
                // there is a snoop hit in upper levels
                Packet snoopPkt(pkt, true, true);
                snoopPkt.setExpressSnoop();
                // the snoop packet does not need to wait any additional
                // time
                snoopPkt.headerDelay = snoopPkt.payloadDelay = 0;
                cpuSidePort.sendTimingSnoopReq(&snoopPkt);

                // add the header delay (including crossbar and snoop
                // delays) of the upward snoop to the snoop delay for this
                // cache
                snoop_delay += snoopPkt.headerDelay;

                // If this request is a prefetch or clean evict and an upper level
                // signals block present, make sure to propagate the block
                // presence to the requestor.
                if (snoopPkt.isBlockCached())
                {
                    pkt->setBlockCached();
                }
                // If the request was satisfied by snooping the cache
                // above, mark the original packet as satisfied too.
                if (snoopPkt.satisfied())
                {
                    pkt->setSatisfied();
                }

                // Copy over flags from the snoop response to make sure we
                // inform the final destination
                pkt->copyResponderFlags(&snoopPkt);
            }
            else
            {
                bool already_responded = pkt->cacheResponding();
                cpuSidePort.sendAtomicSnoop(pkt);
                if (!already_responded && pkt->cacheResponding())
                {
                    // cache-to-cache response from some upper cache:
                    // forward response to original requestor
                    assert(pkt->isResponse());
                }
            }
            }

            bool respond = false;
            bool blk_valid = blk && blk->isValid();
            if (pkt->isClean())
            {

            // if (blk_valid && blk->isSet(CacheBlk::DirtyBit))
            // Replace the above line with the following
            if (blk_valid && rdbi->isDirty(pkt))
            {
                DPRINTF(CacheVerbose, "%s: packet (snoop) %s found block: %s\n",
                        __func__, pkt->print(), blk->print());
                PacketPtr wb_pkt =
                    writecleanBlk(blk, pkt->req->getDest(), pkt->id);
                PacketList writebacks;
                writebacks.push_back(wb_pkt);

                if (is_timing)
                {
                    // anything that is merely forwarded pays for the forward
                    // latency and the delay provided by the crossbar
                    Tick forward_time = clockEdge(forwardLatency) +
                                        pkt->headerDelay;
                    doWritebacks(writebacks, forward_time);
                }
                else
                {
                    doWritebacksAtomic(writebacks);
                }
                pkt->setSatisfied();
            }
            }
            else if (!blk_valid)
            {
            DPRINTF(CacheVerbose, "%s: snoop miss for %s\n", __func__,
                    pkt->print());
            if (is_deferred)
            {
                // we no longer have the block, and will not respond, but a
                // packet was allocated in MSHR::handleSnoop and we have
                // to delete it
                assert(pkt->needsResponse());

                // we have passed the block to a cache upstream, that
                // cache should be responding
                assert(pkt->cacheResponding());

                delete pkt;
            }
            return snoop_delay;
            }
            else
            {
            DPRINTF(Cache, "%s: snoop hit for %s, old state is %s\n", __func__,
                    pkt->print(), blk->print());

            // We may end up modifying both the block state and the packet (if
            // we respond in atomic mode), so just figure out what to do now
            // and then do it later. We respond to all snoops that need
            // responses provided we have the block in dirty state. The
            // invalidation itself is taken care of below. We don't respond to
            // cache maintenance operations as this is done by the destination
            // xbar.

            // respond = blk->isSet(CacheBlk::DirtyBit) && pkt->needsResponse();
            // Replace the above line with the following
            respond = rdbi->isDirty(pkt) && pkt->needsResponse();

            // gem5_assert(!(isReadOnly && blk->isSet(CacheBlk::DirtyBit)),
            //            "Should never have a dirty block in a read-only cache %s\n",
            //            name());
            // Replace the above line with the following
            gem5_assert(!(isReadOnly && rdbi->isDirty(pkt)),
                        "Should never have a dirty block in a read-only cache %s\n",
                        name());
            }

            // Invalidate any prefetch's from below that would strip write permissions
            // MemCmd::HardPFReq is only observed by upstream caches.  After missing
            // above and in it's own cache, a new MemCmd::ReadReq is created that
            // downstream caches observe.
            if (pkt->mustCheckAbove())
            {
            DPRINTF(Cache, "Found addr %#llx in upper level cache for snoop %s "
                           "from lower cache\n",
                    pkt->getAddr(), pkt->print());
            pkt->setBlockCached();
            return snoop_delay;
            }

            if (pkt->isRead() && !invalidate)
            {
            // reading without requiring the line in a writable state
            assert(!needs_writable);
            pkt->setHasSharers();

            // if the requesting packet is uncacheable, retain the line in
            // the current state, otherwhise unset the writable flag,
            // which means we go from Modified to Owned (and will respond
            // below), remain in Owned (and will respond below), from
            // Exclusive to Shared, or remain in Shared
            if (!pkt->req->isUncacheable())
            {
                blk->clearCoherenceBits(CacheBlk::WritableBit);
            }
            DPRINTF(Cache, "new state is %s\n", blk->print());
            }

            if (respond)
            {
            // prevent anyone else from responding, cache as well as
            // memory, and also prevent any memory from even seeing the
            // request
            pkt->setCacheResponding();
            if (!pkt->isClean() && blk->isSet(CacheBlk::WritableBit))
            {
                // inform the cache hierarchy that this cache had the line
                // in the Modified state so that we avoid unnecessary
                // invalidations (see Packet::setResponderHadWritable)
                pkt->setResponderHadWritable();

                // in the case of an uncacheable request there is no point
                // in setting the responderHadWritable flag, but since the
                // recipient does not care there is no harm in doing so
            }
            else
            {
                // if the packet has needsWritable set we invalidate our
                // copy below and all other copies will be invalidates
                // through express snoops, and if needsWritable is not set
                // we already called setHasSharers above
            }

            // if we are returning a writable and dirty (Modified) line,
            // we should be invalidating the line
            panic_if(!invalidate && !pkt->hasSharers(),
                     "%s is passing a Modified line through %s, "
                     "but keeping the block",
                     name(), pkt->print());

            if (is_timing)
            {
                doTimingSupplyResponse(pkt, blk->data, is_deferred, pending_inval);
            }
            else
            {
                pkt->makeAtomicResponse();
                // packets such as upgrades do not actually have any data
                // payload
                if (pkt->hasData())
                    pkt->setDataFromBlock(blk->data, blkSize);
            }

            // When a block is compressed, it must first be decompressed before
            // being read, and this increases the snoop delay.
            if (compressor && pkt->isRead())
            {
                snoop_delay += compressor->getDecompressionLatency(blk);
            }
            }

            if (!respond && is_deferred)
            {
            assert(pkt->needsResponse());
            delete pkt;
            }

            // Do this last in case it deallocates block data or something
            // like that
            if (blk_valid && invalidate)
            {
            invalidateBlock(blk);
            DPRINTF(Cache, "new state is %s\n", blk->print());
            }

            return snoop_delay;
    }

    Tick
    DBICache::recvAtomic(PacketPtr pkt)
    {
            // Print statement for debugging
            // cout << "DBICache::recvAtomic" << endl;
            // should assert here that there are no outstanding MSHRs or
            // writebacks... that would mean that someone used an atomic
            // access in timing mode

            // We use lookupLatency here because it is used to specify the latency
            // to access.
            Cycles lat = lookupLatency;

            CacheBlk *blk = nullptr;
            PacketList writebacks;
            bool satisfied = access(pkt, blk, lat, writebacks);

            // if (pkt->isClean() && blk && blk->isSet(CacheBlk::DirtyBit))
            // Replace the above line with the following
            if (pkt->isClean() && blk && rdbi->isDirty(pkt))
            {
            // A cache clean opearation is looking for a dirty
            // block. If a dirty block is encountered a WriteClean
            // will update any copies to the path to the memory
            // until the point of reference.
            DPRINTF(CacheVerbose, "%s: packet %s found block: %s\n",
                    __func__, pkt->print(), blk->print());
            PacketPtr wb_pkt = writecleanBlk(blk, pkt->req->getDest(), pkt->id);
            writebacks.push_back(wb_pkt);
            pkt->setSatisfied();
            }

            // handle writebacks resulting from the access here to ensure they
            // logically precede anything happening below
            doWritebacksAtomic(writebacks);
            assert(writebacks.empty());

            if (!satisfied)
            {
            lat += handleAtomicReqMiss(pkt, blk, writebacks);
            }

            // Note that we don't invoke the prefetcher at all in atomic mode.
            // It's not clear how to do it properly, particularly for
            // prefetchers that aggressively generate prefetch candidates and
            // rely on bandwidth contention to throttle them; these will tend
            // to pollute the cache in atomic mode since there is no bandwidth
            // contention.  If we ever do want to enable prefetching in atomic
            // mode, though, this is the place to do it... see timingAccess()
            // for an example (though we'd want to issue the prefetch(es)
            // immediately rather than calling requestMemSideBus() as we do
            // there).

            // do any writebacks resulting from the response handling
            doWritebacksAtomic(writebacks);

            // if we used temp block, check to see if its valid and if so
            // clear it out, but only do so after the call to recvAtomic is
            // finished so that any downstream observers (such as a snoop
            // filter), first see the fill, and only then see the eviction
            if (blk == tempBlock && tempBlock->isValid())
            {
            // the atomic CPU calls recvAtomic for fetch and load/store
            // sequentuially, and we may already have a tempBlock
            // writeback from the fetch that we have not yet sent
            if (tempBlockWriteback)
            {
                // if that is the case, write the prevoius one back, and
                // do not schedule any new event
                writebackTempBlockAtomic();
            }
            else
            {
                // the writeback/clean eviction happens after the call to
                // recvAtomic has finished (but before any successive
                // calls), so that the response handling from the fill is
                // allowed to happen first
                schedule(writebackTempBlockAtomicEvent, curTick());
            }

            tempBlockWriteback = evictBlock(blk);
            }

            if (pkt->needsResponse())
            {
            pkt->makeAtomicResponse();
            }

            return lat * clockPeriod();
    }

    bool
    DBICache::access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                     PacketList &writebacks)
    {
            // Print statement for debugging
            // cout << "DBICache::access" << endl;

            if (pkt->req->isUncacheable())
            {
            assert(pkt->isRequest());

            gem5_assert(!(isReadOnly && pkt->isWrite()),
                        "Should never see a write in a read-only cache %s\n",
                        name());

            DPRINTF(Cache, "%s for %s\n", __func__, pkt->print());

            // flush and invalidate any existing block
            CacheBlk *old_blk(tags->findBlock(pkt->getAddr(), pkt->isSecure()));
            if (old_blk && old_blk->isValid())
            {
                BaseCache::evictBlock(old_blk, writebacks);
            }

            blk = nullptr;
            // lookupLatency is the latency in case the request is uncacheable.
            lat = lookupLatency;
            return false;
            }

            // Replacing call to the base cache access() with the entire code

            // sanity check
            assert(pkt->isRequest());

            gem5_assert(!(isReadOnly && pkt->isWrite()),
                        "Should never see a write in a read-only cache %s\n",
                        name());

            // Access block in the tags
            Cycles tag_latency(0);
            blk = tags->accessBlock(pkt, tag_latency);

            DPRINTF(Cache, "%s for %s %s\n", __func__, pkt->print(),
                    blk ? "hit " + blk->print() : "miss");

            if (pkt->req->isCacheMaintenance())
            {
            // A cache maintenance operation is always forwarded to the
            // memory below even if the block is found in dirty state.

            // We defer any changes to the state of the block until we
            // create and mark as in service the mshr for the downstream
            // packet.

            // Calculate access latency on top of when the packet arrives. This
            // takes into account the bus delay.
            lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

            return false;
            }

            if (pkt->isEviction())
            {
            // We check for presence of block in above caches before issuing
            // Writeback or CleanEvict to write buffer. Therefore the only
            // possible cases can be of a CleanEvict packet coming from above
            // encountering a Writeback generated in this cache peer cache and
            // waiting in the write buffer. Cases of upper level peer caches
            // generating CleanEvict and Writeback or simply CleanEvict and
            // CleanEvict almost simultaneously will be caught by snoops sent out
            // by crossbar.
            WriteQueueEntry *wb_entry = writeBuffer.findMatch(pkt->getAddr(),
                                                              pkt->isSecure());
            if (wb_entry)
            {
                assert(wb_entry->getNumTargets() == 1);
                PacketPtr wbPkt = wb_entry->getTarget()->pkt;
                assert(wbPkt->isWriteback());

                if (pkt->isCleanEviction())
                {
                    // The CleanEvict and WritebackClean snoops into other
                    // peer caches of the same level while traversing the
                    // crossbar. If a copy of the block is found, the
                    // packet is deleted in the crossbar. Hence, none of
                    // the other upper level caches connected to this
                    // cache have the block, so we can clear the
                    // BLOCK_CACHED flag in the Writeback if set and
                    // discard the CleanEvict by returning true.
                    wbPkt->clearBlockCached();

                    // A clean evict does not need to access the data array
                    lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

                    return true;
                }
                else
                {
                    assert(pkt->cmd == MemCmd::WritebackDirty);
                    // Dirty writeback from above trumps our clean
                    // writeback... discard here
                    // Note: markInService will remove entry from writeback buffer.
                    markInService(wb_entry);
                    delete wbPkt;
                }
            }
            }

            // The critical latency part of a write depends only on the tag access
            if (pkt->isWrite())
            {
            lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);
            }

            // Writeback handling is special case.  We can write the block into
            // the cache without having a writeable copy (or any copy at all).
            if (pkt->isWriteback())
            {
            assert(blkSize == pkt->getSize());

            // we could get a clean writeback while we are having
            // outstanding accesses to a block, do the simple thing for
            // now and drop the clean writeback so that we do not upset
            // any ordering/decisions about ownership already taken
            if (pkt->cmd == MemCmd::WritebackClean &&
                mshrQueue.findMatch(pkt->getAddr(), pkt->isSecure()))
            {
                DPRINTF(Cache, "Clean writeback %#llx to block with MSHR, "
                               "dropping\n",
                        pkt->getAddr());

                // A writeback searches for the block, then writes the data.
                // As the writeback is being dropped, the data is not touched,
                // and we just had to wait for the time to find a match in the
                // MSHR. As of now assume a mshr queue search takes as long as
                // a tag lookup for simplicity.
                return true;
            }

            const bool has_old_data = blk && blk->isValid();
            if (!blk)
            {
                // need to do a replacement
                blk = allocateBlock(pkt, writebacks);
                if (!blk)
                {
                    // no replaceable block available: give up, fwd to next level.
                    incMissCount(pkt);
                    return false;
                }

                blk->setCoherenceBits(CacheBlk::ReadableBit);
            }
            else if (compressor)
            {
                // This is an overwrite to an existing block, therefore we need
                // to check for data expansion (i.e., block was compressed with
                // a smaller size, and now it doesn't fit the entry anymore).
                // If that is the case we might need to evict blocks.
                if (!updateCompressionData(blk, pkt->getConstPtr<uint64_t>(),
                                           writebacks))
                {
                    invalidateBlock(blk);
                    return false;
                }
            }

            // only mark the block dirty if we got a writeback command,
            // and leave it as is for a clean writeback
            if (pkt->cmd == MemCmd::WritebackDirty)
            {
                // TODO: the coherent cache can assert that the dirty bit is set

                // blk->setCoherenceBits(CacheBlk::DirtyBit);
                // Replace the above line with the following

                rdbi->setDirtyBit(pkt, blk, writebacks);
            }
            // if the packet does not have sharers, it is passing
            // writable, and we got the writeback in Modified or Exclusive
            // state, if not we are in the Owned or Shared state
            if (!pkt->hasSharers())
            {
                blk->setCoherenceBits(CacheBlk::WritableBit);
            }
            // nothing else to do; writeback doesn't expect response
            assert(!pkt->needsResponse());

            updateBlockData(blk, pkt, has_old_data);
            DPRINTF(Cache, "%s new state is %s\n", __func__, blk->print());
            incHitCount(pkt);

            // When the packet metadata arrives, the tag lookup will be done while
            // the payload is arriving. Then the block will be ready to access as
            // soon as the fill is done
            blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
                              std::max(cyclesToTicks(tag_latency), (uint64_t)pkt->payloadDelay));

            return true;
            }
            else if (pkt->cmd == MemCmd::CleanEvict)
            {
            // A CleanEvict does not need to access the data array
            lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

            if (blk)
            {
                // Found the block in the tags, need to stop CleanEvict from
                // propagating further down the hierarchy. Returning true will
                // treat the CleanEvict like a satisfied write request and delete
                // it.
                return true;
            }
            // We didn't find the block here, propagate the CleanEvict further
            // down the memory hierarchy. Returning false will treat the CleanEvict
            // like a Writeback which could not find a replaceable block so has to
            // go to next level.
            return false;
            }
            else if (pkt->cmd == MemCmd::WriteClean)
            {
            // WriteClean handling is a special case. We can allocate a
            // block directly if it doesn't exist and we can update the
            // block immediately. The WriteClean transfers the ownership
            // of the block as well.
            assert(blkSize == pkt->getSize());

            const bool has_old_data = blk && blk->isValid();
            if (!blk)
            {
                if (pkt->writeThrough())
                {
                    // if this is a write through packet, we don't try to
                    // allocate if the block is not present
                    return false;
                }
                else
                {
                    // a writeback that misses needs to allocate a new block
                    blk = allocateBlock(pkt, writebacks);
                    if (!blk)
                    {
                        // no replaceable block available: give up, fwd to
                        // next level.
                        incMissCount(pkt);
                        return false;
                    }

                    blk->setCoherenceBits(CacheBlk::ReadableBit);
                }
            }
            else if (compressor)
            {
                // This is an overwrite to an existing block, therefore we need
                // to check for data expansion (i.e., block was compressed with
                // a smaller size, and now it doesn't fit the entry anymore).
                // If that is the case we might need to evict blocks.
                if (!updateCompressionData(blk, pkt->getConstPtr<uint64_t>(),
                                           writebacks))
                {
                    invalidateBlock(blk);
                    return false;
                }
            }

            // at this point either this is a writeback or a write-through
            // write clean operation and the block is already in this
            // cache, we need to update the data and the block flags
            assert(blk);
            // TODO: the coherent cache can assert that the dirty bit is set
            if (!pkt->writeThrough())
            {
                // blk->setCoherenceBits(CacheBlk::DirtyBit);
                // Replace the above line with the following
                rdbi->setDirtyBit(pkt, blk, writebacks);
            }
            // nothing else to do; writeback doesn't expect response
            assert(!pkt->needsResponse());

            updateBlockData(blk, pkt, has_old_data);
            DPRINTF(Cache, "%s new state is %s\n", __func__, blk->print());

            incHitCount(pkt);

            // When the packet metadata arrives, the tag lookup will be done while
            // the payload is arriving. Then the block will be ready to access as
            // soon as the fill is done
            blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
                              std::max(cyclesToTicks(tag_latency), (uint64_t)pkt->payloadDelay));

            // If this a write-through packet it will be sent to cache below
            return !pkt->writeThrough();
            }
            else if (blk && (pkt->needsWritable() ? blk->isSet(CacheBlk::WritableBit) : blk->isSet(CacheBlk::ReadableBit)))
            {
            // OK to satisfy access
            incHitCount(pkt);

            // Calculate access latency based on the need to access the data array
            if (pkt->isRead())
            {
                lat = calculateAccessLatency(blk, pkt->headerDelay, tag_latency);

                // When a block is compressed, it must first be decompressed
                // before being read. This adds to the access latency.
                if (compressor)
                {
                    lat += compressor->getDecompressionLatency(blk);
                }
            }
            else
            {
                lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);
            }

            satisfyRequest(pkt, blk);
            maintainClusivity(pkt->fromCache(), blk);

            return true;
            }

            // Can't satisfy access normally... either no block (blk == nullptr)
            // or have block but need writable

            incMissCount(pkt);

            lat = calculateAccessLatency(blk, pkt->headerDelay, tag_latency);

            if (!blk && pkt->isLLSC() && pkt->isWrite())
            {
            // complete miss on store conditional... just give up now
            pkt->req->setExtraData(0);
            return true;
            }

            return false;
    }

    void
    DBICache::functionalAccess(PacketPtr pkt, bool from_cpu_side)
    {
            // Print statement for debugging
            // cout << "DBICache::functionalAccess" << endl;
            Addr blk_addr = pkt->getBlockAddr(blkSize);
            bool is_secure = pkt->isSecure();
            CacheBlk *blk = tags->findBlock(pkt->getAddr(), is_secure);
            MSHR *mshr = mshrQueue.findMatch(blk_addr, is_secure);

            pkt->pushLabel(name());

            CacheBlkPrintWrapper cbpw(blk);

            // Note that just because an L2/L3 has valid data doesn't mean an
            // L1 doesn't have a more up-to-date modified copy that still
            // needs to be found.  As a result we always update the request if
            // we have it, but only declare it satisfied if we are the owner.

            // see if we have data at all (owned or otherwise)
            bool have_data = blk && blk->isValid() && pkt->trySatisfyFunctional(&cbpw, blk_addr, is_secure, blkSize, blk->data);

            // data we have is dirty if marked as such or if we have an
            // in-service MSHR that is pending a modified line

            // bool have_dirty =
            //     have_data && (blk->isSet(CacheBlk::DirtyBit) ||
            //                   (mshr && mshr->inService && mshr->isPendingModified()));

            // Replacing the above line with the following
            bool have_dirty =
                have_data && (rdbi->isDirty(pkt) ||
                              (mshr && mshr->inService && mshr->isPendingModified()));

            bool done = have_dirty ||
                        cpuSidePort.trySatisfyFunctional(pkt) ||
                        mshrQueue.trySatisfyFunctional(pkt) ||
                        writeBuffer.trySatisfyFunctional(pkt) ||
                        memSidePort.trySatisfyFunctional(pkt);

            DPRINTF(CacheVerbose, "%s: %s %s%s%s\n", __func__, pkt->print(),
                    (blk && blk->isValid()) ? "valid " : "",
                    have_data ? "data " : "", done ? "done " : "");

            // We're leaving the cache, so pop cache->name() label
            pkt->popLabel();

            if (done)
            {
            pkt->makeResponse();
            }
            else
            {
            // if it came as a request from the CPU side then make sure it
            // continues towards the memory side
            if (from_cpu_side)
            {
                memSidePort.sendFunctional(pkt);
            }
            else if (cpuSidePort.isSnooping())
            {
                // if it came from the memory side, it must be a snoop request
                // and we should only forward it if we are forwarding snoops
                cpuSidePort.sendFunctionalSnoop(pkt);
            }
            }
    }

    bool
    DBICache::sendMSHRQueuePacket(MSHR *mshr)
    {
            // Print statement for debugging
            // cout << "DBICache::sendMSHRQueuePacket" << endl;
            assert(mshr);

            // use request from 1st target
            PacketPtr tgt_pkt = mshr->getTarget()->pkt;

            DPRINTF(Cache, "%s: MSHR %s\n", __func__, tgt_pkt->print());

            // if the cache is in write coalescing mode or (additionally) in
            // no allocation mode, and we have a write packet with an MSHR
            // that is not a whole-line write (due to incompatible flags etc),
            // then reset the write mode
            if (writeAllocator && writeAllocator->coalesce() && tgt_pkt->isWrite())
            {
            if (!mshr->isWholeLineWrite())
            {
                // if we are currently write coalescing, hold on the
                // MSHR as many cycles extra as we need to completely
                // write a cache line
                if (writeAllocator->delay(mshr->blkAddr))
                {
                    Tick delay = blkSize / tgt_pkt->getSize() * clockPeriod();
                    DPRINTF(CacheVerbose, "Delaying pkt %s %llu ticks to allow "
                                          "for write coalescing\n",
                            tgt_pkt->print(), delay);
                    mshrQueue.delay(mshr, delay);
                    return false;
                }
                else
                {
                    writeAllocator->reset();
                }
            }
            else
            {
                writeAllocator->resetDelay(mshr->blkAddr);
            }
            }

            CacheBlk *blk = tags->findBlock(mshr->blkAddr, mshr->isSecure);

            // either a prefetch that is not present upstream, or a normal
            // MSHR request, proceed to get the packet to send downstream
            PacketPtr pkt = createMissPacket(tgt_pkt, blk, mshr->needsWritable(),
                                             mshr->isWholeLineWrite());

            mshr->isForward = (pkt == nullptr);

            if (mshr->isForward)
            {
            // not a cache block request, but a response is expected
            // make copy of current packet to forward, keep current
            // copy for response handling
            pkt = new Packet(tgt_pkt, false, true);
            assert(!pkt->isWrite());
            }

            // play it safe and append (rather than set) the sender state,
            // as forwarded packets may already have existing state
            pkt->pushSenderState(mshr);

            // if (pkt->isClean() && blk && blk->isSet(CacheBlk::DirtyBit))
            // Replacing the above line with the following
            if (pkt->isClean() && rdbi->isDirty(pkt))
            {
            // A cache clean opearation is looking for a dirty block. Mark
            // the packet so that the destination xbar can determine that
            // there will be a follow-up write packet as well.
            pkt->setSatisfied();
            }

            if (!memSidePort.sendTimingReq(pkt))
            {
            // we are awaiting a retry, but we
            // delete the packet and will be creating a new packet
            // when we get the opportunity
            delete pkt;

            // note that we have now masked any requestBus and
            // schedSendEvent (we will wait for a retry before
            // doing anything), and this is so even if we do not
            // care about this packet and might override it before
            // it gets retried
            return true;
            }
            else
            {
            // As part of the call to sendTimingReq the packet is
            // forwarded to all neighbouring caches (and any caches
            // above them) as a snoop. Thus at this point we know if
            // any of the neighbouring caches are responding, and if
            // so, we know it is dirty, and we can determine if it is
            // being passed as Modified, making our MSHR the ordering
            // point
            bool pending_modified_resp = !pkt->hasSharers() &&
                                         pkt->cacheResponding();
            markInService(mshr, pending_modified_resp);

            // if (pkt->isClean() && blk && blk->isSet(CacheBlk::DirtyBit))
            // Replacing the above line with the following
            if (pkt->isClean() && blk && rdbi->isDirty(pkt))
            {
                // A cache clean opearation is looking for a dirty
                // block. If a dirty block is encountered a WriteClean
                // will update any copies to the path to the memory
                // until the point of reference.
                DPRINTF(CacheVerbose, "%s: packet %s found block: %s\n",
                        __func__, pkt->print(), blk->print());
                PacketPtr wb_pkt = writecleanBlk(blk, pkt->req->getDest(),
                                                 pkt->id);
                PacketList writebacks;
                writebacks.push_back(wb_pkt);
                doWritebacks(writebacks, 0);
            }

            return false;
            }
    }
}