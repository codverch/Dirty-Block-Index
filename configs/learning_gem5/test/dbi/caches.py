""" Caches with options for a simple gem5 configuration script
This file contains L1 I/D and L2 caches to be used in the simple
gem5 configuration script. It uses the SimpleOpts wrapper to set up command
line options from each individual class.
"""

import m5
from m5.objects import Cache
from m5.objects import DBICache
from m5.objects import *

# Add the common scripts to our path
m5.util.addToPath('../../')

from common import SimpleOpts

# Some specific options for caches
# For all options see src/mem/cache/BaseCache.py

SimpleOpts.add_option('-n', help=" " )
SimpleOpts.add_option('-k', help=" " )  
SimpleOpts.add_option('-t', help=" " )

class L1Cache(Cache):
    """Simple L1 Cache with default values"""

    assoc = 2
    tag_latency = 1
    data_latency = 1
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20

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

class L1ICache(L1Cache):
    """Simple L1 instruction cache with default values"""

    # Set the default size
    size = '32kB'
    replacement_policy = RandomRP()


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
    size = '32kB'
    replacement_policy = RandomRP()


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
    """Simple L2 Cache with default values"""

    # Default parameters
    size = '256kB'
    assoc = 8
    tag_latency = 2
    data_latency = 6
    response_latency = 20
    mshrs = 20
    tgts_per_mshr = 12
    # Random replacement policy
    replacement_policy = RandomRP()

    SimpleOpts.add_option('--l2_size', help="L2 cache size. Default: %s" % size)

    def __init__(self, opts=None):
        super(L2Cache, self).__init__()
        if not opts or not opts.l2_size:
            return
        self.size = opts.l2_size

    def connectCPUSideBus(self, bus):
        self.cpu_side = bus.mem_side_ports


    def connectMemSideBus(self, bus):
        self.mem_side = bus.cpu_side_ports


    
    def __init__(self, options=None):
        super(L1Cache,self).__init__()
        pass

    def __init__(self, options=None):
        super(L1ICache, self).__init__(options)
        if not options or not options.l1i_size:
            return
        self.size = options.l1i_size

    def __init__(self, options=None):
        super(L1DCache, self).__init__(options)
        if not options or not options.l1d_size:
            return
        self.size = options.l1i_size


    def __init__(self, options=None):
        super(L2Cache, self).__init__()
        if not options or not options.l2_size:
            return
        self.size = options.l2_size


    
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
    replacement_policy = RandomRP()

    
    # DBI parameters
    blkSize = '64'
    alpha = 0.5
    dbi_assoc = 2
    blk_per_dbi_entry = 64
    aggr_writeback = True

    SimpleOpts.add_option('--l3_size', help="L3 cache size. Default: %s" % size) 

    def __init__(self, opts=None):
         super(L3Cache, self).__init__()
         if not opts or not opts.l3_size:
            return
         self.size = opts.l3_size

    def connectCPUSideBus(self, bus):
        self.cpu_side = bus.mem_side_ports

    def connectMemSideBus(self, bus):
        self.mem_side = bus.cpu_side_ports

