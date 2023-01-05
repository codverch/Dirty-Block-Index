#include "mem/cache/dbi/reg_dbi.hh"
#include "mem/cache/dbi/dbi_tags/dbi_entry.hh"

using namespace std;

namespace gem5
{

    /**
     * Constructor for RegDBI.
     */

    RegDBI::RegDBI(unsigned int RegDBISets, unsigned int RegDBIAssoc, unsigned int RegDBIBlkPerDBIEntry, int NumRegDBIEntries)
    {
        // Set the number of sets in the DBI Cache
        this->RegDBISets = RegDBISets;
        // Set the associativity of the DBI Cache
        this->RegDBIAssoc = RegDBIAssoc;
        // Set the number of cacheblocks per DBIEntry in the DBI Cache
        this->RegDBIBlkPerDBIEntry = RegDBIBlkPerDBIEntry;
        // Set the size of the DBI
        RegDBISize = RegDBISets * RegDBIAssoc * RegDBIBlkPerDBIEntry;
        this->NumRegDBIEntries = NumRegDBIEntries;
        // Initialize the RegDBIStore
        initRegDBIStore();
    }

    /**
     * Set the size of the DBI. : NEEDS TO BE FIXED
     */
    void
    RegDBI::setRegDBISize(unsigned int DBISize)
    {
        // Compute the RegDBI capacity based on the number
        RegDBISize = DBISize;
    }

    /**
     * Get the size of the DBI.
     */
    unsigned int
    RegDBI::getRegDBISize()
    {
        return RegDBISize;
    }

    /**
     * Initialize the RegDBIStore using the RegDBISize.
     * It is unclear if the RegDBISize should be included in the constructor for the RegDBIStore.
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
     * Obtain the cache block address from the incoming packet using the cache block size as a reference.
     */
    Addr
    RegDBI::getCacheBlockAddr(PacketPtr pkt)
    {

        // Get the cacheblock address from the incoming packet
        Addr cacheBlockAddr = pkt->getBlockAddr(cacheBlockSize);
        return cacheBlockAddr;
    }
    /*
     * Determine the number of DBIEntries contained in the RegDBIStore.
     */

    unsigned int
    RegDBI::getNumDBIEntries()
    {

        unsigned int numDBIEntries = RegDBIStore.size();
        return numDBIEntries;
    }

    /*
     * Obtain the RowAddr from the cache block address.
     */

    Addr
    RegDBI::getRowAddr(Packet *pkt)
    {
        // Retrieve the cache block address from the incoming packet.
        Addr cacheBlockAddr = getCacheBlockAddr(pkt);
        // Determine the number of bits needed to store the byte in the block offset.
        int bytesInBlk = log2(cacheBlockSize);
        // Obtain the RowAddr from the cache block address.
        RowAddr = cacheBlockAddr >> (int)(bytesInBlk + log2(RegDBIBlkPerDBIEntry));

        return RowAddr;
    }

    /*
     * Determine the number of bits needed to index into the RegDBIStore, based on the number of DBIEntries.
     */
    int
    RegDBI::getNumTagShiftBits()
    {

        int numTagShiftBits = log2(getNumDBIEntries());

        return numTagShiftBits;
    }

    /*
     * Retrieve the RowTag from the cache block address.
     */
    Addr
    RegDBI::getRowTag(Packet *pkt)
    {

        // Obtain the RowAddr from the cache block address.
        RowAddr = getRowAddr(pkt);
        // Determine the number of bits needed to index into the RegDBIStore, based on the number of DBIEntries.
        NumTagShiftBits = getNumTagShiftBits();
        // Obtain the RowTag from the RowAddr.
        unsigned int RowTag = RowAddr >> NumTagShiftBits;

        return RowTag;
    }

    /*
     * Obtain the necessary bits from the RowAddr to index into the RegDBIStore.
     */
    unsigned int
    RegDBI::IndexBits(Packet *pkt)
    {
        // Obtain the RowAddr from the cache block address.
        RowAddr = getRowAddr(pkt);
        // Determine the number of bits needed to index into the RegDBIStore, based on the number of DBIEntries.
        NumTagShiftBits = getNumTagShiftBits();
        // Construct a mask with 1's in the least significant bits (number of tagshift bits) and 0's in the most significant bits.
        unsigned int mask = (1 << NumTagShiftBits) - 1;
        // Obtain the necessary bits to index into the RegDBIStore.
        unsigned int indexBits = RowAddr & mask;

        return indexBits;
    }

