/*
 * Copyright (c) 2006 The Regents of The University of Michigan
 * Copyright (c) 2007 MIPS Technologies, Inc.
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
 * Authors: Korey Sewell
 */

#ifndef __ARCH_MIPS_REGISTERS_HH__
#define __ARCH_MIPS_REGISTERS_HH__

#include "arch/mips/max_inst_regs.hh"
#include "base/misc.hh"
#include "base/types.hh"

class ThreadContext;

namespace MipsISA
{

using MipsISAInst::MaxInstSrcRegs;
using MipsISAInst::MaxInstDestRegs;

// Constants Related to the number of registers
const int NumIntArchRegs = 32;
const int NumIntSpecialRegs = 9;
const int NumFloatArchRegs = 32;
const int NumFloatSpecialRegs = 5;

const int MaxShadowRegSets = 16; // Maximum number of shadow register sets
const int NumIntRegs = NumIntArchRegs + NumIntSpecialRegs;        //HI & LO Regs
const int NumFloatRegs = NumFloatArchRegs + NumFloatSpecialRegs;//

const uint32_t MIPS32_QNAN = 0x7fbfffff;
const uint64_t MIPS64_QNAN = ULL(0x7fbfffffffffffff);

enum FPControlRegNums {
   FIR = NumFloatArchRegs,
   FCCR,
   FEXR,
   FENR,
   FCSR
};

enum FCSRBits {
    Inexact = 1,
    Underflow,
    Overflow,
    DivideByZero,
    Invalid,
    Unimplemented
};

enum FCSRFields {
    Flag_Field = 1,
    Enable_Field = 6,
    Cause_Field = 11
};

enum MiscIntRegNums {
   LO = NumIntArchRegs,
   HI,
   DSPACX0,
   DSPLo1,
   DSPHi1,
   DSPACX1,
   DSPLo2,
   DSPHi2,
   DSPACX2,
   DSPLo3,
   DSPHi3,
   DSPACX3,
   DSPControl,
   DSPLo0 = LO,
   DSPHi0 = HI
};

// semantically meaningful register indices
const int ZeroReg = 0;
const int AssemblerReg = 1;
const int SyscallSuccessReg = 7;
const int FirstArgumentReg = 4;
const int ReturnValueReg = 2;

const int KernelReg0 = 26;
const int KernelReg1 = 27;
const int GlobalPointerReg = 28;
const int StackPointerReg = 29;
const int FramePointerReg = 30;
const int ReturnAddressReg = 31;

const int SyscallPseudoReturnReg = 3;

//@TODO: Implementing ShadowSets needs to
//edit this value such that:
//TotalArchRegs = NumIntArchRegs * ShadowSets
const int TotalArchRegs = NumIntArchRegs;

// These help enumerate all the registers for dependence tracking.
const int FP_Base_DepTag = NumIntRegs;
const int Ctrl_Base_DepTag = FP_Base_DepTag + NumFloatRegs;

// Enumerate names for 'Control' Registers in the CPU
// Reference MIPS32 Arch. for Programmers, Vol. III, Ch.8
// (Register Number-Register Select) Summary of Register
//------------------------------------------------------
// The first set of names classify the CP0 names as Register Banks
// for easy indexing when using the 'RD + SEL' index combination
// in CP0 instructions.
enum MiscRegTags {
    Index = Ctrl_Base_DepTag + 0,       //Bank 0: 0 - 3
    MVPControl,
    MVPConf0,
    MVPConf1,

    CP0_Random = Ctrl_Base_DepTag + 8,      //Bank 1: 8 - 15
    VPEControl,
    VPEConf0,
    VPEConf1,
    YQMask,
    VPESchedule,
    VPEScheFBack,
    VPEOpt,

    EntryLo0 = Ctrl_Base_DepTag + 16,   //Bank 2: 16 - 23
    TCStatus,
    TCBind,
    TCRestart,
    TCHalt,
    TCContext,
    TCSchedule,
    TCScheFBack,

    EntryLo1 = Ctrl_Base_DepTag + 24,   // Bank 3: 24

    Context = Ctrl_Base_DepTag + 32,    // Bank 4: 32 - 33
    ContextConfig,

    PageMask = Ctrl_Base_DepTag + 40, //Bank 5: 40 - 41
    PageGrain = Ctrl_Base_DepTag + 41,

