#include "mem/cache/dbi/reg_dbi.hh"

using namespace std;

namespace gem5
{
    RegDBI::RegDBI(int size)
    {
        RegDBISize = size;
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

}