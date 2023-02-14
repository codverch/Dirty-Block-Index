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

    struct DBICmdStats : public statistics::Group
    {
        DBICmdStats(DBICache &deepz, const std::string &name);

        void regStatsFromParentDBI();

        const DBICache &dbiCache;
    };

    struct DBICacheStats : public statistics::Group
    {
        DBICacheStats(DBICache &deepz);

        void regStats() override;

        DBICmdStats &cmdStats(const PacketPtr p)
        {
            return *cmd[p->cmdToIndex()];
        }

        const DBICache &dbiCache;

        statistics::Vector agrWritebacks;
        std::vector<std::unique_ptr<DBICmdStats>> cmd;
    };
} // namespace gem5
#endif // _MEM_CACHE_DBI_CACHE_STATS_HH_
