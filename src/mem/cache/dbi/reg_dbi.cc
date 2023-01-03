#include "mem/cache/dbi/reg_dbi.hh"

using namespace std;

namespace gem5
{

    /**
     * Constructor for RegDBI.
     */

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

    /**
     * Set the size of the DBI. : NEEDS TO BE FIXED
     */

    RegDBI::setRegDBISize(unsigned int DBISize)
    {
        RegDBISize = DBISize;
    }

    /**
     * Get the size of the DBI.
     */

    RegDBI::getRegDBISize()
    {
        return RegDBISize;
    }

    /**
     * Initialize the RegDBIStore based on RegDBISize. NOT SURE IF THE CONSTRUCTOR SHOULD HAVE DBI SIZE
     */
    void
    RegDBI::initRegDBIStore()
    {
        // Fetch the size of the RegDBI
        RegDBISize = getRegDBISize();
        // Initialize the RegDBIStore
        RegDBIStore.resize(RegDBISize);
    }

    /*
     * Get the cacheblock address from the incoming packet based on the cacheblock size.
     */
    Addr
    RegDBI::getCacheBlockAddr(PacketPtr pkt)
    {

        // Get the cacheblock address from the incoming packet
        Addr cacheBlockAddr = pkt->getBlockAddr(BaseCache::blkSize);
        return cacheBlockAddr;
    }
    /*
     * Get the number of DBIEntries in the RegDBIStore.
     */

    unsigned int
    RegDBI::getNumDBIEntries()
    {
        // Get the number of DBIEntries in the RegDBIStore
        unsigned int numDBIEntries = RegDBIStore.size();
        return numDBIEntries;
    }

    /*
     * Get the RowAddr from the cacheblock address.
     */

    Addr
    RegDBI::getRowAddr(Packet *pkt)
    {
        // Get the cacheblock address from the incoming packet
        Addr cacheBlockAddr = getCacheBlockAddr(pkt);
        // Number of bits required to store the byte in block offset
        int bytesInBlk = log2(BaseCache::blkSize);
        RowAddr = cacheBlockAddr >> (bytesInBlk + log2(RegDBIBlkPerDBIEntry));

        return RowAddr;
    }

    int
    RegDBI::getNumTagShiftBits()
    {
        // Number of bits required to index into RegDBIStore i.e., based on the number of DBIEntries
        int bitsInDBIStore = log2(getNumDBIEntries());

        return numTagShiftBits;
    }

    /*
     * Get the RowTag from the cacheblock address.
     */

    Addr
    RegDBI::getRowTag(Packet *pkt)
    {

        // Get the RowAddr from the cacheblock address
        RowAddr = getRowAddr(pkt);
        // Get the number of bits required to index into RegDBIStore i.e., based on the number of DBIEntries
        NumTagShiftBits = getNumTagShiftBits();
        unsigned int RowTag = RowAddr >> NumTagShiftBits;

        return RowTag;
    }

    /* Extract the bits required to index into RegDBIStore
     * From the RowAddr
     */
    RegDBI::IndexBits(Packet *pkt)
    {
        // Get the RowAddr from the cacheblock address
        RowAddr = getRowAddr(pkt);
        // Get the number of bits required to index into RegDBIStore i.e., based on the number of DBIEntries
        NumTagShiftBits = getNumTagShiftBits(pkt);

        // Create a mask with 1's in the LSBs(Number of tagshift bits) and 0's in the MSBs
        unsigned int mask = (1 << NumTagShiftBits) - 1;
        // Extract the bits required to index into RegDBIStore
        IndexBits = RowAddr & mask;

        return IndexBits;
    }

    /*
     * Get the DBIEntry index from the cacheblock address.
     * This is a hash function that maps the DRAM row address to an index in the RegDBI.
     */

    int
    RegDBI::getDBIEntryIndex(Packet *pkt, unsigned int RegDBIAssoc, unsigned int RegDBISets, unsigned int RegDBIBlkPerDBIEntry)
    {

        // Get the RowAddr from the cacheblock address NOOO use the first few bits of the Row Address
        RowAddr = getRowAddr(pkt);
        // Extract the LSB from the row address
        unsigned int lsb = IndexBits(pkt);
        // Combine the LSB with the set, associativity, and blocks per entry to generate the hash value
        unsigned int hash_value = lsb ^ set ^ associativity ^ blocks_per_entry;

        return hash_value;
    }

    /*
     * Set the dirty bit of a cache block in a DBIEntry
     */
    RegDBI::setDirtyBit(Packet *pkt, int bit_index)
    {
        // Given is a cache block address (By the cache)
        // Compute the RowTag of the given cache block address
        // Match it with all the RowTags in the RegDBIStore
        // If there is a match, set the dirty bit of an index in the bitset as true

        // Step-1: Compute the RowAddr of the given cache block address
        unsigned int TempRowAddr = getRowAddr(pkt);

        // Step-2: Compute the RowTag of the given cache block address
        unsigned int TempRowTag = getRowTag(pkt);

        // Step-3: Match it with all the RowTags in the RegDBIStore
        const size_t count = RegDBIStore.size();

        for (size_t i = 0; i < count; i++)
        {
            if (RegDBIStore[i].RowTag == TempRowTag)
            {
                // Step-4: If there is a match, set the dirty bit of an index in the bitset as true
                RegDBIStore[i].DirtyBits[bit_index] = true;
            }
        }
    }

    /*
     * Clear the dirty bit of a cache block in a DBIEntry
     */

