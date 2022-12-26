#include "mem/cache/dbi/map_dbi.hh"

using namespace std;

namespace gem5
{

    void MapDBI::setDirtyBit(Addr blkAddr)
    {
        dirtyBitMap[blkAddr] = true;
    }

    void MapDBI::clearDirtyBit(Addr blkAddr)
    {
        dirtyBitMap[blkAddr] = false;
    }

    bool MapDBI::isDirty(Addr blkAddr)
    {
        return dirtyBitMap[blkAddr];
    }
}