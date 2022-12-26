#ifndef _MEM_CACHE_DBI_BASE_DBI_HH_
#define _MEM_CACHE_DBI_BASE_DBI_HH_

#include "base/types.hh"

using namespace std;

namespace gem5
{

    class BaseDBI
    {

    public:
        // Abstract functions
        virtual void setDirtyBit(Addr) = 0;
        virtual void clearDirtyBit(Addr) = 0;
        virtual bool isDirty(Addr) = 0;
    };
}

#endif // _MEM_CACHE_DBI_BASE_DBI_HH_