    RegDBI::clearDirtyBit(Packet *pkt, int bit_index)
    {

        // Step-1: Compute the RowAddr of the given cache block address
        unsigned int TempRowAddr = getRowAddr(pkt);

        // Step-2: Compute the RowTag of the given cache block address
        unsigned int TempRowTag = getRowTag(pkt);

        // Step-3: Match it with all the RowTags in the RegDBIStore
        const size_t count = RegDBIStore.size();

        for (size_t i = 0; i < count; i++)
        {
            if (RegDBIStore[i].RowTag == TempRowTag)
            {
                // Step-4: If there is a match, clear the dirty bit of the corresponding index in the bitset
                RegDBIStore[i].DirtyBits[bit_index] = false;
            }
        }
    }

    /*
     * Check if a cache block is dirty in a DBIEntry
     */

    RegDBI::isDirty(Packet *pkt, int bit_index)
    {
        // Step-1: Compute the RowAddr of the given cache block address
        unsigned int TempRowAddr = getRowAddr(pkt);

        // Step-2: Compute the RowTag of the given cache block address
        unsigned int TempRowTag = getRowTag(pkt);

        // Step-3: Match it with all the RowTags in the RegDBIStore
        const size_t count = RegDBIStore.size();

        for (size_t i = 0; i < count; i++)
        {
            if (RegDBIStore[i].RowTag == TempRowTag)
            {
                // Step-4: If there is a match, check if the dirty bit of the corresponding index in the bitset is set
                if (RegDBIStore[i].DirtyBits[bit_index] == true)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }
    }

    /*
     * Create a new DBI entry and evict an existing DBI entry if necessary.
     */
    void
    // Create a new DBI entry and evict an existing DBI entry if necessary
    RegDBI::createRegDBIEntry(Packet *pkt, unsigned int RegDBIAssoc, unsigned int RegDBISets, unsigned int RegDBIBlkPerDBIEntry)
    {
        // Step 1: Calculate the RowTag of the given cache block address
        unsigned int TempRowTag = getRowTag(pkt);

        // Step 2: Calculate the DBIEntry index of the given cache block address
        unsigned int TempDBIEntryIndex = getDBIEntryIndex(pkt, RegDBIAssoc, RegDBISets, RegDBIBlkPerDBIEntry);

        // Step 3: Check if there is space in the DBI vector array at the generated index

        // If there is space, create a new DBIEntry and insert it into the RegDBIStore

        if (RegDBIStore.size() < TempDBIEntryIndex + 1)
        {
            // Create a new DBIEntry
            RegDBIEntry TempDBIEntry;
            // Set the RowTag of the new DBIEntry
            TempDBIEntry.RowTag = TempRowTag;
            // Set the dirty bits of the new DBIEntry
            TempDBIEntry.DirtyBits = 0;
            // Insert the new DBIEntry into the RegDBIStore
            RegDBIStore.insert(TempDBIEntryIndex, TempDBIEntry);
        }

        // If there is no space, evict an existing DBIEntry and insert the new DBIEntry into the RegDBIStore
        else
        {
            // Call the evictRegDBIEntry function to evict an existing DBIEntry
            evictRegDBIEntry(TempDBIEntryIndex);
            // Create a new DBIEntry
            RegDBIEntry TempDBIEntry;
            // Set the RowTag of the new DBIEntry
            TempDBIEntry.RowTag = TempRowTag;
            // Set the dirty bits of the new DBIEntry
            TempDBIEntry.DirtyBits = 0;
            // Insert the new DBIEntry into the RegDBIStore
            RegDBIStore.insert(TempDBIEntryIndex, TempDBIEntry);
        }
    }

    /* Evict an entry from the RegDBIStore and perform any necessary writebacks.*/
    void
    RegDBI::evictRegDBIEntry(int index)
    {
        // Based on the given DBIEntry index, evict the corresponding DBIEntry from the RegDBIStore
        // Check if the index is within the bounds of the RegDBIStore vector
        if (index < RegDBIStore.size())
        {
            // Access the DBI entry at the given index
            DBIEntry &entry = RegDBIStore[index];
            // Check if any of the dirty bits in the dirty bit field are set
            for (unsigned int i = 0; i < entry.DirtyBits.size(); i++)
            {
                if (entry.DirtyBits[i])
                {
                    // Perform the necessary writeback to the DRAM based on the cache block
                    // Call a function that re-generates the cache block address from the RowTag
                    // NEEDS TO BE COMPLETED
                }
            }
            // Erase the DBI entry from the RegDBIStore vector
            RegDBIStore.erase(RegDBIStore.begin() + index);
        }
    }

    /*
     * Get the cache block address from the RowTag and bit index
     */

    unsigned int GenerateCacheBlockAddress(unsigned int RowTag, unsigned int index, unsigned int bytesInBlk, unsigned int RegDBIBlkPerDBIEntry)
    {
        // Reverse engineer the DRAM row address from the row tag and the number of bits required to index into the RegDBIStore
        unsigned int row_addr = ReverseEngineerCacheBlockAddress(RowTag, getNumTagShiftBits());
        // Calculate the byte offset within the cache block using the index of the dirty bit in the bit set
        unsigned int byte_offset = index * 1; // Dirty bits are stored in 1-bit chunks
        // Shift the DRAM row address left by the same number of bits used to extract it from the cache block address
        unsigned int cache_block_address = row_addr << (bytesInBlk + log2(RegDBIBlkPerDBIEntry));
        // OR the cache block address with the byte offset
        cache_block_address |= byte_offset;
        return cache_block_address;
    }
}

#endif // __REG_DBI_HH__
