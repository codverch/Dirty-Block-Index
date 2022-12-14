from m5.params import *
from m5.SimObject import SimObject

class Yo(SimObject):
    type = 'Yo'
    cxx_header = "learning_gem5/part2/yo.hh"
    cxx_class = 'gem5::Yo'

    