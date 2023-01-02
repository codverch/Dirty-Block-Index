#ifndef _MEM_CACHE_DBI_REG_DBI_HH_
#define _MEM_CACHE_DBI_REG_DBI_HH_

#include <vector>
#include <cstdint>

#include "base/types.hh"
#include "mem/cache/dbi/base_dbi.hh"
#include "mem/cache/dbi/dbi_tags/dbi_entry.hh"
#include "mem/cache/dbi/dbi_tags/dbi_entry.cc"

using namespace std;

namespace gem5
{
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
        unsigned int RegDBIBlkPerDBIEntry;

    protected:
        // An array of DBIEntry objects
        vector<DBIEntry> RegDBIStore;

    public:
        // A Constructor for RegDBI, which takes the sets, associativity and number of cacheblocks per DBIEntry as arguments
        RegDBI(unsigned int RegDBISets, unsigned int RegDBIAssoc, unsigned int RegDBIBlkPerDBIEntry);

        // Function to set the size of the DBI
        void setRegDBISize(unsigned int DBISize);

        // Function to get the size of the DBI
        unsigned int getRegDBISize();

        // Function to initialize the RegDBIStore based on RegDBISize
        void initRegDBIStore();

        // Function to generate the index in the RegDBIStore based on the RowTag
        int getIndexRegDBIStore(int RowTag);

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