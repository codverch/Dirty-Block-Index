#ifndef _MEM_CACHE_DBI_REG_DBI_HH_
#define _MEM_CACHE_DBI_REG_DBI_HH_

#include <vector>
#include <cstdint>

#include "base/types.hh"
#include "mem/cache/dbi/base_dbi.hh"

using namespace std;

namespace gem5
{
    struct RegDBIEntry
    {
        int RowTag;
        vector<bool> DirtyBits;
    };

    class RegDBI : public BaseDBI
    {

    private:
        int RegDBISize;
        vector<RegDBIEntry> RegDBIStore;

    public:
        RegDBI(int size);
        void setDirtyBitRegDBI(int index, int bit_index, bool value);
        void clearDirtyBitRegDBI(int index, int bit_index);
        int getRowTagRegDBI(int index);
        void evictRegDBIEntry(int index);
    };
}