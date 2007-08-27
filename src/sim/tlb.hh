/*
 * Copyright (c) 2006 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Gabe Black
 */

#ifndef __SIM_TLB_HH__
#define __SIM_TLB_HH__

#include "base/misc.hh"
#include "mem/request.hh"
#include "sim/faults.hh"
#include "sim/sim_object.hh"

class ThreadContext;
class Packet;

class GenericTLBBase : public SimObject
{
  protected:
    GenericTLBBase(const std::string &name) : SimObject(name)
    {}

    Fault translate(RequestPtr req, ThreadContext *tc);
};

template <bool doSizeCheck=true, bool doAlignmentCheck=true>
class GenericTLB : public GenericTLBBase
{
  public:
    GenericTLB(const std::string &name) : GenericTLBBase(name)
    {}

    Fault translate(RequestPtr req, ThreadContext *tc, bool=false)
    {
        Fault fault = GenericTLBBase::translate(req, tc);
        if (fault != NoFault)
            return fault;

        typeof(req->getSize()) size = req->getSize();
        Addr paddr = req->getPaddr();

        if(doSizeCheck && !isPowerOf2(size))
            panic("Invalid request size!\n");
        if (doAlignmentCheck && ((size - 1) & paddr))
            return Fault(new GenericAlignmentFault(paddr));

        return NoFault;
    }
};

template <bool doSizeCheck=true, bool doAlignmentCheck=true>
class GenericITB : public GenericTLB<doSizeCheck, doAlignmentCheck>
{
  public:
    GenericITB(const std::string &name) :
        GenericTLB<doSizeCheck, doAlignmentCheck>(name)
    {}
};

template <bool doSizeCheck=true, bool doAlignmentCheck=true>
class GenericDTB : public GenericTLB<doSizeCheck, doAlignmentCheck>
{
  public:
    GenericDTB(const std::string &name) :
        GenericTLB<doSizeCheck, doAlignmentCheck>(name)
    {}
};

#endif // __ARCH_SPARC_TLB_HH__
