#ifndef _MEM_CACHE_DBI_CACHE_STATS_HH_
#define _MEM_CACHE_DBI_CACHE_STATS_HH_

#include <memory>
#include <string>
#include <vector>

#include "base/statistics.hh"
#include "mem/cache/tags/base.hh"

namespace gem5
{
    class DBICache;

    // Defining the a stat group
    struct DBICacheStats : public Stats::Group
    {
        DBICacheStats(DBICache &d, Stats::Group *parent); // constructor
        Stats::Scalar writebacksGenerated;
        // Print the stats
        void printDBICacheStats(DBICache &d);
    };

} // namespace gem5
#endif // _MEM_CACHE_DBI_CACHE_STATS_HH_