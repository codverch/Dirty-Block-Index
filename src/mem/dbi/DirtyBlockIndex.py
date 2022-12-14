from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject

class DirtyBlockIndex(SimObject):
    type = 'DirtyBlockIndex'
    cxx_header = "mem/dbi/dirty_block_index.hh"
    cxx_class = 'gem5::DirtyBlockIndex'

  #  system = Param.System(Parent.any, "System we belong to")
    
    # Dirty Block Index size
    #size = Param.MemorySize("Capacity in bytes")
    
    # Tag lookup latency
    #tag_latency = Param.Cycles("Tag lookup latency")
    
    # Dirty bit lookup latency
    #dirty_bit_latency = Param.Cycles("Dirty Bit lookup latency")
    
    # Request Port: Memory
    #mem_side = RequestPort("Downstream port closer to memory")
    
    # Response Port: CPU
    #cpu_side = ResponsePort("Upstream port closer to the CPU and/or device")
    
