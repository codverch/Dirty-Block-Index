#include "mem/cache/dbi/set_dbi.hh"

#include "/home/gandiva/deepanjali/gem5/src/base/types.hh"

using namespace std;

namespace gem5
{

    void SetDBI::setDirtyBit(Addr blkAddr)
    {
        dirtyBitSet.insert(blkAddr);
    }

    void SetDBI::clearDirtyBit(Addr blkAddr)
    {
        dirtyBitSet.erase(blkAddr);
    }

    bool SetDBI::isDirty(Addr blkAddr)
    {
        return dirtyBitSet.find(blkAddr) != dirtyBitSet.end();
    }
}
