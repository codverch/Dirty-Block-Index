#ifndef _MEM_CACHE_DBI_REG_DBI_HH_
#define _MEM_CACHE_DBI_REG_DBI_HH_

#include <vector>
#include <cstdint>

#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/cache/base.hh"
#include "mem/cache/dbi/base_dbi.hh"
#include "mem/cache/dbi/dbi_tags/dbi_entry.hh"
#include "mem/cache/dbi/dbi_tags/dbi_entry.cc"

using namespace std;

namespace gem5
{

    class BaseCache;
    class DBIEntry;
    // typedef DBIEntry *DBIEntryPtr;

    class RegDBI : public BaseDBI
    {

    private:
        // Size of the DBI
        unsigned int RegDBISize;

        // Number of sets in the DBI Cache
        unsigned int RegDBISets;

        // Associativity of the DBI Cache
        unsigned int RegDBIAssoc;

        // Number of cacheblocks per DBIEntry in the DBI Cache
        unsigned int RegDBIBlkPerDBIEntry = 64;

        // Row Address of the DBIEntry
        unsigned int RowAddr;

        // Number of bits required to shift the RowAddr to get the RowTag
        unsigned int NumTagShiftBits;

    protected:
        // An array of DBIEntry objects
        vector<DBIEntry> RegDBIStore;

    public:
        // A Constructor for RegDBI, which takes the sets, associativity and number of cacheblocks per DBIEntry as arguments
        RegDBI(unsigned int RegDBISets, unsigned int RegDBIAssoc, unsigned int RegDBIBlkPerDBIEntry);

        // Set the size of the DBI
        void setRegDBISize(unsigned int DBISize);

        // Get the size of the DBI
        unsigned int getRegDBISize();

        // Initialize the RegDBIStore based on RegDBISize
        void initRegDBIStore();

        // Get the number of DBIEntries in the RegDBIStore
        unsigned int getNumDBIEntries();

        // Get the cacheblock address from the incoming packet based on the cacheblock size
        Addr getCacheBlockAddr(PacketPtr pkt);

        // Get the RowAddress from the cacheblock address
        Addr getRowAddr(Packet *pkt);

        // Get the number of tagshift bits from the cacheblock address
        int getNumTagShiftBits();

        // Get the RowTag from the cacheblock address
        Addr getRowTag(Packet *ptr);

        // Get the bits required to index into RegDBIStore from the RowAddress
        unsigned int IndexBits(Packet *pkt);

        // Get the index in the RegDBIStore from the RowAddress
        int getIndexRegDBIStore(Packet *pkt, unsigned int RegDBIAssoc, unsigned int RegDBISets, unsigned int RegDBIBlkPerDBIEntry = 64);

        // Will setDirtyBit take packet or just the cacheblock address as argument

        // Set the dirty bit in the DBIEntry
        void setDirtyBit(Packet *pkt, int);

        // Clear the dirty bit in the DBIEntry
        void clearDirtyBit(Packet *pkt, int);

        // Check if a cache block in a DBIEntry is dirty
        bool isDirty(Packet *pkt, int);

        // Create a new DBI entry by evicting an existing DBI entry
        void createRegDBIEntry(Packet *pkt, unsigned int RegDBIAssoc,
                               unsigned int RegDBISets, unsigned int RegDBIBlkPerDBIEntry = 64);

        // Evict an entry from the RegDBIStore and perform any necessary writebacks.
        void evictRegDBIEntry(int index);

        // Re-generate the cache block address from the RowTag
        unsigned int GenerateCacheBlockAddress(Addr RowTag, int bitIndex, unsigned int RegDBIBlkPerDBIEntry = 64);
    };
}

#endif // _MEM_CACHE_DBI_REG_DBI_HH_
