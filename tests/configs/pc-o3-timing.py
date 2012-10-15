# Copyright (c) 2006-2007 The Regents of The University of Michigan
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
# Authors: Steve Reinhardt

import m5
from m5.objects import *
m5.util.addToPath('../configs/common')
from Benchmarks import SysConfig
import FSConfig

mem_size = '128MB'

# --------------------
# Base L1 Cache
# ====================

class L1(BaseCache):
    hit_latency = 2
    response_latency = 2
    block_size = 64
    mshrs = 4
    tgts_per_mshr = 20
    is_top_level = True

# ----------------------
# Base L2 Cache
# ----------------------

class L2(BaseCache):
    block_size = 64
    hit_latency = 20
    response_latency = 20
    mshrs = 92
    tgts_per_mshr = 16
    write_buffers = 8

# ---------------------
# Page table walker cache
# ---------------------
class PageTableWalkerCache(BaseCache):
    assoc = 2
    block_size = 64
    hit_latency = 2
    response_latency = 2
    mshrs = 10
    size = '1kB'
    tgts_per_mshr = 12

# ---------------------
# I/O Cache
# ---------------------
class IOCache(BaseCache):
    assoc = 8
    block_size = 64
    hit_latency = 50
    response_latency = 50
    mshrs = 20
    size = '1kB'
    tgts_per_mshr = 12
    addr_ranges = [AddrRange(0, size=mem_size)]
    forward_snoops = False

#cpu
cpu = DerivO3CPU(cpu_id=0)
#the system
mdesc = SysConfig(disk = 'linux-x86.img')
system = FSConfig.makeLinuxX86System('timing', mdesc=mdesc)
system.kernel = FSConfig.binary('x86_64-vmlinux-2.6.22.9')

system.cpu = cpu

#create the iocache
system.iocache = IOCache(clock = '1GHz')
system.iocache.cpu_side = system.iobus.master
system.iocache.mem_side = system.membus.slave

#connect up the cpu and caches
cpu.addTwoLevelCacheHierarchy(L1(size = '32kB', assoc = 1),
                              L1(size = '32kB', assoc = 4),
                              L2(size = '4MB', assoc = 8),
                              PageTableWalkerCache(),
                              PageTableWalkerCache())
# create the interrupt controller
cpu.createInterruptController()
# connect cpu and caches to the rest of the system
cpu.connectAllPorts(system.membus)
# set the cpu clock along with the caches and l1-l2 bus
cpu.clock = '2GHz'

root = Root(full_system=True, system=system)
m5.ticks.setGlobalFrequency('1THz')

