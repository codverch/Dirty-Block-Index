# Copyright (c) 2019 Inria
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
# Authors: Daniel Carvalho

from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject

class AbstractBloomFilter(SimObject):
    type = 'AbstractBloomFilter'
    abstract = True
    cxx_header = "mem/ruby/filters/AbstractBloomFilter.hh"

    size = Param.Int(4096, "Number of entries in the filter")

    # By default assume that bloom filters are used for 64-byte cache lines
    offset_bits = Param.Unsigned(6, "Number of bits in a cache line offset")

    # Most of the filters are booleans, and thus saturate on 1
    threshold = Param.Int(1, "Value at which an entry is considered as set")

class BlockBloomFilter(AbstractBloomFilter):
    type = 'BlockBloomFilter'
    cxx_class = 'BlockBloomFilter'
    cxx_header = "mem/ruby/filters/BlockBloomFilter.hh"

    masks_lsbs = VectorParam.Unsigned([Self.offset_bits,
        2 * Self.offset_bits], "Position of the LSB of each mask")
    masks_sizes = VectorParam.Unsigned([Self.offset_bits, Self.offset_bits],
        "Size, in number of bits, of each mask")

class BulkBloomFilter(AbstractBloomFilter):
    type = 'BulkBloomFilter'
    cxx_class = 'BulkBloomFilter'
    cxx_header = "mem/ruby/filters/BulkBloomFilter.hh"

class LSB_CountingBloomFilter(AbstractBloomFilter):
    type = 'LSB_CountingBloomFilter'
    cxx_class = 'LSB_CountingBloomFilter'
    cxx_header = "mem/ruby/filters/LSB_CountingBloomFilter.hh"

    # By default use 4-bit saturating counters
    max_value = Param.Int(15, "Maximum value of the filter entries")

    # We assume that isSet will return true only when the counter saturates
    threshold = Self.max_value

class MultiBitSelBloomFilter(AbstractBloomFilter):
    type = 'MultiBitSelBloomFilter'
    cxx_class = 'MultiBitSelBloomFilter'
    cxx_header = "mem/ruby/filters/MultiBitSelBloomFilter.hh"

    num_hashes = Param.Int(4, "Number of hashes")
    threshold = Self.num_hashes
    skip_bits = Param.Int(2, "Offset from block number")
    is_parallel = Param.Bool(False, "Whether hashing is done in parallel")

class H3BloomFilter(MultiBitSelBloomFilter):
    type = 'H3BloomFilter'
    cxx_class = 'H3BloomFilter'
    cxx_header = "mem/ruby/filters/H3BloomFilter.hh"

class MultiGrainBloomFilter(AbstractBloomFilter):
    type = 'MultiGrainBloomFilter'
    cxx_class = 'MultiGrainBloomFilter'
    cxx_header = "mem/ruby/filters/MultiGrainBloomFilter.hh"

    # The base filter should not be used, since this filter is the combination
    # of multiple sub-filters, so we use a dummy value
    size = 1

    # By default there are two sub-filters that hash sequential bitfields
    filters = VectorParam.AbstractBloomFilter([
        BlockBloomFilter(size = 4096, masks_lsbs = [6, 12]),
        BlockBloomFilter(size = 1024, masks_lsbs = [18, 24])],
        "Sub-filters to be combined")

    # By default match this with the number of sub-filters
    threshold = 2
