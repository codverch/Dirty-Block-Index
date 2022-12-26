#ifndef _MEM_CACHE_DBI_SET_DBI_HH_
#define _MEM_CACHE_DBI_SET_DBI_HH_

#include <set>

#include "base/types.hh"
#include "mem/cache/dbi/base_dbi.hh"

using namespace std;

namespace gem5
{

        class SetDBI : public BaseDBI
    {
    protected:
        std::set<Addr> dirtyBitSet;

    public:
        void setDirtyBit(Addr);
        void clearDirtyBit(Addr);
        bool isDirty(Addr);
    };
}

#endif // _MEM_CACHE_DBI_SET_DBI_HH_