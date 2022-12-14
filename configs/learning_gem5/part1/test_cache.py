""" Caches with L1 I/D, L2, and L3"""


import m5
from m5.objects import Cache

m5.util.addToPath('../../')
from common import SimpleOpts

class L1Cache(Cache):
    """L1 cache"""
    data_latency = 1
    tag_latency = 1
    blk_size = 64
    mshrs = 4
    tgts_per_mshr = 20
    
    # Constructor
    
    def __init__(self, options=None):
        super(L1Cache, self).__init__()
        pass
    
    def connectBus(self, bus):
        """Connect this cache to a memory-side bus"""
        self.mem_side = bus.cpu_side_ports

    def connectCPU(self, cpu):
        """Connect this cache's port to a CPU-side port
           This must be defined in a subclass"""
        raise NotImplementedError
    
    
    SimpleOpts.add_option('--l1_size', help="L1 instruction cache size. Default: %s" % size)
    
    class L1ICache(L1Cache):
    """Simple L1 instruction cache with default values"""

    # Set the default size
    size = '16kB'

    SimpleOpts.add_option('--l1i_size',
                          help="L1 instruction cache size. Default: %s" % size)

    def __init__(self, opts=None):
        super(L1ICache, self).__init__(opts)
        if not opts or not opts.l1i_size:
            return
        self.size = opts.l1i_size

    def connectCPU(self, cpu):
        """Connect this cache's port to a CPU icache port"""
        self.cpu_side = cpu.icache_port

class L1DCache(L1Cache):
    """Simple L1 data cache with default values"""

    # Set the default size
    size = '64kB'

    SimpleOpts.add_option('--l1d_size',
                          help="L1 data cache size. Default: %s" % size)

    def __init__(self, opts=None):
        super(L1DCache, self).__init__(opts)
        if not opts or not opts.l1d_size:
            return
        self.size = opts.l1d_size

    def connectCPU(self, cpu):
        """Connect this cache's port to a CPU dcache port"""
        self.cpu_side = cpu.dcache_port
        
class L2Cache(Cache):
    
    tag_latency = 2
    data_latency = 6
    assoc = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20