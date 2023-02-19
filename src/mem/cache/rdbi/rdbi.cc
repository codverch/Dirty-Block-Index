// Header file to add custom statistics
#include "sim/stat_control.hh"
#include "sim/stats.hh"
#include "base/statistics.hh"
#include "base/stats/types.hh"
#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/cache/cache.hh"
#include "mem/cache/base.hh"
#include "mem/cache/dbi_cache_stats.hh"
#include "mem/cache/dbi.hh"
#include "mem/cache/rdbi/rdbi_entry.hh"
#include "mem/cache/rdbi/rdbi.hh"

using namespace std;
using namespace gem5::statistics;

namespace gem5
{

    RDBI::RDBI(unsigned int _numSets, unsigned int _numBlkBits, unsigned int _numblkIndexBits, unsigned int _assoc, unsigned int _numBlksInRegion, unsigned int _blkSize, bool _useAggressiveWriteback, DBICacheStats &dbistats)

    {

        dbiCacheStats = &dbistats;
        cout << "Hey, I am a RDBI component" << endl;
        numSetBits = log2(_numSets);
        numBlkBits = _numBlkBits;
        numblkIndexBits = _numblkIndexBits;
        Assoc = _assoc;
        numBlksInRegion = _numBlksInRegion;
        blkSize = _blkSize;
        useAggressiveWriteback = _useAggressiveWriteback;
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
        // int rDBIIndex = regAddr & ((1 << numSetBits) - 1);
        int rDBIIndex = (regAddr >> numblkIndexBits) & ((1 << numSetBits) - 1);

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

    RDBIEntry *
    RDBI::getRDBIEntry(PacketPtr pkt)
    {

        regAddr = getRegDBITag(pkt);
        // Identify the entry
        rDBIIndex = getRDBIEntryIndex(pkt);

        // Get the inner vector of DBI entries at the specified index location
        vector<RDBIEntry> &rDBIEntries = rDBIStore[rDBIIndex];

        // Iterate through the inner vector of DBI entries using an iterator
        // Return the DBIEntry if it is found
        for (vector<RDBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
        {
            RDBIEntry &entry = *i;
            if (entry.regTag == regAddr)
            {
                return &entry;
            }
        }

        // Else return a null pointer
        return nullptr;
    }

    bool
    RDBI::isDirty(PacketPtr pkt)
    {
        // cout << "Deepanjali, I am being called from RDBI isdirty" << endl; // works
        //  Get the RDBI entry
        RDBIEntry *entry = getRDBIEntry(pkt);

        // Check if a valid RDBI entry is found
        if (entry)
        {
            // If the entry is valid, check if the dirty bit is set
            if (entry->validBit == 1)
            {
                // Compute the cache block index from the bitset
                int blkIndexInBitset = getblkIndexInBitset(pkt);
                // Check the entry's dirty bit from the bitset
                return entry->dirtyBits.test(blkIndexInBitset);
            }

            else
                return false;
        }

        // If a valid RDBI entry is not found, return false
        else
            return false;
    }

    void
    RDBI::clearDirtyBit(PacketPtr pkt, PacketList &writebacks)
    {
        dbiCacheStats->writebacksGenerated++;
        // Print the wriebacks generated
        cout << "Deepanjali, I am being called from RDBI cleardirty" << endl;
        // Get the RDBI entry
        RDBIEntry *entry = getRDBIEntry(pkt);

        // Check if a valid RDBI entry is found
        if (entry != NULL)
        {
            // If the entry is valid
            if (entry->validBit == 1)
            {
                // Compute the cache block index from the bitset
                int blkIndexInBitset = getblkIndexInBitset(pkt);

                // Clear the dirty bit from the bitset
                entry->dirtyBits.reset(blkIndexInBitset);

                // If the useAggressiveWriteback flag is set, writeback the entire region
                // Then invalidate the entry
                if (useAggressiveWriteback)
                {
                    writebackRDBIEntry(writebacks, entry);
                    entry->validBit = 0;
                }

                // Else, if the dirty bitset is empty, invalidate the entry
                else if (entry->dirtyBits.none())
                {
                    entry->validBit = 0;
                }
            }
        }
    }

    void
    RDBI::clearDirty(PacketPtr pkt, CacheBlk *blk)
    {
        // Get the RDBI entry
        RDBIEntry *entry = getRDBIEntry(pkt);

        // Check if a valid RDBI entry is found
        if (entry != NULL)
        {
            // If the entry is valid
            if (entry->validBit == 1)
            {
                // Compute the cache block index from the bitset
                int blkIndexInBitset = getblkIndexInBitset(pkt);

                // Clear the dirty bit from the bitset
                entry->dirtyBits.reset(blkIndexInBitset);

                // If the dirty bitset is empty, invalidate the entry
                if (entry->dirtyBits.none())
                {
                    entry->validBit = 0;
                }
            }
        }
    }

    void
    RDBI::setDirtyBit(PacketPtr pkt, CacheBlk *blkPtr, PacketList &writebacks)
    {
        cout << "Deepanjali, I am being called from RDBI setdirty" << endl;
        // Get the RDBI entry
        RDBIEntry *entry = getRDBIEntry(pkt);
        int blkIndexInBitset = getblkIndexInBitset(pkt);

        // Check if a valid RDBI entry is found
        if (entry != NULL)
        {
            // If the entry is valid, set the dirty bit from the bitset
            if (entry->validBit == 1)
            {
                entry->dirtyBits.set(blkIndexInBitset);
                // Get the block pointer
                entry->blkPtrs[blkIndexInBitset] = blkPtr;
            }
        }

        else
        {
            // If a valid RDBI entry is not found, create a new entry and set the dirty bit
            createRDBIEntry(writebacks, pkt, blkPtr);
            setDirtyBit(pkt, blkPtr, writebacks);
        }
    }

    void
    RDBI::createRDBIEntry(PacketList &writebacks, PacketPtr pkt, CacheBlk *blkPtr)
    {
        // Iterate through the inner vector of RDBI Entries
        // Look for an invalid entry
        // If an invalid entry is found, create a new entry
        // If no invalid entry is found, evict an entry

        // Get the index of the RDBI entry
        rDBIIndex = getRDBIEntryIndex(pkt);

        // Get the block index from the bitset
        int blkIndexInBitset = getblkIndexInBitset(pkt);

        // Get the inner vector of DBI entries at the specified index location
        vector<RDBIEntry> &rDBIEntries = rDBIStore[rDBIIndex];

        // Iterate through the inner vector of DBI entries using an iterator
        // Fetch an invalid entry if found
        // If no invalid entry is found at the end of the for loop, evict an entry
        for (vector<RDBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
        {
            RDBIEntry &entry = *i;
            if (entry.validBit == 0)
            {
                // Create a new entry
                entry.regTag = regAddr;
                entry.validBit = 1;
                entry.dirtyBits.set(blkIndexInBitset);
                entry.blkPtrs[blkIndexInBitset] = blkPtr;
                return;
            }
        }

        // If no invalid entry is found, evict an entry
        evictRDBIEntry(writebacks, rDBIEntries);
        createRDBIEntry(writebacks, pkt, blkPtr);
    }

    RDBIEntry *
    RDBI::pickRDBIEntry(vector<RDBIEntry> &rDBIEntries)
    {
        // Return the RDBIEntry returned by the replacement policy
        return randomReplacementPolicy(rDBIEntries);
    }

    RDBIEntry *
    RDBI::randomReplacementPolicy(vector<RDBIEntry> &rDBIEntries)
    {
        // Generate a random index within the range of the associativity of the set
        int randomIndex = rand() % Assoc;
        // Get the RDBIEntry at the generated index
        RDBIEntry &entry = rDBIEntries[randomIndex];
        return &entry;
    }

    void
    RDBI::evictRDBIEntry(PacketList &writebacks, vector<RDBIEntry> &rDBIEntries)
    {

        // Get the RDBIEntry to be evicted
        RDBIEntry *entry = pickRDBIEntry(rDBIEntries);

        // Generate writebacks for all the dirty cache blocks in the region
        // Invalidate the RDBIEntry
        writebackRDBIEntry(writebacks, entry);
        entry->validBit = 0;
    }

    void
    RDBI::writebackRDBIEntry(PacketList &writebacks, RDBIEntry *entry)
    {

        // Iterate over the bitset field of the RDBIEntry and check if any of the dirtyBit is set
        // If a dirty bit is set, fetch the corresponding cache block pointer from the blkPtrs field
        // Re-generate the cache block address from the rowTag
        // Create a new packet and set the address to the cache block address
        // Create a new request and set the requestor ID to the writeback requestor ID
        // Create a new writeback packet and set the address to the cache block address
        // Set the writeback packet's destination to the memory controller
        // Push the writeback packet to the writebacks list

        for (int i = 0; i < numBlksInRegion; i++)
        {
            if (entry->dirtyBits.test(i))
            {
                dbiCacheStats->writebacksGenerated++; // DEEPANJALi
                // If there is a dirty bit set, fetch the cache block pointer corresponding to the dirtyBit in the blkPtrs field
                CacheBlk *blk = entry->blkPtrs[i];

                //_stats.writebacks[Request::wbRequestorId]++;

                // Re-generate the cache block address from the rowTag
                Addr addr = (entry->regTag << numBlkBits) | (i << numBlkBits);

                RequestPtr req = std::make_shared<Request>(
                    addr, blkSize, 0, Request::wbRequestorId);

                if (blk->isSecure())
                    req->setFlags(Request::SECURE);

                req->taskId(blk->getTaskId());

                // Create a new packet and set the address to the cache block address
                PacketPtr wbPkt = new Packet(req, MemCmd::WritebackDirty);
                wbPkt->setAddr(addr);

                wbPkt->allocate();
                wbPkt->setDataFromBlock(blk->data, blkSize);

                // if (compressor)
                // {
                //     pkt->payloadDelay = compressor->getDecompressionLatency(blk);
                // }

                // Append the packet to the PacketList
                writebacks.push_back(wbPkt);

                // Increment the Scalar writebacks stat
                dbiCacheStats->writebacksGenerated++;
            }
        }
    }
}