    /*
     * Determine the DBIEntry index from the cache block address.
       This is a function that maps a part of the DRAM row address to an index in the RegDBI using a hashing algorithm.
     */

    int
    RegDBI::getIndexRegDBIStore(Packet *pkt, unsigned int RegDBIAssoc, unsigned int RegDBISets, unsigned int RegDBIBlkPerDBIEntry)
    {

        // Retrieve the RowAddr from the cache block address
        RowAddr = getRowAddr(pkt);
        // Obtain the least significant bit from the row address.
        unsigned int lsb = IndexBits(pkt);
        // Concatenate the least significant bit with the set, associativity, and blocks per entry to calculate the hash value.
        unsigned int hash_value = lsb ^ RegDBISets ^ RegDBIAssoc ^ RegDBIBlkPerDBIEntry;

        return hash_value;
    }

    /*
     * Set the dirty bit of a cache block within a DBIEntry
     */
    void
    RegDBI::setDirtyBit(Packet *pkt, int bit_index)
    {
        // Step 1: Calculate the RowTag of the provided cache block address.
        unsigned int TempRowTag = getRowTag(pkt);

        // Step 2: Compare the RowTag to all the RowTags in the RegDBIStore.
        const size_t count = RegDBIStore.size();

        for (size_t i = 0; i < count; i++)
        {
            if (RegDBIStore[i].RowTag == TempRowTag)
            {
                // Step 3: If a match is found, set the dirty bit of an index in the bitset to true.
                RegDBIStore[i].DirtyBits[bit_index] = true;
            }
        }
    }

    /*
     * Clear the dirty bit of a cache block in a DBIEntry
     */
    void
    RegDBI::clearDirtyBit(Packet *pkt, int bit_index)
    {

        // Step 1: Calculate the RowTag of the provided cache block address.
        unsigned int TempRowTag = getRowTag(pkt);

        // Step 2: Compare the RowTag to all the RowTags in the RegDBIStore.
        const size_t count = RegDBIStore.size();

        for (size_t i = 0; i < count; i++)
        {
            if (RegDBIStore[i].RowTag == TempRowTag)
            {
                // Step 3: If a match is found, set the dirty bit of an index in the bitset to false.
                RegDBIStore[i].DirtyBits[bit_index] = false;
            }
        }
    }

