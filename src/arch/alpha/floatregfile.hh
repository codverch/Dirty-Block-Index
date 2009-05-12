/*
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
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
 * Authors: Steve Reinhardt
 *          Gabe Black
 */

#ifndef __ARCH_ALPHA_FLOATREGFILE_HH__
#define __ARCH_ALPHA_FLOATREGFILE_HH__

#include <iosfwd>
#include <string>

#include "arch/alpha/isa_traits.hh"
#include "arch/alpha/types.hh"

class Checkpoint;

namespace AlphaISA {

const int SingleWidth = 32;
const int SingleBytes = SingleWidth / 4;
const int DoubleWidth = 64;
const int DoubleBytes = DoubleWidth / 4;
const int QuadWidth = 128;
const int QuadBytes = QuadWidth / 4;

class FloatRegFile
{
  public:
    union {
        uint64_t q[NumFloatRegs];   // integer qword view
        double d[NumFloatRegs];     // double-precision floating point view
    };

    void clear();

    void serialize(std::ostream &os);
    void unserialize(Checkpoint *cp, const std::string &section);

    FloatReg
    readReg(int floatReg)
    {
        return d[floatReg];
    }

    FloatReg
    readReg(int floatReg, int width)
    {
        return readReg(floatReg);
    }

    FloatRegBits
    readRegBits(int floatReg)
    {
        return q[floatReg];
    }

    FloatRegBits
    readRegBits(int floatReg, int width)
    {
        return readRegBits(floatReg);
    }

    void
    setReg(int floatReg, const FloatReg &val)
    {
        d[floatReg] = val;
    }

    void
    setReg(int floatReg, const FloatReg &val, int width)
    {
        setReg(floatReg, val);
    }

    void
    setRegBits(int floatReg, const FloatRegBits &val)
    {
        q[floatReg] = val;
    }

    void
    setRegBits(int floatReg, const FloatRegBits &val, int width)
    {
        setRegBits(floatReg, val);
    }

};

} // namespace AlphaISA

#endif // __ARCH_ALPHA_FLOATREGFILE_HH__
