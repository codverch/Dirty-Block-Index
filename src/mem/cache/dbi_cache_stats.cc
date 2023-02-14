#include "mem/cache/base.hh"
#include "mem/cache/dbi.hh"
#include "mem/cache/dbi_cache_stats.hh"
#include "mem/cache/tags/base.hh"
#include "mem/packet.hh"
#include "params/DBICache.hh"

using namespace std;

namespace gem5
{
    DBICacheStats::DBICacheStats(Stats::Group *parent)
        : Stats::Group(parent), // initilizing the base class
          ADD_STAT(writebacksGenerated, "Number of DBI writebacks")

    {
        // Writebacks generated
        writebacksGenerated
            .init(1)
            .flags(Stats::total);
    }
}