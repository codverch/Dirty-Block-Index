#ifndef _MEM_DBI_DIRTY_BLOCK_INDEX_HH_
#define _MEM_DBI_DIRTY_BLOCK_INDEX_HH_

#include <string>
#include "mem/dbi/dirty_block_index.hh"
#include "params/DirtyBlockIndex.hh"
#include "sim/sim_object.hh"

namespace gem5
{

    class DirtyBlockIndex : public SimObject
    {

    public:
        DirtyBlockIndex(const DirtyBlockIndexParams &params);
    };

} // namespace gem5

#endif // __MEM_DBI_DIRTY_BLOCK_INDEX_HH_