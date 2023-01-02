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
        Addr getCacheBlockAddr(PacketPtr pkt, unsigned int cacheBlockSize);

        // Get the RowTag from the cacheblock address
        Addr getRowTag(Packet *pkt);

        // // Function to calculate the number of bits required to index into the RegDBIStore
        // // Based on the number of DBIEntries in the RegDBIStore
        // int getNumBitsRegDBIStore();

        // // Function to generate the index in the RegDBIStore based on the RowTag
        // int getIndexRegDBIStore(int RowTag);

        // void setDirtyBitRegDBI(int RowTag, int bit_index);
        // void clearDirtyBitRegDBI(int RowTag, int bit_index);
        // int getIndexRegDBIStore(int RowTag);
        // void evictRegDBIEntry(int RowTag);
    };
}

// Wrong implementation since the index for DBI entry should be evaluated from the
// Cacheblock address. This is just a placeholder for now.

// Actually, that index can be hashed
#endif // _MEM_CACHE_DBI_REG_DBI_HH_