    Wired = Ctrl_Base_DepTag + 48,          //Bank 6:48-55
    SRSConf0,
    SRSConf1,
    SRSConf2,
    SRSConf3,
    SRSConf4,

    HWRena = Ctrl_Base_DepTag + 56,         //Bank 7: 56-63

    BadVAddr = Ctrl_Base_DepTag + 64,       //Bank 8: 64-71

    Count = Ctrl_Base_DepTag + 72,          //Bank 9: 72-79

    EntryHi = Ctrl_Base_DepTag + 80,        //Bank 10: 80-87

    Compare = Ctrl_Base_DepTag + 88,        //Bank 11: 88-95

    Status = Ctrl_Base_DepTag + 96,         //Bank 12: 96-103
    IntCtl,
    SRSCtl,
    SRSMap,

    Cause = Ctrl_Base_DepTag + 104,         //Bank 13: 104-111

    EPC = Ctrl_Base_DepTag + 112,           //Bank 14: 112-119

    PRId = Ctrl_Base_DepTag + 120,          //Bank 15: 120-127,
    EBase,

    Config = Ctrl_Base_DepTag + 128,        //Bank 16: 128-135
    Config1,
    Config2,
    Config3,
    Config4,
    Config5,
    Config6,
    Config7,


    LLAddr = Ctrl_Base_DepTag + 136,        //Bank 17: 136-143

    WatchLo0 = Ctrl_Base_DepTag + 144,      //Bank 18: 144-151
    WatchLo1,
    WatchLo2,
    WatchLo3,
    WatchLo4,
    WatchLo5,
    WatchLo6,
    WatchLo7,

    WatchHi0 = Ctrl_Base_DepTag + 152,     //Bank 19: 152-159
    WatchHi1,
    WatchHi2,
    WatchHi3,
    WatchHi4,
    WatchHi5,
    WatchHi6,
    WatchHi7,

    XCContext64 = Ctrl_Base_DepTag + 160, //Bank 20: 160-167

                       //Bank 21: 168-175

                       //Bank 22: 176-183

    Debug = Ctrl_Base_DepTag + 184,       //Bank 23: 184-191
    TraceControl1,
    TraceControl2,
    UserTraceData,
    TraceBPC,

    DEPC = Ctrl_Base_DepTag + 192,        //Bank 24: 192-199

    PerfCnt0 = Ctrl_Base_DepTag + 200,    //Bank 25: 200-207
    PerfCnt1,
    PerfCnt2,
    PerfCnt3,
    PerfCnt4,
    PerfCnt5,
    PerfCnt6,
    PerfCnt7,

    ErrCtl = Ctrl_Base_DepTag + 208,      //Bank 26: 208-215

    CacheErr0 = Ctrl_Base_DepTag + 216,   //Bank 27: 216-223
    CacheErr1,
    CacheErr2,
    CacheErr3,

    TagLo0 = Ctrl_Base_DepTag + 224,      //Bank 28: 224-231
    DataLo1,
    TagLo2,
    DataLo3,
    TagLo4,
    DataLo5,
    TagLo6,
    DataLo7,

    TagHi0 = Ctrl_Base_DepTag + 232,      //Bank 29: 232-239
    DataHi1,
    TagHi2,
    DataHi3,
    TagHi4,
    DataHi5,
    TagHi6,
    DataHi7,


    ErrorEPC = Ctrl_Base_DepTag + 240,    //Bank 30: 240-247

    DESAVE = Ctrl_Base_DepTag + 248,       //Bank 31: 248-256

    LLFlag = Ctrl_Base_DepTag + 257,

    NumControlRegs
};

const int TotalDataRegs = NumIntRegs + NumFloatRegs;

const int NumMiscRegs = NumControlRegs;

const int TotalNumRegs = NumIntRegs + NumFloatRegs + NumMiscRegs;

typedef uint16_t  RegIndex;

typedef uint32_t IntReg;

// floating point register file entry type
typedef uint32_t FloatRegBits;
typedef float FloatReg;

// cop-0/cop-1 system control register
typedef uint64_t MiscReg;

typedef union {
    IntReg   intreg;
    FloatReg fpreg;
    MiscReg  ctrlreg;
} AnyReg;

} // namespace MipsISA

#endif
