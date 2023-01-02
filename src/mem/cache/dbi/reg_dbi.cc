#include "mem/cache/dbi/reg_dbi.hh"

using namespace std;

namespace gem5
{

    //----------------------------------------------------------------------------------------------------------------------------------------------
    /**
     * Constructor for RegDBI.
     */
    //----------------------------------------------------------------------------------------------------------------------------------------------
    RegDBI::RegDBI(unsigned int RegDBISets, unsigned int RegDBIAssoc, unsigned int RegDBIBlkPerDBIEntry)
    {
        // Set the number of sets in the DBI Cache
        RegDBISets = RegDBISets;
        // Set the associativity of the DBI Cache
        RegDBIAssoc = RegDBIAssoc;
        // Set the number of cacheblocks per DBIEntry in the DBI Cache
        RegDBIBlkPerDBIEntry = RegDBIBlkPerDBIEntry;
        // Set the size of the DBI
        RegDBISize = RegDBISets * RegDBIAssoc * RegDBIBlkPerDBIEntry;
        // Initialize the RegDBIStore
        initRegDBIStore();
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------

    /**
     * Set the size of the DBI. : NEEDS TO BE FIXED
     */
    //----------------------------------------------------------------------------------------------------------------------------------------------
    RegDBI::setRegDBISize(unsigned int DBISize)
    {
        RegDBISize = DBISize;
    }

    //----------------------------------------------------------------------------------------------------------------------------------------------
    /**
     * Get the size of the DBI.
     */
    //----------------------------------------------------------------------------------------------------------------------------------------------
    RegDBI::getRegDBISize()
    {
        return RegDBISize;
    }

    //----------------------------------------------------------------------------------------------------------------------------------------------
    /**
     * Initialize the RegDBIStore based on RegDBISize. NOT SURE IF THE CONSTRUCTOR SHOULD HAVE DBI SIZE
     */
    //----------------------------------------------------------------------------------------------------------------------------------------------
    RegDBI::initRegDBIStore()
    {
        // Fetch the size of the RegDBI
        RegDBISize = getRegDBISize();
        // Initialize the RegDBIStore
        RegDBIStore.resize(RegDBISize);
    }

    RegDBI::getIndexRegDBIStore(int RowTag)
    {
        for (int i = 0; i < RegDBISize; i++)
        {
            if (RegDBIStore[i].RowTag == RowTag)
            {
                return i;
            }
        }
        return -1;
    }

    RegDBI::setDirtyBitRegDBI(int RowTag, int bit_index)
    {
        int index = getIndexRegDBIStore(RowTag);
        if (index == -1)
        {
            RegDBIEntry newEntry;
            newEntry.RowTag = RowTag;
            newEntry.DirtyBits.resize(8);
            newEntry.DirtyBits[bit_index] = true;
            RegDBIStore.push_back(newEntry);
        }
        else
        {
            RegDBIStore[index].DirtyBits[bit_index] = true;
        }

        // doWriteback()?
    }

    /* A hash function that maps the DRAM row address to an index in the RegDBI.
     * It takes the DRAM row address as an input and returns an index in the RegDBI as an output.
     */

    RegDBI::getIndexRegDBIStore(unsigned int row_addr)
    {
        return row_addr % RegDBISize;
        // return row_addr;
    }

    RegDBI::clearDirtyBitRegDBI(int RowTag, int bit_index)
    {
        int index = getIndexRegDBIStore(RowTag);
        if (index != -1)
        {
            RegDBIStore[index].DirtyBits[bit_index] = false;
        }

        // doWriteback()?
    }

    RegDBI::evictRegDBIEntry(int RowTag)
    {
        int index = getIndexRegDBIStore(RowTag);
        if (index != -1)
        {
            RegDBIStore.erase(RegDBIStore.begin() + index);
        }
    }

    /*
     * This hash function is a modulo operation that maps the DRAM row address to an index in the RegDBI.
     */

    unsigned int RegDBI::HashFunction(unsigned int row_addr)

        Re

        // HashFunction method to look up values in the DBI by their DRAM row address.

        unsigned int index = HashFunction(row_address); // Calculate the index in the DBI using the hash function
    DBIEntry entry = DBI[index];                        // Look up the DBI entry at the calculated index
}
