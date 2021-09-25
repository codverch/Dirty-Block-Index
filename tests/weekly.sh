#!/bin/bash

# Copyright (c) 2021 The Regents of the University of California
# All Rights Reserved.
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

set -e
set -x

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
gem5_root="${dir}/.."

# We assume the lone argument is the number of threads. If no argument is
# given we default to one.
threads=1
if [[ $# -gt 0 ]]; then
    threads=$1
fi

# Run the gem5 very-long tests.
docker run -u $UID:$GID --volume "${gem5_root}":"${gem5_root}" -w \
    "${gem5_root}"/tests --rm gcr.io/gem5-test/ubuntu-20.04_all-dependencies \
        ./main.py run --length very-long -j${threads} -t${threads}

# For the GPU tests we compile and run GCN3_X86 inside a gcn-gpu container.
docker pull gcr.io/gem5-test/gcn-gpu:latest
docker run --rm -u $UID:$GUID --volume "${gem5_root}":"${gem5_root}" -w \
    "${gem5_root}" gcr.io/gem5-test/gcn-gpu:latest bash -c \
    "scons build/GCN3_X86/gem5.opt -j${threads} \
        || (rm -rf build && scons build/GCN3_X86/gem5.opt -j${threads})"

# get LULESH
wget -qN http://dist.gem5.org/dist/develop/test-progs/lulesh/lulesh

mkdir -p tests/testing-results

# LULESH is heavily used in the HPC community on GPUs, and does a good job of
# stressing several GPU compute and memory components
docker run --rm -v ${PWD}:${PWD} -w ${PWD} -u $UID:$GID gcr.io/gem5-test/gcn-gpu gem5/build/GCN3_X86/gem5.opt gem5/configs/example/apu_se.py -n3 --mem-size=8GB --benchmark-root=gem5-resources/src/gpu/lulesh/bin -clulesh
