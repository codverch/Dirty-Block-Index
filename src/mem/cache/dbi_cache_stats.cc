#include "mem/cache/base.hh"
#include "mem/cache/dbi.hh"
#include "mem/cache/dbi_cache_stats.hh"
#include "mem/cache/tags/base.hh"
#include "mem/packet.hh"
#include "params/DBICache.hh"

using namespace std;

namespace gem5
{

    DBICmdStats::DBICmdStats(DBICache &c, const std::string &name)
        : statistics::Group(&c, name.c_str()), dbiCache(c)
    {
    }

    void
    DBICmdStats::regStatsFromParentDBI()
    {
        using namespace statistics;

        statistics::Group::regStats();
    }

    DBICacheStats::DBICacheStats(DBICache &c)
        : statistics::Group(&c), dbiCache(c),

          ADD_STAT(agrWritebacks, statistics::units::Count::get(),
                   "Number of writebacks generated due to aggressive writeback"),
          cmd(MemCmd::NUM_MEM_CMDS)

    {
        for (int idx = 0; idx < MemCmd::NUM_MEM_CMDS; ++idx)
        {
            cmd[idx].reset(new DBICmdStats(c, MemCmd(idx).toString()));
        }
    }

    void
    DBICacheStats::regStats()
    {
        using namespace statistics;

        statistics::Group::regStats();

        System *system = dbiCache.system;
        const auto max_requestors = system->maxRequestors();
        for (auto &cs : cmd)
            cs->regStatsFromParentDBI();

#define SUM_DEMAND(s)                                           \
    (cmd[MemCmd::ReadReq]->s + cmd[MemCmd::WriteReq]->s +       \
     cmd[MemCmd::WriteLineReq]->s + cmd[MemCmd::ReadExReq]->s + \
     cmd[MemCmd::ReadCleanReq]->s + cmd[MemCmd::ReadSharedReq]->s)

// should writebacks be included here?  prior code was inconsistent...
#define SUM_NON_DEMAND(s)                                    \
    (cmd[MemCmd::SoftPFReq]->s + cmd[MemCmd::HardPFReq]->s + \
     cmd[MemCmd::SoftPFExReq]->s)

        agrWritebacks
            .init(max_requestors)
            .flags(total | nozero | nonan);
        for (int i = 0; i < max_requestors; i++)
        {
            agrWritebacks.subname(i, system->getRequestorName(i));
        }
    }
}