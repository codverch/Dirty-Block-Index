
#include <cmath>

#include "mem/cache/dbi.hh"
#include "base/compiler.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/DBICache.hh"
#include "enums/Clusivity.hh"
#include "debug/CacheTags.hh"
#include "debug/CacheVerbose.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/mshr.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/write_queue_entry.hh"
#include "mem/cache/rdbi/rdbi.hh"
#include "params/DBICache.hh"

using namespace std;

namespace gem5
{
    DBICache::DBICache(const DBICacheParams &p)
        : Cache(p),
          alpha(p.alpha),
          dbiAssoc(p.dbi_assoc),
          blkEntry(p.blk_per_dbi_entry),
          cacheSize(p.size),
          blkSize(p.blkSize)
    {
        DPRINTF(DBICache, "Hey, I am a DBICache object");
    }

    unsigned int
    DBICache::numBlksInCache(unsigned int cacheSize, unsigned int blkSize)
    {
        return cacheSize / blkSize;
    }

    unsigned int
    DBICache::numBlksInDBI(unsigned int cacheSize, unsigned int blkSize, unsigned int alpha)
    {
        unsigned int _numBlksInCache = numBlksInCache(cacheSize, blkSize);
        return _numBlksInCache * alpha;
    }

    unsigned int
    DBICache::numDBIEntries(unsigned int cacheSize, unsigned int blkSize, unsigned int alpha, unsigned int blkEntry)
    {
        unsigned int _numBlksInDBI = numBlksInDBI(cacheSize, blkSize, alpha);
        return _numBlksInDBI / blkEntry;
    }

    unsigned int
    DBICache::numDBISets(unsigned int cacheSize, unsigned int blkSize, unsigned int alpha, unsigned int blkEntry, unsigned int dbiAssoc)
    {
        unsigned int numEntriesInDBI = numDBIEntries(cacheSize, blkSize, alpha, blkEntry);
        return numEntriesInDBI / dbiAssoc;
    }

    unsigned int
    DBICache::numDBISetsBits(unsigned int cacheSize, unsigned int blkSize, unsigned int alpha, unsigned int blkEntry, unsigned int dbiAssoc)
    {
        unsigned int numDBISets = numDBISets(cacheSize, blkSize, alpha, blkEntry, dbiAssoc);
        return log2(numDBISets) + 1;
    }

    unsigned int
    DBICache::numBlockSizeBits(unsigned int blkSize)
    {
        return log2(blkSize) + 1;
    }

    unsigned int
    DBICache::numBlocksPerDBIEntryBits(unsigned int blkEntry)
    {
        return log2(blkEntry) + 1;
    }
}