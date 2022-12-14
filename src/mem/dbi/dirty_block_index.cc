#include "mem/dbi/dirty_block_index.hh"

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/DirtyBlockIndex.hh"

namespace gem5
{
    DirtyBlockIndex::DirtyBlockIndex(const DirtyBlockIndexParams &params) : SimObject(params)
    {
        // std::cout << "Hello World! From DirtyBlockIndex" << std::endl;
        DPRINTF(DirtyBlockIndex, "Hello from Dirty Block Index"); // gem5 convention of printing(With debug support)
    }
}