
#include "mem/cache/rdbi/rdbi.hh"
#include "mem/cache/rdbi/rdbi_entry.hh"
#include "base/statistics.hh"
#include "mem/cache/dbi.hh"


using namespace std;

namespace gem5
{

    RDBI::RDBI(unsigned int _numSets, unsigned int _numBlkBits, unsigned int _numblkIndexBits, unsigned int _assoc, unsigned int _numBlksInRegion, unsigned int _blkSize, BaseCache::CacheStats &stats) : _stats(stats)
    
    {
        numSetBits = log2(_numSets);
        numBlkBits = _numBlkBits;
        numblkIndexBits = _numblkIndexBits;
        Assoc = _assoc;
        numBlksInRegion = _numBlksInRegion;
        blkSize = _blkSize;
        rDBIStore = vector<vector<RDBIEntry>>(_numSets, vector<RDBIEntry>(_assoc, RDBIEntry(numBlksInRegion)));
    }

    Addr
    RDBI::getRegDBITag(PacketPtr pkt)
    {
        Addr addr = pkt->getAddr();
        regAddr = addr >> (numBlkBits + numblkIndexBits);
        return regAddr;
    }

    int
    RDBI::getRDBIEntryIndex(PacketPtr pkt)
    {

        regAddr = getRegDBITag(pkt);
        int rDBIIndex = regAddr & ((1 << numSetBits) - 1);
        return rDBIIndex;
    }

    unsigned int
    RDBI::getblkIndexInBitset(PacketPtr pkt)
    {
        Addr addr = pkt->getAddr();
        Addr temp = addr >> numBlkBits;
        int bitmask = (1 << numblkIndexBits) - 1;
        int blkIndexInBitset = temp & bitmask;

        return blkIndexInBitset;
    }

