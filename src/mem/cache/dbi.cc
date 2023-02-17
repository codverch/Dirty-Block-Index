
#include <cmath>

#include "base/stats/units.hh"
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
#include "mem/cache/dbi_cache_stats.hh"
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
        cout << "Hey, I am a DBICache component + Deepanjali" << endl;
        numBlksInCache = cacheSize / blkSize;
        numBlksInDBI = numBlksInCache * alpha;
        numDBIEntries = numBlksInDBI / numBlksInRegion;
        numDBISets = numDBIEntries / dbiAssoc;
        //  numDBISetsBits = log2(numDBISets);
        numBlockSizeBits = log2(blkSize);
        numBlockIndexBits = log2(numBlksInRegion);
        rdbi = new RDBI(numDBISets, numBlockSizeBits, numBlockIndexBits, dbiAssoc, numBlksInRegion, blkSize, useAggressiveWriteback, dbistats);
        DPRINTF(DBICache, "Hey, I am a DBICache object");
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
        PacketList writebacks;

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
        PacketList writebacks;
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
}

// namespace gem5
