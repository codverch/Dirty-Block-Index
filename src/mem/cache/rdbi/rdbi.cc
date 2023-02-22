
#include "mem/cache/rdbi/rdbi.hh"
#include "mem/cache/rdbi/rdbi_entry.hh"
#include "base/statistics.hh"
#include "mem/cache/dbi.hh"

using namespace std;

namespace gem5
{

    RDBI::RDBI(unsigned int _numSets, unsigned int _numBlkBits, unsigned int _numblkIndexBits, unsigned int _assoc, unsigned int _numBlksInRegion, unsigned int _blkSize, bool _useAggressiveWriteback, DBICacheStats &dbistats)

    {
        cout << "Hey, I am a RDBI component" << endl;
        dbiCacheStats = &dbistats;
        numSetBits = log2(_numSets);
        numBlkBits = _numBlkBits;
        // Bits required to index into DBI entries
        numblkIndexBits = _numblkIndexBits;
        Assoc = _assoc;
        numBlksInRegion = _numBlksInRegion;
        blkSize = _blkSize;
        useAggressiveWriteback = _useAggressiveWriteback;
        rDBIStore = vector<vector<RDBIEntry>>(_numSets, vector<RDBIEntry>(_assoc, RDBIEntry(numBlksInRegion)));
    }

    unsigned int
    RDBI::getBytesInBlock(PacketPtr pkt)
    {
        // Fetch the numBlkBits number of LHS bits from the packet address
        Addr addr = pkt->getAddr();
        // Obtain the total number of bits in the packet address
        // int numAddrBits = sizeof(addr) * 8;
        // Create a mask with 1's in the LHS numBlkBits bits and 0's in the rest numAddrBits - numBlkBits bits
        Addr mask = ((1 << numBlkBits) - 1);
        // Fetch the numBlkBits number of LHS bits from the packet address
        Addr BytesInBlock = addr & mask;
        return BytesInBlock;
    }

    unsigned int
    RDBI::getBlocksInRegion(PacketPtr pkt)
    {
        // Fetch the packet address
        Addr addr = pkt->getAddr();
        // Remove the bytes in block field from the packet address
        Addr temp = addr >> numBlkBits;
        // Create a mask with 1's in the numblkIndexBits number of LHS bits and 0's in the rest numAddrBits - numblkIndexBits bits
        Addr mask = (1 << numblkIndexBits) - 1;
        // Fetch the blocks in region field from the packet address
        Addr BlocksInRegion = temp & mask;
        return BlocksInRegion;
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

        // Get the block index from the bitset
        blkIndexInBitset = getblkIndexInBitset(pkt);
        // cout << "blkIndexInBitset from getRDBIEntry: " << blkIndexInBitset << endl; // FOR DEBUGGING
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
        // Get the RDBI entry
        RDBIEntry *entry = getRDBIEntry(pkt);
        // Check if a valid RDBI entry is found
        if (entry != NULL)
        {
            // If the entry is valid, check if the dirty bit is set
            if (entry->validBit == 1)
            {
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

        RDBIEntry *entry = getRDBIEntry(pkt);
        // Check if a valid RDBI entry is found
        if (entry != NULL)
        {
            // If the entry is valid
            if (entry->validBit == 1)
            {
                // If the useAggressiveWriteback flag is set, writeback the entire region
                // Then clear the dirty bits from the bitset
                if (useAggressiveWriteback)
                {
                    // Fetch the value of the bytes in block field from the packet address
                    // Store it in the bytesInBlock variable
                    // bytesInBlock = getBytesInBlock(pkt);
                    // Fetch the value of the blocks in region field from the packet address
                    // Store it in the blocksInRegion variable
                    blocksInRegion = getBlocksInRegion(pkt);
                    // Set the numBlocksInRegionBits variable to the number of bits required to represent the number of blocks in a region
                    numBlocksInRegionBits = log2(blocksInRegion);
                    writebackRDBIEntry(writebacks, entry);
                    entry->dirtyBits.reset();
                }

                // Else, clear the dirty bit from the bitset
                else
                {
                    entry->dirtyBits.reset(blkIndexInBitset);
                }
            }
        }
    }

    void
    RDBI::setDirtyBit(PacketPtr pkt, CacheBlk *blkPtr, PacketList &writebacks)
    {
        // Get the RDBI entry
        RDBIEntry *entry = getRDBIEntry(pkt);
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
        // Fetch the value of the bytes in block field from the packet address
        // Store it in the bytesInBlock variable
        bytesInBlock = getBytesInBlock(pkt);
        // Fetch the value of the blocks in region field from the packet address
        // Store it in the blocksInRegion variable
        blocksInRegion = getBlocksInRegion(pkt);
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
        // Set the numBlocksInRegionBits variable to the number of bits required to represent the number of blocks in a region
        numBlocksInRegionBits = log2(blocksInRegion);
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
                // DBI Stats
                dbiCacheStats->writebacksGenerated++;
                // If there is a dirty bit set, fetch the cache block pointer corresponding to the dirtyBit in the blkPtrs field
                CacheBlk *blk = entry->blkPtrs[i];

                //_stats.writebacks[Request::wbRequestorId]++;

                // Re-generate the cache block address from the rowTag
                // Addr addr = (entry->regTag << numBlkBits) | (i << numBlkBits);

                // The packet address has: RowTag + Blocks inside region + bytes inside block
                // The different fields of the packet address from MSB to LSB are:
                // 1. RowTag: Row address + Bits to index into DBI
                // 2. Blocks inside region
                // 3. Bytes inside block
                // Re-generate the packet address from the rowTag

                // Step 1: Get the rowTag from the packet address
                Addr rowTag = entry->regTag;
                // Step 2: Shift the rowTag to the left by the sum of number of bits in the Blocks in region
                // and Bytes in block field
                rowTag = rowTag << (numBlkBits + numBlocksInRegionBits);
                // Step 3: Shift the blocksInRegion field to the left by the number of bits in the Bytes in block field
                blocksInRegion = blocksInRegion << numBlkBits;
                // Step 4: A bitwise OR operation between the rowTag, blocksInRegion and bytesInBlock fields
                // will give the packet address
                Addr addr = rowTag | blocksInRegion | bytesInBlock;
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
            }
        }
    }
}