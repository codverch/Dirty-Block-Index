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
# Authors: Ron Dreslinski
#          Brad Beckmann

import m5
from m5.objects import *
from m5.defines import buildEnv
from m5.util import addToPath
import os, optparse, sys
addToPath('../common')
addToPath('../ruby')

import Ruby

if buildEnv['FULL_SYSTEM']:
    panic("This script requires system-emulation mode (*_SE).")

# Get paths we might need.  It's expected this file is in m5/configs/example.
config_path = os.path.dirname(os.path.abspath(__file__))
config_root = os.path.dirname(config_path)
m5_root = os.path.dirname(config_root)

parser = optparse.OptionParser()

parser.add_option("-l", "--requests", metavar="N", default=100,
                  help="Stop after N requests")
parser.add_option("-f", "--wakeup_freq", metavar="N", default=10,
                  help="Wakeup every N cycles")
parser.add_option("--test-type", type="string", default="SeriesGetx",
                  help="SeriesGetx|SeriesGets|Invalidate")

#
# Add the ruby specific and protocol specific options
#
Ruby.define_options(parser)

execfile(os.path.join(config_root, "common", "Options.py"))

(options, args) = parser.parse_args()

if args:
     print "Error: script doesn't take any positional arguments"
     sys.exit(1)

#
# Select the direct test generator
#
if options.test_type == "SeriesGetx":
    generator = SeriesRequestGenerator(num_cpus = options.num_cpus,
                                             issue_writes = True)
elif options.test_type == "SeriesGets":
    generator = SeriesRequestGenerator(num_cpus = options.num_cpus,
                                             issue_writes = False)
elif options.test_type == "Invalidate":
    generator = InvalidateGenerator(num_cpus = options.num_cpus)
else:
    print "Error: unknown direct test generator"
    sys.exit(1)

#
# Create the M5 system.  Note that the PhysicalMemory Object isn't
# actually used by the rubytester, but is included to support the
# M5 memory size == Ruby memory size checks
#
system = System(physmem = PhysicalMemory())

#
# Create the ruby random tester
#
system.tester = RubyDirectedTester(requests_to_complete = \
                                   options.requests,
                                   generator = generator)

system.ruby = Ruby.create_system(options, system)

assert(options.num_cpus == len(system.ruby._cpu_ruby_ports))

for ruby_port in system.ruby._cpu_ruby_ports:
    #
    # Tie the ruby tester ports to the ruby cpu ports
    #
    system.tester.cpuPort = ruby_port.port

# -----------------------
# run simulation
# -----------------------

root = Root( system = system )
root.system.mem_mode = 'timing'

# Not much point in this being higher than the L1 latency
m5.ticks.setGlobalFrequency('1ns')

# instantiate configuration
m5.instantiate()

# simulate until program terminates
exit_event = m5.simulate(options.maxtick)

print 'Exiting @ tick', m5.curTick(), 'because', exit_event.getCause()
