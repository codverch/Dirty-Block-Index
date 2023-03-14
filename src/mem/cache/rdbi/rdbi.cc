
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
        cout << "Number of sets in DBI: " << _numSets;
        // Print the number of bits required to index into DBI
        cout << "Number of bits required to index into DBI: " << numSetBits << endl;
        numBlkBits = _numBlkBits;
        // Bits required to index into DBI entries
        numblkIndexBits = _numblkIndexBits;
        // Print the number of Blk in region bits
        Assoc = _assoc;
        numBlksInRegion = _numBlksInRegion;
        blkSize = _blkSize;
        useAggressiveWriteback = _useAggressiveWriteback;
        rDBIStore = vector<vector<RDBIEntry>>(_numSets, vector<RDBIEntry>(_assoc, RDBIEntry(numBlksInRegion)));
    }

    // Fetch the numBlkBits number of LHS bits from the packet address
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
        // int rDBIIndex = (regAddr >> numblkIndexBits) & ((1 << numSetBits) - 1);
        // Use the LHS 5 bits of regAddr to index into the RDBI
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

        // Debugging the address re-generation logic
        cout << "Packet address request from CPU: " << pkt->getAddr() << endl; // FOR DEBUGGING
        // Print the packet address in binary
        cout << "Packet address request from CPU in binary: " << bitset<64>(pkt->getAddr()) << endl; // FOR DEBUGGING

        // // Print the bytes in block field of the packet address
        // cout << "Bytes in block field of the packet address: " << getBytesInBlock(pkt) << endl; // FOR DEBUGGING

        // // Print the blocks in region field of the packet address
        // cout << "Blocks in region field of the packet address: " << getBlocksInRegion(pkt) << endl; // FOR DEBUGGING
        // // Print the blocks in region field of the packet address in binary
        // cout << "Blocks in region field of the packet address in binary: " << bitset<64>(getBlocksInRegion(pkt)) << endl; // FOR DEBUGGING

        // Print the region DBI tag of the packet address
        cout << "Region DBI tag of the packet address: " << getRegDBITag(pkt) << endl; // FOR DEBUGGING
        // Print the region DBI tag of the packet address in binary
        cout << "regTag in binary: " << bitset<64>(getRegDBITag(pkt)) << endl; // FOR DEBUGGING

        // Print the bits used to index into DBI in decimal
        cout << "Bits to index into DBI in decimal: " << getRDBIEntryIndex(pkt) << endl; // FOR DEBUGGING

        // Print the bits to index into DBI in binary(LHS 5 bits of the regTag)
        cout << "Bits to index into DBI in binary: " << bitset<64>(getRDBIEntryIndex(pkt)) << endl; // FOR DEBUGGING

        // Print statement for debugging
        // cout << "RDBI::isDirty() called" << endl;
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
        // Print statement for debugging
        // cout << "RDBI::clearDirtyBit() called" << endl;
        // Adding print statements for debugging
        // cout << "Packet address request from CPU: " << pkt->getAddr() << endl;

        RDBIEntry *entry = getRDBIEntry(pkt);

        // cout << "Got the RDBI entry" << endl;

        // Check if a valid RDBI entry is found
        if (entry != NULL)
        {
            // cout << "Checking if the entry is valid" << endl;
            // If the entry is valid
            if (entry->validBit == 1)
            {
                // cout << "Entry is valid" << endl;
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
                    // cout << "Valid entry not found" << endl;
                    entry->dirtyBits.reset(blkIndexInBitset);
                }
            }
        }

        // // Debugging start: Check if the re-generated address matches the packet address

        // // Step 1: Get the rowTag from the packet address
        // Addr rowTag = entry->regTag;
        // // Step 2: Shift the rowTag to the left by the sum of number of bits in the Blocks in region
        // // and Bytes in block field
        // rowTag = rowTag << (numBlkBits + numBlocksInRegionBits);
        // // Step 3: Shift the blocksInRegion field to the left by the number of bits in the Bytes in block field
        // blocksInRegion = blocksInRegion << numBlkBits;
        // // Step 4: A bitwise OR operation between the rowTag, blocksInRegion and bytesInBlock fields
        // // will give the packet address
        // Addr addr = rowTag | blocksInRegion | bytesInBlock;

        // // Print the re-generated address
        // cout << "Re-generated address: " << addr << endl;

        // // Debugging end
    }

    void
    RDBI::setDirtyBit(PacketPtr pkt, CacheBlk *blkPtr, PacketList &writebacks)
    {
        // Print statement for debugging
        // cout << "RDBI::setDirtyBit() called" << endl;
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

                // Adding the addres re-generation logic code here for debugging
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

                // Print the regenerated address for debugging
                // cout << "Regenerated address: " << addr << endl;
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
        // Print statement for debugging
        // cout << "RDBI::createRDBIEntry() called" << endl;
        // Iterate through the inner vector of RDBI Entries
        // Look for an invalid entry
        // If an invalid entry is found, create a new entry
        // If no invalid entry is found, evict an entry

        // Get the regAddr from the packet address
        regAddr = getRegDBITag(pkt);

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

        // cout << "Successfully created a new RDBI entry" << endl;

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
        // Print statement for debugging
        // cout << "RDBI::evictRDBIEntry() called" << endl;
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

        // Print statement for debugging
        // cout << "RDBI::writebackRDBIEntry() called (at the start)" << endl;

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
                CacheBlk *blkPtr = entry->blkPtrs[i];

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

                if (blkPtr->isSecure())
                    req->setFlags(Request::SECURE);

                req->taskId(blkPtr->getTaskId());

                // Create a new packet and set the address to the cache block address
                PacketPtr wbPkt = new Packet(req, MemCmd::WritebackDirty);
                wbPkt->setAddr(addr);

                wbPkt->allocate();
                // Copy the data pointed by the cache block pointer in the RDBIEntry to the packet
                wbPkt->setDataFromBlock(blkPtr->data, blkSize);
                // wbPkt->setDataFromBlock(blk->data, blkSize);

                // if (compressor)
                // {
                //     pkt->payloadDelay = compressor->getDecompressionLatency(blk);
                // }

                // Append the packet to the PacketList
                writebacks.push_back(wbPkt);

                // Clear the corresponding dirty bit in the dirtyBits field
                entry->dirtyBits.reset(i);
            }
        }

        // Print statement for debugging
        // cout << "RDBI::writebackRDBIEntry() called (at the end1)" << endl;

        // Adding the code snippet to actually do the writebacks
        while (!writebacks.empty())
        {
            PacketPtr wbPkt = writebacks.front();
            if (wbPkt->cmd == MemCmd::WritebackDirty)
            {
                // cout << "Inside the if condition" << endl;
                wbPkt->setBlockCached();
                // Allocate write buffer by sending the packet for writeback
                // cout << "Before calling allocateWriteBuffer()" << endl;
                baseCache->allocateWriteBuffer(wbPkt, 0);
                // cout << "After calling allocateWriteBuffer()" << endl;
            }
            writebacks.pop_front();

            // Print statement for debugging
            // cout << "In the doWritebacks loop" << endl; // Doesn't get printed
        }

        // Print statement for debugging
        // cout << "RDBI::createRDBIEntry() called (at the end2)" << endl; // Doesn't get printed
    }
}