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

1.Generate a bit mask consisting of all ones with the same number of bits as the original packet address, and store it in a variable.   
2. Retrieve the Row Tag (regTag) from the DBI entry and store it in the least significant bits of a new variable, filling the remaining bits with zeroes to match the number of bits in the original packet address.   
3. Perform a bitwise AND operation between the bit mask from step 1 and the regTag from step 2 to obtain a packet address with the regTag.
4. Left shift the result from step 3 by the number of bits in a row, and perform a bitwise AND operation with a bit mask consisting of all ones to obtain a packet address with only the block information.   
5. Create a new bit mask with the block information in the least significant bits and all other bits set to one, with the same number of bits as the original packet address.   
6. Perform a bitwise AND operation between the result from step 4 and the bit mask from step 5 to obtain a packet address with both the regTag and block information.   
7. Left shift the result from step 6 by the number of bits in a block, and perform a bitwise AND operation with a bit mask consisting of all ones to obtain a packet address with only the byte information.  
8. Create a new bit mask with the byte information in the least significant bits and all other bits set to one, with the same number of bits as the original packet address.   
9. Perform a bitwise AND operation between the result from step 7 and the bit mask from step 8 to obtain a packet address with all three fields: regTag, block, and byte information.