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
 *          Korey Sewell
 */

#include "arch/mips/regfile/int_regfile.hh"
#include "sim/serialize.hh"

using namespace MipsISA;
using namespace std;


void
IntRegFile::clear()
{
    bzero(&regs, sizeof(regs));
    currShadowSet=0;
}

void
IntRegFile::setShadowSet(int css)
{
    DPRINTF(MipsPRA,"Setting Shadow Set to :%d (%s)\n",css,currShadowSet);
    currShadowSet = css;
}

IntReg
IntRegFile::readReg(int intReg)
{
    if(intReg < NumIntRegs)
    { // Regular GPR Read
        DPRINTF(MipsPRA,"Reading Reg: %d, CurrShadowSet: %d\n",intReg,currShadowSet);
        if(intReg >= NumIntArchRegs*NumShadowRegSets){
            return regs[intReg+NumIntRegs*currShadowSet];
        }
        else {
            return regs[(intReg + NumIntArchRegs*currShadowSet) % NumIntArchRegs];
        }
    }
    else
    { // Read from shadow GPR .. probably called by RDPGPR
        return regs[intReg];
    }
}

Fault
IntRegFile::setReg(int intReg, const IntReg &val)
{
    if (intReg != ZeroReg) {

        if(intReg < NumIntRegs)
        {
            if(intReg >= NumIntArchRegs*NumShadowRegSets){
                regs[intReg] = val;
            }
            else{
                regs[intReg+NumIntRegs*currShadowSet] = val;
            }
        }
        else{
            regs[intReg] = val;
        }
    }
    return NoFault;
}

void
IntRegFile::serialize(std::ostream &os)
{
    SERIALIZE_ARRAY(regs, NumIntRegs);
}

void
IntRegFile::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_ARRAY(regs, NumIntRegs);
}

