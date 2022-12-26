#ifndef _MEM_CACHE_DBI_MAP_DBI_HH_
#define _MEM_CACHE_DBI_MAP_DBI_HH_

#include <unordered_map>

#include "base/types.hh"
#include "mem/cache/dbi/base_dbi.hh"

using namespace std;

namespace gem5
{

    class MapDBI : public BaseDBI
    {
    protected:
        unordered_map<Addr, bool> dirtyBitMap;

    public:
        void setDirtyBit(Addr);
        void clearDirtyBit(Addr);
        bool isDirty(Addr);
    };
}

#endif // _MEM_CACHE_DBI_MAP_DBI_HH_