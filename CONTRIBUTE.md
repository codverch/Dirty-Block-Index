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

### Relevant file locations

- `gem5/src/mem/cache/rdbi/rdbi.hh `
- `gem5/src/mem/cache/rdbi/rdbi.cc`
- `gem5/src/mem/cache/rdbi/SConscript`   

The key functions are:

1. `setDirtyBit()` - sets the dirty bit for a specific cache block.  
2. `clearDirtyBit()` - generates writebacks and clears the dirty bit for a specific cache block.  
3. `isDirty()` - is a function that returns a Boolean value indicating whether a specific cache block is dirty. A return value of true means that the cache block is dirty, while a return value of false means that the cache block is clean.

### Aggressive writeback mechanism: Re-generating the packet address 

Packet address is re-generated and appended to the PacketList writebacks to be written back to the DRAM in the `RDBI::writebackRDBIEntry()`.

The packet address contains: Row Tag, Blocks in row, and Bytes in block(MSB to LSB)

### Steps involved in re-generating the packet address:

```
1. The Row Tag is shifted to the left by the sum of the bits in the Blocks in Region and Bytes in Block fields.    
2. The Blocks in Region is shifted to the left by the number of bits in the Bytes in Block field.    
3. A bitwise OR operation is performed between the shifted Row Tag and Blocks in Region fields, and the Bytes in Block field to create the final packet address.
```


