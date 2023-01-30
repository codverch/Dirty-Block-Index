# The Dirty Block Index: Featuring Aggressive Writeback Optimization

The Dirty Block Index(DBI) has been implemented as a separate structure to decouple the dirty bits from the cache tag store. This was accomplished by creating a new cache named the DBICache and augmenting it with the DBI.

This gem5 implementation is based on the paper by Seshadri, Vivek, et al. titled "The Dirty-Block Index" which was published in ACM SIGARCH Computer Architecture News, volume 42 issue 3, pages 157-168 in 2014: https://dl.acm.org/doi/abs/10.1145/2678373.2665697


## Installation

To install gem5 with DBICache support:

```

 # Clone repository
 $ git clone https://github.com/codverch/Dirty-Block-Index.git ./gem5
 $ cd gem5
 # Install gem5 dependencies
 $ scons build/X86/gem5.opt -j$(nproc)
 
```
**Note:** Install gem5 dependencies as instructed on http://www.gem5.org/