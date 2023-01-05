
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
                // Return a reference to the matching DBI entry
                return entry;
            }
        }

        // If no matching DBI entry was found, return an empty DBI entry
        return RDBIEntry();
    }

}