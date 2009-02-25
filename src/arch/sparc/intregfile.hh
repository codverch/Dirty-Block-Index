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
 * Authors: Gabe Black
 *          Ali Saidi
 */

#ifndef __ARCH_SPARC_INTREGFILE_HH__
#define __ARCH_SPARC_INTREGFILE_HH__

#include "arch/sparc/isa_traits.hh"
#include "arch/sparc/types.hh"
#include "base/bitfield.hh"

#include <string>

class Checkpoint;

namespace SparcISA
{
    class RegFile;

    //This function translates integer register file indices into names
    std::string getIntRegName(RegIndex);

    const int NumIntArchRegs = 32;
    const int NumIntRegs = (MaxGL + 1) * 8 + NWindows * 16 + NumMicroIntRegs;

    class IntRegFile
    {
      private:
        friend class RegFile;
      protected:
        //The number of bits needed to index into each 8 register frame
        static const int FrameOffsetBits = 3;
        //The number of bits to choose between the 4 sets of 8 registers
        static const int FrameNumBits = 2;

        //The number of registers per "frame" (8)
        static const int RegsPerFrame = 1 << FrameOffsetBits;
        //A mask to get the frame number
        static const uint64_t FrameNumMask =
                (FrameNumBits == sizeof(int)) ?
                (unsigned int)(-1) :
                (1 << FrameNumBits) - 1;
        static const uint64_t FrameOffsetMask =
                (FrameOffsetBits == sizeof(int)) ?
                (unsigned int)(-1) :
                (1 << FrameOffsetBits) - 1;

        IntReg regGlobals[MaxGL+1][RegsPerFrame];
        IntReg regSegments[2 * NWindows][RegsPerFrame];
        IntReg microRegs[NumMicroIntRegs];
        IntReg regs[NumIntRegs];

        enum regFrame {Globals, Outputs, Locals, Inputs, NumFrames};

        IntReg * regView[NumFrames];

        static const int RegGlobalOffset = 0;
        static const int FrameOffset = (MaxGL + 1) * RegsPerFrame;
        int offset[NumFrames];

      public:

        int flattenIndex(int reg);

        void clear();

        IntRegFile();

        IntReg readReg(int intReg);

        void setReg(int intReg, const IntReg &val);

        void serialize(std::ostream &os);

        void unserialize(Checkpoint *cp, const std::string &section);

      protected:
        void setGlobals(int gl);
    };
}

#endif
