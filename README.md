# The Dirty Block Index: Featuring Aggressive Writeback Optimization

The Dirty Block Index(DBI) has been implemented as a separate structure to decouple the dirty bits from the cache tag store. This was accomplished by creating a new cache named the DBICache and augmenting it with the DBI.

This gem5 implementation is based on the paper by Seshadri, Vivek, et al. titled "The Dirty-Block Index" which was published in ACM SIGARCH Computer Architecture News, volume 42 issue 3, pages 157-168 in 2014: https://dl.acm.org/doi/abs/10.1145/2678373.2665697


## Installation

To install gem5 with DBICache support:

```shell
 # Clone repository
 $ git clone https://github.com/codverch/Dirty-Block-Index.git ./gem5
 $ cd gem5
 # Install gem5 dependencies
 $ scons build/X86/gem5.opt -j$(nproc)
 
```
**Note:** Install gem5 dependencies as instructed on http://www.gem5.org/

## Usage

To use DBICache in gem5, you need to follow these steps:

1. Import DBICache in the `caches.py` cache configuration file.
2. Replace the last level cache with DBICache.
3. Set parameters for DBICache: block size, DBI size, DBI associativity, number of cache blocks per region, and aggressive writeback choice.

``` python
For example:

# Import the DBICache
from m5.objects import DBICache

# Set DBICache parameters
class L3Cache(DBICache):
    """DBI augmented L3 Cache with default values"""

    # DBICache parameters
    size = '1MB'
    assoc = 8
    tag_latency = 20
    data_latency = 20
    response_latency = 20
    mshrs = 20
    tgts_per_mshr = 12
    
    # DBI parameters
    blkSize = '64'
    alpha = 0.5
    dbi_assoc = 2
    blk_per_dbi_entry = 128
    aggr_writeback = True

```

## Implementation Details

#### DBICache - `gem5/src/mem/cache/dbi.cc`   
#### DBI component - `gem5/src/mem/cche/rdbi/rdbi.cc`  
#### DBI Entry - `gem5/src/mem/cache/rdbi/rdbi_entry.hh`   


#### **Note:** Add the name of any new source file you create in the `SConscript`.    
  
``` python
For example: Source('new-file-name.cc')
```

#### DBICache routes all access to dirty bits to the DBI component, instead of the tag store in a conventional cache.   

#### 1. `setDirtyBit()` - sets the dirty bit for a specific cache block.  
#### 2. `clearDirtyBit()` - generates writebacks and clears the dirty bit for a specific cache block.  
#### 3. `isDirty()` - is a function that returns a Boolean value indicating whether a specific cache block is dirty. A return value of true means that the cache block is dirty, while a return value of false means that the cache block is clean.






<!-- ## Contributing

We are open to contributions via forks, issues, pull requests, or emails. Your input is highly valued by us. Our objective is to integrate DBICache as a replacement for the conventional cache in gem5, to facilitate further research in computer architecture. -->