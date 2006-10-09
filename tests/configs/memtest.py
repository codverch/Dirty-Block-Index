# Copyright (c) 2006 The Regents of The University of Michigan
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
# Authors: Ron Dreslinski

import m5
from m5.objects import *

# --------------------
# Base L1 Cache
# ====================

class L1(BaseCache):
    latency = 1
    block_size = 64
    mshrs = 4
    tgts_per_mshr = 8
    protocol = CoherenceProtocol(protocol='moesi')

# ----------------------
# Base L2 Cache
# ----------------------

class L2(BaseCache):
    block_size = 64
    latency = 100
    mshrs = 92
    tgts_per_mshr = 16
    write_buffers = 8

nb_cores = 1
cpus = [ MemTest(max_loads=1e12) for i in xrange(nb_cores) ]

# system simulated
system = System(cpu = cpus, funcmem = PhysicalMemory(),
                physmem = PhysicalMemory(), membus = Bus())

# l2cache & bus
system.toL2Bus = Bus()
system.l2c = L2(size='4MB', assoc=8)
system.l2c.cpu_side = system.toL2Bus.port

# connect l2c to membus
system.l2c.mem_side = system.membus.port

# add L1 caches
for cpu in cpus:
    cpu.l1c = L1(size = '32kB', assoc = 4)
    cpu.l1c.cpu_side = cpu.test
    cpu.l1c.mem_side = system.toL2Bus.port
    system.funcmem.port = cpu.functional


# connect memory to membus
system.physmem.port = system.membus.port


# -----------------------
# run simulation
# -----------------------

root = Root( system = system )
root.system.mem_mode = 'timing'
#root.trace.flags="InstExec"
root.trace.flags="Bus"
