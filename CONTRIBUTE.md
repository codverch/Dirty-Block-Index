## Implementation Details

DBICache - `gem5/src/mem/cache/dbi.cc`   
DBI component - `gem5/src/mem/cche/rdbi/rdbi.cc`  
DBI Entry - `gem5/src/mem/cache/rdbi/rdbi_entry.hh`   


**Note:** Add the name of any new source file you create in the `SConscript`.    

``` python
For example: Source('new-file-name.cc')
```

DBICache routes all access to dirty bits to the DBI component, instead of the tag store in a conventional cache.   

1. `setDirtyBit()` - sets the dirty bit for a specific cache block.  
2. `clearDirtyBit()` - generates writebacks and clears the dirty bit for a specific cache block.  
3. `isDirty()` - is a function that returns a Boolean value indicating whether a specific cache block is dirty. A return value of true means that the cache block is dirty, while a return value of false means that the cache block is clean.

