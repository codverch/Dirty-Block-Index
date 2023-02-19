DBICache routes all access to dirty bits to the DBI component, instead of the tag store in a conventional cache.   

## Implementation Details

DBICache - Inherits from `cache.cc` 

In DBICache, an instance of the Dirty Block Index (DBI) is created in the constructor by instantiating a DBI object.

### Relevant file locations
- `gem5/src/mem/cache/dbi.hh`: Header file
- `gem5/src/mem/cache/dbi.cc`: C++ implementation of DBICache
- `gem5/src/mem/cache/SConscript`
- `gem5/src/mem/cache/Cache.py`

**Note:** Add the name of any new source file you create in the `SConscript`.    

``` python
For example: Source('new-file-name.cc')
```

DBI component: Contains a number DBI Entries `gem5/src/mem/cache/rdbi/rdbi_entry.hh`

The key functions are:

1. `setDirtyBit()` - sets the dirty bit for a specific cache block.  
2. `clearDirtyBit()` - generates writebacks and clears the dirty bit for a specific cache block.  
3. `isDirty()` - is a function that returns a Boolean value indicating whether a specific cache block is dirty. A return value of true means that the cache block is dirty, while a return value of false means that the cache block is clean.

