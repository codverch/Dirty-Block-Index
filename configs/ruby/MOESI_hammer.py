# Copyright (c) 2006-2007 The Regents of The University of Michigan
# Copyright (c) 2009 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Brad Beckmann

import math
import m5
from m5.objects import *
from m5.defines import buildEnv

#
# Note: the L1 Cache latency is only used by the sequencer on fast path hits
#
class L1Cache(RubyCache):
    latency = 2

#
# Note: the L2 Cache latency is not currently used
#
class L2Cache(RubyCache):
    latency = 10

#
# Probe filter is a cache, latency is not used
#
class ProbeFilter(RubyCache):
    latency = 1

def define_options(parser):
    parser.add_option("--allow-atomic-migration", action="store_true",
          help="allow migratory sharing for atomic only accessed blocks")
    parser.add_option("--pf-on", action="store_true",
          help="Hammer: enable Probe Filter")
    parser.add_option("--dir-on", action="store_true",
          help="Hammer: enable Full-bit Directory")

def create_system(options, system, piobus, dma_devices, ruby_system):

    if buildEnv['PROTOCOL'] != 'MOESI_hammer':
        panic("This script requires the MOESI_hammer protocol to be built.")

    cpu_sequencers = []
    
    #
    # The ruby network creation expects the list of nodes in the system to be
    # consistent with the NetDest list.  Therefore the l1 controller nodes must be
    # listed before the directory nodes and directory nodes before dma nodes, etc.
    #
    l1_cntrl_nodes = []
    dir_cntrl_nodes = []
    dma_cntrl_nodes = []

    #
    # Must create the individual controllers before the network to ensure the
    # controller constructors are called before the network constructor
    #
    block_size_bits = int(math.log(options.cacheline_size, 2))

    cntrl_count = 0
    
    for i in xrange(options.num_cpus):
        #
        # First create the Ruby objects associated with this cpu
        #
        l1i_cache = L1Cache(size = options.l1i_size,
                            assoc = options.l1i_assoc,
                            start_index_bit = block_size_bits,
                            is_icache = True)
        l1d_cache = L1Cache(size = options.l1d_size,
                            assoc = options.l1d_assoc,
                            start_index_bit = block_size_bits)
        l2_cache = L2Cache(size = options.l2_size,
                           assoc = options.l2_assoc,
                           start_index_bit = block_size_bits)

        l1_cntrl = L1Cache_Controller(version = i,
                                      cntrl_id = cntrl_count,
                                      L1IcacheMemory = l1i_cache,
                                      L1DcacheMemory = l1d_cache,
                                      L2cacheMemory = l2_cache,
                                      no_mig_atomic = not \
                                        options.allow_atomic_migration,
                                      send_evictions = (
                                          options.cpu_type == "detailed"),
                                      ruby_system = ruby_system)

        cpu_seq = RubySequencer(version = i,
                                icache = l1i_cache,
                                dcache = l1d_cache,
                                physMemPort = system.physmem.port,
                                physmem = system.physmem,
                                ruby_system = ruby_system)

        l1_cntrl.sequencer = cpu_seq

        if piobus != None:
            cpu_seq.pio_port = piobus.slave

        if options.recycle_latency:
            l1_cntrl.recycle_latency = options.recycle_latency

        exec("system.l1_cntrl%d = l1_cntrl" % i)
        #
        # Add controllers and sequencers to the appropriate lists
        #
        cpu_sequencers.append(cpu_seq)
        l1_cntrl_nodes.append(l1_cntrl)

        cntrl_count += 1

    phys_mem_size = long(system.physmem.range.second) - \
                      long(system.physmem.range.first) + 1
    mem_module_size = phys_mem_size / options.num_dirs

    #
    # determine size and index bits for probe filter
    # By default, the probe filter size is configured to be twice the
    # size of the L2 cache.
    #
    pf_size = MemorySize(options.l2_size)
    pf_size.value = pf_size.value * 2
    dir_bits = int(math.log(options.num_dirs, 2))
    pf_bits = int(math.log(pf_size.value, 2))
    if options.numa_high_bit:
        if options.numa_high_bit > 0:
            # if numa high bit explicitly set, make sure it does not overlap
            # with the probe filter index
            assert(options.numa_high_bit - dir_bits > pf_bits)

        # set the probe filter start bit to just above the block offset
        pf_start_bit = 6
    else:
        if dir_bits > 0:
            pf_start_bit = dir_bits + 5
        else:
            pf_start_bit = 6

    for i in xrange(options.num_dirs):
        #
        # Create the Ruby objects associated with the directory controller
        #

        mem_cntrl = RubyMemoryControl(version = i)

        dir_size = MemorySize('0B')
        dir_size.value = mem_module_size

        pf = ProbeFilter(size = pf_size, assoc = 4,
                         start_index_bit = pf_start_bit)

        dir_cntrl = Directory_Controller(version = i,
                                         cntrl_id = cntrl_count,
                                         directory = \
                                         RubyDirectoryMemory( \
                                                    version = i,
                                                    size = dir_size,
                                                    use_map = options.use_map,
                                                    map_levels = \
                                                    options.map_levels,
                                                    numa_high_bit = \
                                                      options.numa_high_bit),
                                         probeFilter = pf,
                                         memBuffer = mem_cntrl,
                                         probe_filter_enabled = options.pf_on,
                                         full_bit_dir_enabled = options.dir_on,
                                         ruby_system = ruby_system)

        if options.recycle_latency:
            dir_cntrl.recycle_latency = options.recycle_latency

        exec("system.dir_cntrl%d = dir_cntrl" % i)
        dir_cntrl_nodes.append(dir_cntrl)

        cntrl_count += 1

    for i, dma_device in enumerate(dma_devices):
        #
        # Create the Ruby objects associated with the dma controller
        #
        dma_seq = DMASequencer(version = i,
                               physMemPort = system.physmem.port,
                               physmem = system.physmem,
                               ruby_system = ruby_system)
        
        dma_cntrl = DMA_Controller(version = i,
                                   cntrl_id = cntrl_count,
                                   dma_sequencer = dma_seq,
                                   ruby_system = ruby_system)

        exec("system.dma_cntrl%d = dma_cntrl" % i)
        if dma_device.type == 'MemTest':
            exec("system.dma_cntrl%d.dma_sequencer.slave = dma_device.test" % i)
        else:
            exec("system.dma_cntrl%d.dma_sequencer.slave = dma_device.dma" % i)
        dma_cntrl_nodes.append(dma_cntrl)

        if options.recycle_latency:
            dma_cntrl.recycle_latency = options.recycle_latency

        cntrl_count += 1

    all_cntrls = l1_cntrl_nodes + dir_cntrl_nodes + dma_cntrl_nodes

    return (cpu_sequencers, dir_cntrl_nodes, all_cntrls)
