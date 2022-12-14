
from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class DeepCache(ClockedObject):
    type = 'DeepCache'
    cxx_header = "learning_gem5/part2/deep_cache.hh"
    cxx_class = 'gem5::DeepCache'

    # CPU-side ports are declared as a vector port
    # This is automatically split into two ports(For instruction and data)

    cpu_side = VectorResponsePort("CPU-side port, receives requests")
    mem_side = RequestPort("Memory-side port, sends requests")

    latency = Param.Cycles(1, "Cycles taken on a hit or to resolve a miss")
    size = Param.MemorySize('16kB', "The size of the cache")
    system = Param.System(Parent.any, "The system this cache is part of")