    bool
    RDBI::isDirty(PacketPtr pkt)
    {

        // Get the block index from the bitset
        blkIndexInBitset = getblkIndexInBitset(pkt);
        regAddr = getRegDBITag(pkt);
        // Identify the entry
        rDBIIndex = getRDBIEntryIndex(pkt);

        // Get the inner vector of DBI entries at the specified index location
        vector<RDBIEntry> &rDBIEntries = rDBIStore[rDBIIndex];

        // Iterate through the inner vector of DBI entries using an iterator
        for (vector<RDBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
        {
            RDBIEntry &entry = *i;
            if (entry.regTag == regAddr)
            {
                // If the entry is valid, check if the dirty bit is set
                if (entry.validBit == 1)
                {
                    // Check the entry's dirty bit from the bitset
                    return entry.dirtyBits.test(blkIndexInBitset);
                }

                else 
                    return false;
            }
        }
        return false;
    }

    void
    RDBI::clearDirtyBit(PacketPtr pkt, PacketList &writebacks)
    {

        // Get the block index from the bitset
        blkIndexInBitset = getblkIndexInBitset(pkt);
        regAddr = getRegDBITag(pkt);
        // Identify the entry
        rDBIIndex = getRDBIEntryIndex(pkt);

        // Get the inner vector of DBI entries at the specified index location
        vector<RDBIEntry> &rDBIEntries = rDBIStore[rDBIIndex];

        // Iterate through the inner vector of DBI entries using an iterator
        for (vector<RDBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
        {
            RDBIEntry &entry = *i;
            if (entry.regTag == regAddr)
            {
                // If the entry is valid, clear the dirty bit
                if (entry.validBit == 1)
                {
                    // Check the entry's dirty bit from the bitset
                    entry.dirtyBits.reset(blkIndexInBitset);
                    if (entry.dirtyBits.none())
                    {
                        entry.validBit = 0;
                    }
                }
            }
        }
    }

    void
    RDBI::setDirtyBit(PacketPtr pkt, CacheBlk *blkPtr, PacketList &writebacks)
    {
        // Get the block index from the bitset
        blkIndexInBitset = getblkIndexInBitset(pkt);
        regAddr = getRegDBITag(pkt);
        // Identify the entry
        rDBIIndex = getRDBIEntryIndex(pkt);

        // Get the inner vector of DBI entries at the specified index location
        vector<RDBIEntry> &rDBIEntries = rDBIStore[rDBIIndex];

        // Iterate through the inner vector of DBI entries using an iterator
        for (vector<RDBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
        {
            RDBIEntry &entry = *i;
            if (entry.regTag == regAddr)
            {
                // If the entry is valid
                if (entry.validBit == 1)
                {
                    // Set the entry's dirty bit from the bitset
                    entry.dirtyBits.set(blkIndexInBitset);
                }
            }

            else
            {
                // If there wasn't a match for the regTag, iterate over the generated index and
                // check if there is an invalid entry at the rDBIIndex
                for (vector<RDBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
                {
                    RDBIEntry &entry = *i;
                    if (entry.validBit == 0)
                    {
                        // If there is an invalid entry, set the valid bit to 1
                        entry.validBit = 1;
                        // Set the regTag to the current regAddr
                        entry.regTag = regAddr;
                        // Set the dirty bit at the block index
                        entry.dirtyBits.set(blkIndexInBitset);
                        // Break out of the loop
                        entry.blkPtrs[blkIndexInBitset] = blkPtr;
                        return;
                    }
                }

                // If there is no invalid entry, do writeback by creating a new packet and appending it to the PacketList.
                // Then, evict the RDBIEntry at the random index and set the new entry's valid bit to 1
                // Call the evictDBIEntry function that takes a pointer to the rDBIEntries at rDBIIndex
                evictDBIEntry(pkt, writebacks, rDBIEntries);

                // Over-write the values at the current DBIEntry, since writebacks are already done
                entry.validBit = 1;
                entry.regTag = regAddr;
                entry.dirtyBits.set(blkIndexInBitset);
                entry.blkPtrs[blkIndexInBitset] = blkPtr;
            }
        }
    }

    void
    RDBI::evictDBIEntry(PacketPtr pkt, PacketList &writebacks, vector<RDBIEntry> &rDBIEntries)
    {
        // 1. Determine the index of the RDBIEntry to be evicted
        // 2. Iterate through the RDBIEntries at the generated index and check if the rowtag matches with
        // any of the DBIEntries
        // 3. If there is a match, iterate over the bitset field of the RDBIEntry and check if
        // any of the dirtyBit is set
        // 4. If there is a dirty bit set, fetch the cache block pointer corresponding to the dirtyBit in the blkPtrs field
        // 5. Re-generate the cache block address from the rowTag  


        int randomIndex = rand() % Assoc;
        RDBIEntry &entry = rDBIEntries[randomIndex];

        // Iterate over the bitset within the DBIEntry

        // Iterate over the bitset field of the RDBIEntry and check if
        // any of the dirtyBit is set
        for (int i = 0; i < numBlksInRegion; i++)
        {
            if (entry.dirtyBits[i] == 1)
            {
                // If there is a dirty bit set, fetch the cache block pointer corresponding to the dirtyBit in the blkPtrs field
                CacheBlk *blk = entry.blkPtrs[i];

                _stats.writebacks[Request::wbRequestorId]++;

                // Re-generate the cache block address from the rowTag
                Addr addr = (entry.regTag << numBlkBits) | (i << numBlkBits);

                RequestPtr req = std::make_shared<Request>(
                    addr, blkSize, 0, Request::wbRequestorId);

                if (blk->isSecure())
                    req->setFlags(Request::SECURE);

                req->taskId(blk->getTaskId());
                // Create a new packet and set the address to the cache block address
                PacketPtr wbPkt = new Packet(req, MemCmd::WritebackDirty);
                wbPkt->setAddr(addr);

                pkt->allocate();
                pkt->setDataFromBlock(blk->data, blkSize);

                // if (compressor)
                // {
                //     pkt->payloadDelay = compressor->getDecompressionLatency(blk);
                // }
                // Append the packet to the PacketList
                writebacks.push_back(wbPkt);
            }
        }
    }
}
