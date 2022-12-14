# A system with three levels of caches inherited from BaseCache

import m5
from m5.objects import *

system = System()
system.clk_domain.clock = '2.67GHz'
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = 'timing'               # Use timing accesses
system.mem_ranges = [AddrRange('512MB')] # Create an address range

system.cpu = TimingSimpleCPU()
system.membus = SystemXBar()

system.cpu.icache = 