    /*
     * Determine if a cache block within a DBIEntry has been modified (i.e., if it is dirty).
     */
    bool
    RegDBI::isDirty(Packet *pkt, int bit_index)
    {

        // Step 1: Calculate the RowTag of the provided cache block address.
        unsigned int TempRowTag = getRowTag(pkt);

        // Step 2: Compare the RowTag to all the RowTags in the RegDBIStore.
        const size_t count = RegDBIStore.size();

        for (size_t i = 0; i < count; i++)
        {
            if (RegDBIStore[i].RowTag == TempRowTag)
            {
                // Step 3: If a match is found, check if the dirty bit of the corresponding index in the bitset is marked as true.
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
    RegDBI::createRegDBIEntry(Packet *pkt, unsigned int RegDBIAssoc, unsigned int RegDBISets, unsigned int RegDBIBlkPerDBIEntry)
    {
        // Step 1: Calculate the RowTag of the provided cache block address.
        unsigned int TempRowTag = getRowTag(pkt);

        // Step 2: Determine the DBIEntry index of the provided cache block address.
        unsigned int RegDBIEntryIndex = getIndexRegDBIStore(pkt, RegDBIAssoc, RegDBISets, RegDBIBlkPerDBIEntry);

        /* Step 3 : Check if there is space in the DBI vector array at the calculated index.
                    If there is space, create a new DBIEntry and insert it into the RegDBIStore.*/

        if (RegDBIStore.size() < RegDBIEntryIndex + 1)
        {
            // Create a new DBIEntry.
            DBIEntry RegDBIEntry;
            // Assign the RowTag of the new DBIEntry.
            RegDBIEntry.RowTag = TempRowTag;
            // Initialize the dirty bits of the new DBIEntry.
            RegDBIEntry.DirtyBits = 0;
            // Place the new DBIEntry into the RegDBIStore.
            RegDBIStore.insert(RegDBIStore.begin() + RegDBIEntryIndex, RegDBIEntry);
        }

        // If there is no space available, evict an existing DBIEntry and insert the new DBIEntry into the RegDBIStore.
        else
        {
            // Call the evictRegDBIEntry function to evict an existing DBIEntry
            evictRegDBIEntry(RegDBIEntryIndex);
            // Create a new DBIEntry.
            DBIEntry RegDBIEntry;
            // Assign the RowTag of the new DBIEntry.
            RegDBIEntry.RowTag = TempRowTag;
            // Initialize the dirty bits of the new DBIEntry.
            RegDBIEntry.DirtyBits = 0;
            // Place the new DBIEntry into the RegDBIStore.
            RegDBIStore.insert(RegDBIStore.begin() + RegDBIEntryIndex, RegDBIEntry);
        }
    }

    /* Remove an entry from the RegDBIStore and execute any necessary writebacks.*/
    void
    RegDBI::evictRegDBIEntry(int index)
    {
        // Using the given DBIEntry index, evict the corresponding DBIEntry from the RegDBIStore.
        // Verify that the index is within the bounds of the RegDBIStore vector.if (index < RegDBIStore.size())
        {
            // Retrieve the DBI entry located at the specified index.
            DBIEntry &entry = RegDBIStore[index];
            // Determine if any of the dirty bits in the dirty bit field are marked as true.
            for (unsigned int i = 0; i < entry.DirtyBits.size(); i++)
            {
                if (entry.DirtyBits[i])
                {
                    // Perform the necessary writeback to the DRAM based on the cache block
                    // Call a function that re-generates the cache block address from the RowTag
                    // NEEDS TO BE COMPLETED
                    cout << "Writeback to DRAM" << endl;
                }
            }
            // Remove the DBI entry from the RegDBIStore vector.
            RegDBIStore.erase(RegDBIStore.begin() + index);
        }
    }
    // /*
    //  * Re-generate the RowAddress from the RowTag
    //  */
    // unsigned int
    // RegDBI::GenerateRowAddress(Addr RowTag)
    // {

    //     // Use the hash function to re-generate the DRAM row address
    //     // Shift the RowTag left by the number of bits used to index into RegDBIStore
    //     unsigned int row_addr = RowTag << getNumTagShiftBits();
    //     // Get the current index of the DBIEntry just based on the location in the RegDBIStore

    //     // Use the getDBIEntryIndex() function to use the DBIEntry index of this RowTag and re-generate the DRAM row address
    //     row_addr = row_addr | getDBIEntryIndex(pkt, RegDBIAssoc, RegDBISets, RegDBIBlkPerDBIEntry);
    //     return row_addr;
    // }

    // /*
    //  * Get the cache block address from the RowTag and bit index
    //  */

    // unsigned int
    // RegDBI::GenerateCacheBlockAddress(unsigned int RowTag, unsigned int bitIndex, unsigned int RegDBIBlkPerDBIEntry)
    // {
    //     // Reverse engineer the DRAM row address from the row tag and the number of bits required to index into the RegDBIStore
    //     unsigned int row_addr = ReverseEngineerRowAddress(RowTag, getNumTagShiftBits());
    //     int bytesInBlk = log2(BaseCache::blkSize);
    //     // Calculate the byte offset within the cache block using the index of the dirty bit in the bit set
    //     unsigned int byte_offset = bitIndex * 1; // Dirty bits are stored in 1-bit chunks
    //     // Shift the DRAM row address left by the same number of bits used to extract it from the cache block address
    //     unsigned int cache_block_address = row_addr << (bytesInBlk + log2(RegDBIBlkPerDBIEntry));
    //     // OR the cache block address with the byte offset
    //     cache_block_address |= byte_offset;
    //     return cache_block_address;
}

