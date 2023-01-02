#include "mem/cache/dbi/dbi_tags/dbi_entry.hh"

using namespace std;

namespace gem5
{

    // Constructor is missing

    /**
     * Checks if the DBI entry is valid.
     *
     * @return True if the DBI entry is valid.
     */
    DBIEntry::isValid()
    {
        return validBit;
    }

    /**
     * Set the valid bit for a DBI entry.
     *
     * @param validBit The valid bit to set.
     */

    DBIEntry::setValidBit(int validBit)
    {
        this->validBit = validBit;
    }

    /**
     * Get RowTag asscoiated to this DBI entry, from the cacheblock
     * @return The RowTag value.
     */

    DBIEntry::getRowTag(CacheBlk *block, Addr address)
    {
        /*DRAM row address*/
        Addr row_addr = block->getTag(address);
        /*Number of bits required to index into DBI*/
        unsigned int num_bits = log2(RegDBI::getRegDBISize() / 64);
        RowTag = row_addr >> num_bits;

        return RowTag;
    }

} // namespace gem5