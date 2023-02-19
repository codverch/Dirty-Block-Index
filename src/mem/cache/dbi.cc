
#include <cmath>

#include "mem/cache/dbi_cache_stats.hh"
#include "base/stats/units.hh"
#include "base/statistics.hh"
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
          useAggressiveWriteback(p.aggr_writeback),
          // Initialize the DBI cache stats
          dbistats(*this, &stats)

    {
        DPRINTF(DBICache, "Hey, I am a DBICache component");
        numBlksInCache = cacheSize / blkSize;
        numBlksInDBI = numBlksInCache * alpha;
        numDBIEntries = numBlksInDBI / numBlksInRegion;
        numDBISets = numDBIEntries / dbiAssoc;
        //  numDBISetsBits = log2(numDBISets);
        numBlockSizeBits = log2(blkSize);
        numBlockIndexBits = log2(numBlksInRegion);
        rdbi = new RDBI(numDBISets, numBlockSizeBits, numBlockIndexBits,
                        dbiAssoc, numBlksInRegion, blkSize,
                        useAggressiveWriteback, dbistats);
        // For debugging purposes - update the dbi cache stats
        dbistats.writebacksGenerated++;
    }

    PacketPtr
    DBICache::getCustomPkt()
    {
        return customPkt;
    }

    CacheBlk *
    DBICache::getCustomBlk()
    {
        return customBlk;
    }

    void
    DBICache::setCustomPkt(PacketPtr pkt)
    {
        customPkt = pkt;
    }

    void
    DBICache::setCustomBlk(CacheBlk *blk)
    {
        customBlk = blk;
    }

    void
    DBICache::cmpAndSwap(CacheBlk *blk, PacketPtr pkt)
    {
        PacketList writebacks; // NEEDS TO BE REMOVED

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
            // Print the cacheblock address
            DPRINTF(DBICache, "Debugging(Yo): Packet CacheBlock address: %d", blk->getTag());
            rdbi->setDirtyBit(pkt, blk, writebacks);

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
        PacketList writebacks; // NEEDS TO BE REMOVED
        setCustomPkt(pkt);     // NEEDS TO BE REMOVED
        cout << "DBICache::satisfyRequest, packet address: " << pkt->getAddr() << endl;
        BaseCache::satisfyRequest(pkt, blk);
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

    bool
    DBICache::access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                     PacketList &writebacks)
    {
        if (pkt->req->isUncacheable())
        {
            assert(pkt->isRequest());

            gem5_assert(!(isReadOnly && pkt->isWrite()),
                        "Should never see a write in a read-only cache %s\n",
                        name());

            DPRINTF(DBICache, "%s for %s\n", __func__, pkt->print());

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

        // Adding the BaseCache::access() code instead of call() here to make the necessary changes
        // sanity check
        assert(pkt->isRequest());

        gem5_assert(!(isReadOnly && pkt->isWrite()),
                    "Should never see a write in a read-only cache %s\n",
                    name());

        // Access block in the tags
        Cycles tag_latency(0);
        blk = tags->accessBlock(pkt, tag_latency);

        DPRINTF(DBICache, "%s for %s %s\n", __func__, pkt->print(),
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
                DPRINTF(DBICache, "Clean writeback %#llx to block with MSHR, "
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
                DPRINTF(DBICache, "DEEAIZSHELL2 CacheBlock address: %d", blk->getTag());
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
            DPRINTF(DBICache, "%s new state is %s\n", __func__, blk->print());
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
                DPRINTF(DBICache, "DEEAIZSHELL3 CacheBlock address: %d", blk->getTag());
                rdbi->setDirtyBit(pkt, blk, writebacks);
            }
            // nothing else to do; writeback doesn't expect response
            assert(!pkt->needsResponse());
            updateBlockData(blk, pkt, has_old_data);
            DPRINTF(DBICache, "%s new state is %s\n", __func__, blk->print());

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

            satisfyRequest(pkt, blk, false, false);
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
                DPRINTF(DBICache, "DEEAIZSHELL4 CacheBlock address: %d", blk->getTag());
                rdbi->setDirtyBit(pkt, blk, writebacks);

                gem5_assert(!isReadOnly, "Should never see dirty snoop response "
                                         "in read-only cache %s\n",
                            name());
            }
        }

        DPRINTF(DBICache, "Block addr %#llx (%s) moving from %s to %s\n",
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

    void
    DBICache::serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt, CacheBlk *blk)
    {
        PacketList writebacks;

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
                        DPRINTF(DBICache, "DEEAIZSHELL5 CacheBlock address: %d", blk->getTag());
                        rdbi->setDirtyBit(pkt, blk, writebacks);

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

    PacketPtr
    DBICache::writebackBlk(CacheBlk *blk)
    {
        PacketPtr temp_pkt = getCustomPkt();
        gem5_assert(!isReadOnly || writebackClean,
                    "Writeback from read-only cache");
        //  assert(blk && blk->isValid() &&
        //         (blk->isSet(CacheBlk::DirtyBit) || writebackClean));
        assert(blk && blk->isValid() &&
               (rdbi->isDirty(temp_pkt) || writebackClean));

        stats.writebacks[Request::wbRequestorId]++;

        RequestPtr req = std::make_shared<Request>(
            regenerateBlkAddr(blk), blkSize, 0, Request::wbRequestorId);

        if (blk->isSecure())
            req->setFlags(Request::SECURE);

        req->taskId(blk->getTaskId());

        //  PacketPtr pkt =
        //      new Packet(req, blk->isSet(CacheBlk::DirtyBit) ? MemCmd::WritebackDirty : MemCmd::WritebackClean);
        PacketPtr pkt =
            new Packet(req, rdbi->isDirty(pkt) ? MemCmd::WritebackDirty : MemCmd::WritebackClean);

        DPRINTF(DBICache, "Create Writeback %s writable: %d, dirty: %d\n",
                pkt->print(), blk->isSet(CacheBlk::WritableBit),
                rdbi->isDirty(pkt));
        if (blk->isSet(CacheBlk::WritableBit))
        {
            // not asserting shared means we pass the block in modified
            // state, mark our own block non-writeable
            blk->clearCoherenceBits(CacheBlk::WritableBit);
        }
        else
        {
            // we are in the Owned state, tell the receiver
            pkt->setHasSharers();
        }

        // make sure the block is not marked dirty
        //  blk->clearCoherenceBits(CacheBlk::DirtyBit);
        PacketList writebacks;
        rdbi->clearDirtyBit(pkt, writebacks);

        pkt->allocate();
        pkt->setDataFromBlock(blk->data, blkSize);

        // When a block is compressed, it must first be decompressed before being
        // sent for writeback.
        if (compressor)
        {
            pkt->payloadDelay = compressor->getDecompressionLatency(blk);
        }

        return pkt;
    }

    PacketPtr
    DBICache::writecleanBlk(CacheBlk *blk, Request::Flags dest, PacketId id)
    {
        RequestPtr req = std::make_shared<Request>(
            regenerateBlkAddr(blk), blkSize, 0, Request::wbRequestorId);

        if (blk->isSecure())
        {
            req->setFlags(Request::SECURE);
        }
        req->taskId(blk->getTaskId());

        PacketPtr pkt = new Packet(req, MemCmd::WriteClean, blkSize, id);

        if (dest)
        {
            req->setFlags(dest);
            pkt->setWriteThrough();
        }

        DPRINTF(DBICache, "Create %s writable: %d, dirty: %d\n", pkt->print(),
                blk->isSet(CacheBlk::WritableBit), rdbi->isDirty(pkt));

        if (blk->isSet(CacheBlk::WritableBit))
        {
            // not asserting shared means we pass the block in modified
            // state, mark our own block non-writeable
            blk->clearCoherenceBits(CacheBlk::WritableBit);
        }
        else
        {
            // we are in the Owned state, tell the receiver
            pkt->setHasSharers();
        }

        // make sure the block is not marked dirty
        //  blk->clearCoherenceBits(CacheBlk::DirtyBit);
        PacketList writebacks;
        rdbi->clearDirtyBit(pkt, writebacks);

        pkt->allocate();
        pkt->setDataFromBlock(blk->data, blkSize);

        // When a block is compressed, it must first be decompressed before being
        // sent for writeback.
        if (compressor)
        {
            pkt->payloadDelay = compressor->getDecompressionLatency(blk);
        }

        return pkt;
    }

    PacketPtr
    DBICache::cleanEvictBlk(CacheBlk *blk)
    {
        PacketPtr temp_pkt = getCustomPkt();
        assert(!writebackClean);
        assert(blk && blk->isValid() && !rdbi->isDirty(temp_pkt));

        // Creating a zero sized write, a message to the snoop filter
        RequestPtr req = std::make_shared<Request>(
            regenerateBlkAddr(blk), blkSize, 0, Request::wbRequestorId);

        if (blk->isSecure())
            req->setFlags(Request::SECURE);

        req->taskId(blk->getTaskId());

        PacketPtr pkt = new Packet(req, MemCmd::CleanEvict);
        pkt->allocate();
        DPRINTF(Cache, "Create CleanEvict %s\n", pkt->print());

        return pkt;
    }

    uint32_t
    DBICache::handleSnoop(PacketPtr pkt, CacheBlk *blk, bool is_timing,
                          bool is_deferred, bool pending_inval)
    {
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
            DPRINTF(DBICache, "%s: snoop hit for %s, old state is %s\n", __func__,
                    pkt->print(), blk->print());

            // We may end up modifying both the block state and the packet (if
            // we respond in atomic mode), so just figure out what to do now
            // and then do it later. We respond to all snoops that need
            // responses provided we have the block in dirty state. The
            // invalidation itself is taken care of below. We don't respond to
            // cache maintenance operations as this is done by the destination
            // xbar.

            respond = rdbi->isDirty(pkt) && pkt->needsResponse();

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
            DPRINTF(DBICache, "Found addr %#llx in upper level cache for snoop %s "
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
            DPRINTF(DBICache, "new state is %s\n", blk->print());
        }

        return snoop_delay;
    }

    PacketPtr
    DBICache::evictBlock(CacheBlk *blk)
    {
        PacketPtr temp_pkt = getCustomPkt();
        PacketPtr pkt = (rdbi->isDirty(temp_pkt) || writebackClean) ? writebackBlk(blk) : cleanEvictBlk(blk);
        // PacketPtr pkt = (rdbi->isDirty(temp_pkt) || writebackClean) ? cleanEvictBlk(blk) : cleanEvictBlk(blk); // Snoop filter seems to have occurred because
        // // the cache block was being written back twice, one by RDBI and one by the cache itself.

        invalidateBlock(blk);

        return pkt;
    }
}

// namespace gem5