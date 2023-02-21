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

Packet address is re-generate and appended to the PacketList writebacks to be written back to the DRAM in the `RDBI::writebackRDBIEntry()`.

The packet address contains: Row Tag, Blocks in row, and Bytes in block(MSB to LSB)

Steps involved in re-generating the packet address:

1. Create a mask of all 1's with the same number of bits as the original packet address, and store this in a variable.   
2. Retrieve the Row Tag (regTag) from the DBI entry.    
3. Combine the mask from step 1 and the Row Tag from step 2 using a bitwise AND operation.    
`This results in a new value that contains both the Row Tag and the original packet address.`   
4. Shift the result from step 3 left by the number of bits in the `Blocks in row` field, and then use a bitwise AND operation with a bit mask that contains all 1's. This will isolate the blocks in row fields.   
5. Create a new mask that contains the blocks in row fields in the least significant bits, with the rest of the bits set to 1. The total number of bits in the mask should be the same as the total number of bits in the original packet address.    
6. Combine the result from step 4 and the mask from step 5 using a bitwise AND operation. `This will result in a new value that contains the Row Tag and the blocks in row fields`.    
7. Shift the result from step 6 left by the number of bits in the `Bytes in block`, and then use a bitwise AND operation with a bit mask that contains all 1's. This will isolate the bytes in block fields.   
8. Create a new mask that contains the bytes in block fields in the least significant bits, with the rest of the bits set to 1. The total number of bits in the mask should be the same as the total number of bits in the original packet address.   
9. Combine the result from step 7 and the mask from step 8 using a bitwise AND operation. `This will result in the entire regenerated packet address` that contains the Row Tag, blocks in row fields, and bytes in block fields.
