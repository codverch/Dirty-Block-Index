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
        rdbi->setDirtyBit(pkt, blk, writebacks);
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
    rdbi->setDirtyBit(pkt, blk, writebacks);
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

    if (rdbi->isDirty(pkt))
    {
        // we were in the Owned state, and a cache above us that
        // has the line in Shared state needs to be made aware
        // that the data it already has is in fact dirty
        pkt->setCacheResponding();
        rdbi->clearDirtyBit(pkt, writebacks);
    }
}
else if (pkt->isClean())
{
    rdbi->clearDirtyBit(pkt, writebacks);
}
else
{
    assert(pkt->isInvalidate());
    invalidateBlock(blk);
    DPRINTF(CacheVerbose, "%s for %s (invalidation)\n", __func__,
            pkt->print());
}