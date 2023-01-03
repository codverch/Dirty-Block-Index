#include "mem/cache/dbi/dbi_tags/dbi_entry.hh"
#include "base/types.hh"
#include "mem/cache/tags/tagged_entry.hh"

using namespace std;

namespace gem5
{

    // Constructor is missing

    /**
     * Checks if the DBI entry is valid.
     *
     * @return True if the DBI entry is valid.
     */
    bool
    DBIEntry::isValid()
    {
        return validBit;
    }

    /**
     * Set the valid bit for a DBI entry.
     *
     * @param validBit The valid bit to set.
     */
    void
    DBIEntry::setValidBit(int validBit)
    {
        this->validBit = validBit;
    }

    /**
     * Get RowTag asscoiated to this DBI entry, from the cacheblock
     * @return The RowTag value.
     */
    Addr
    DBIEntry::getRowTag(CacheBlk *block)
    {
        /*DRAM row address*/
        Addr row_addr = block->getTag(block);
        /*Number of bits required to index into DBI*/
        unsigned int num_bits = log2(RegDBI::getRegDBISize() / 64);
        RowTag = row_addr >> num_bits;

        return RowTag;
    }

} // namespace gem5