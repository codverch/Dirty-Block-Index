
#include "mem/cache/rdbi/rdbi.hh"
#include "mem/cache/rdbi/rdbi_entry.hh"

using namespace std;

namespace gem5
{

    RDBI::RDBI()
    {
    }

    RDBI::RDBI(unsigned int _numSetBits, unsigned int _numBlkBits, unsigned int _numblkIndexBits)
    {
        numSetBits = _numSetBits;
        numBlkBits = _numBlkBits;
        numblkIndexBits = _numblkIndexBits;
    }

    Addr
    RDBI::getRegDBITag(PacketPtr pkt)
    {
        Addr addr = pkt->getAddr();
        regAddr = addr >> (numBlkBits + numblkIndexBits);
        return regAddr;
    }

    unsigned int
    RDBI::getRDBIEntryIndex(PacketPtr pkt, unsigned int numSetBits, unsigned int numBlkBits,
                            unsigned int numblkIndexBits)
    {
        Addr addr = pkt->getAddr();
        regAddr = addr >> (numBlkBits + numblkIndexBits);
        int rDBIIndex = regAddr & ((1 << numSetBits) - 1);

        return rDBIIndex;
    }

    bool
    RDBI::isDirty(PacketPtr pkt, unsigned int numSetBits, unsigned int numBlkBits,
                  unsigned int numblkIndexBits)
    {

        // Get the complete address
        Addr addr = pkt->getAddr();
        // Shift right to remove the block offset bits
        Addr temp = addr >> numBlkBits;
        // Construct bitmask with 1's in the lower (z) bits from notes
        int bitmask = (1 << numBlkIndexBits) - 1;
        // AND the bitmask with the shifted address to get the block index
        int blkIndex = temp & bitmask;

        regAddr = getRegDBITag(pkt);

        // Identify the entry
        rDBIIndex = getRDBIEntryIndex(pkt, numSetBits, numBlkBits, numblkIndexBits);

        // Get the inner vector of DBI entries at the specified index location
        vector<RDBIEntry> &rDBIEntries = rDBIStore[rDBIIndex];

        // Iterate through the inner vector of DBI entries using an iterator
        for (vector<DBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
        {
            RDBIEntry &entry = *i;
            if (entry.regTag == regAddr)
            {
                // If the entry is valid, return the dirty bit
                if (entry.validBit == 1)
                {
                    // Check the entry's dirty bit from the bitset
                    return entry.dirtyBits.test(blkIndex);
                }

                else
                {
                    // If the entry is not valid, return false
                    return false;
                }
            }
        }

        // If no matching DBI entry was found, return an empty DBI entry
        return RDBIEntry();
    }

    void
    RDBI::clearDirtyBit(PacketPtr pkt, unsigned int numSetBits, unsigned int numBlkBits,
                        unsigned int numblkIndexBits)
    {

        // Get the complete address
        Addr addr = pkt->getAddr();
        // Shift right to remove the block offset bits
        Addr temp = addr >> numBlkBits;
        // Construct bitmask with 1's in the lower (z) bits from notes
        int bitmask = (1 << numBlkIndexBits) - 1;
        // AND the bitmask with the shifted address to get the block index
        int blkIndex = temp & bitmask;

        regAddr = getRegDBITag(pkt);

        // Identify the entry
        rDBIIndex = getRDBIEntryIndex(pkt, numSetBits, numBlkBits, numblkIndexBits);

        // Get the inner vector of DBI entries at the specified index location
        vector<RDBIEntry> &rDBIEntries = rDBIStore[rDBIIndex];

        // Iterate through the inner vector of DBI entries using an iterator
        for (vector<DBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
        {
            RDBIEntry &entry = *i;
            if (entry.regTag == regAddr)
            {
                // If the entry is valid, return the dirty bit
                if (entry.validBit == 1)
                {
                    // NEEDS TO DO WRITEBACK
                    // Check the entry's dirty bit from the bitset
                    entry.dirtyBits.reset(blkIndex);
                }
            }
        }
    }

    void
    RDBI::setDirtyBit(PacketPtr pkt, unsigned int numSetBits, unsigned int numBlkBits,
                      unsigned int numblkIndexBits)
    {

        // Get the complete address
        Addr addr = pkt->getAddr();
        // Shift right to remove the block offset bits
        Addr temp = addr >> numBlkBits;
        // Construct bitmask with 1's in the lower (z) bits from notes
        int bitmask = (1 << numBlkIndexBits) - 1;
        // AND the bitmask with the shifted address to get the block index
        int blkIndex = temp & bitmask;

        regAddr = getRegDBITag(pkt);

        // Identify the entry
        rDBIIndex = getRDBIEntryIndex(pkt, numSetBits, numBlkBits, numblkIndexBits);

        // Get the inner vector of DBI entries at the specified index location
        vector<RDBIEntry> &rDBIEntries = rDBIStore[rDBIIndex];

        // Iterate through the inner vector of DBI entries using an iterator
        for (vector<DBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
        {
            RDBIEntry &entry = *i;
            if (entry.regTag == regAddr)
            {
                // If the entry is valid, return the dirty bit
                if (entry.validBit == 1)
                {
                    // Check the entry's dirty bit from the bitset
                    entry.dirtyBits.set(blkIndex);
                }
            }

            else
            {
                // If there wasn't a match for the regTag, iterate over the generated index and
                // check if there is an invalid entry at the rDBIIndex
                for (vector<DBIEntry>::iterator i = rDBIEntries.begin(); i != rDBIEntries.end(); ++i)
                {
                    RDBIEntry &entry = *i;
                    if (entry.validBit == 0)
                    {
                        // If there is an invalid entry, set the valid bit to 1
                        entry.validBit = 1;
                        // Set the regTag to the current regAddr
                        entry.regTag = regAddr;
                        // Set the dirty bit at the block index
                        entry.dirtyBits.set(blkIndex);
                        // Break out of the loop
                        break;
                    }

                    // If there wasn't an invalid entry, then we need to evict an entry and replace it, but at the same rDBIIndex
                    else
                    {
                        // NEEDS TO DO WRITEBACK
                        // Evict the entry at the rDBIIndex
                        rDBIEntries.erase(i);
                        // Create a new entry
                        RDBIEntry newEntry;
                        // Set the valid bit to 1
                        newEntry.validBit = 1;
                        // Set the regTag to the current regAddr
                        newEntry.regTag = regAddr;
                        // Set the dirty bit at the block index
                        newEntry.dirtyBits.set(blkIndex);
                        // Push the new entry into the rDBIEntries vector
                        rDBIEntries.push_back(newEntry);
                        // Break out of the loop
                        break;
                    }
                }
            }
        }
    }
}