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

#include "arch/mips/faults.hh"
#include "cpu/thread_context.hh"
#include "cpu/base.hh"
#include "base/trace.hh"

#if !FULL_SYSTEM
#include "sim/process.hh"
#include "mem/page_table.hh"
#endif

namespace MipsISA
{

FaultName MachineCheckFault::_name = "Machine Check";
FaultVect MachineCheckFault::_vect = 0x0401;
FaultStat MachineCheckFault::_count;

FaultName AlignmentFault::_name = "Alignment";
FaultVect AlignmentFault::_vect = 0x0301;
FaultStat AlignmentFault::_count;

FaultName ResetFault::_name = "reset";
FaultVect ResetFault::_vect = 0x0001;
FaultStat ResetFault::_count;

FaultName CoprocessorUnusableFault::_name = "Coprocessor Unusable";
FaultVect CoprocessorUnusableFault::_vect = 0xF001;
FaultStat CoprocessorUnusableFault::_count;

FaultName ReservedInstructionFault::_name = "Reserved Instruction";
FaultVect ReservedInstructionFault::_vect = 0x0F01;
FaultStat ReservedInstructionFault::_count;

FaultName ThreadFault::_name = "thread";
FaultVect ThreadFault::_vect = 0x00F1;
FaultStat ThreadFault::_count;


FaultName ArithmeticFault::_name = "arith";
FaultVect ArithmeticFault::_vect = 0x0501;
FaultStat ArithmeticFault::_count;

FaultName UnimplementedOpcodeFault::_name = "opdec";
FaultVect UnimplementedOpcodeFault::_vect = 0x0481;
FaultStat UnimplementedOpcodeFault::_count;

FaultName InterruptFault::_name = "interrupt";
FaultVect InterruptFault::_vect = 0x0101;
FaultStat InterruptFault::_count;

FaultName NDtbMissFault::_name = "dtb_miss_single";
FaultVect NDtbMissFault::_vect = 0x0201;
FaultStat NDtbMissFault::_count;

FaultName PDtbMissFault::_name = "dtb_miss_double";
FaultVect PDtbMissFault::_vect = 0x0281;
FaultStat PDtbMissFault::_count;

FaultName DtbPageFault::_name = "dfault";
FaultVect DtbPageFault::_vect = 0x0381;
FaultStat DtbPageFault::_count;

FaultName DtbAcvFault::_name = "dfault";
FaultVect DtbAcvFault::_vect = 0x0381;
FaultStat DtbAcvFault::_count;

FaultName ItbMissFault::_name = "itbmiss";
FaultVect ItbMissFault::_vect = 0x0181;
FaultStat ItbMissFault::_count;

FaultName ItbPageFault::_name = "itbmiss";
FaultVect ItbPageFault::_vect = 0x0181;
FaultStat ItbPageFault::_count;

FaultName ItbAcvFault::_name = "iaccvio";
FaultVect ItbAcvFault::_vect = 0x0081;
FaultStat ItbAcvFault::_count;

FaultName FloatEnableFault::_name = "fen";
FaultVect FloatEnableFault::_vect = 0x0581;
FaultStat FloatEnableFault::_count;

FaultName IntegerOverflowFault::_name = "intover";
FaultVect IntegerOverflowFault::_vect = 0x0501;
FaultStat IntegerOverflowFault::_count;

FaultName DspStateDisabledFault::_name = "intover";
FaultVect DspStateDisabledFault::_vect = 0x001a;
FaultStat DspStateDisabledFault::_count;

void ResetFault::invoke(ThreadContext *tc)
{
    warn("[tid:%i]: %s encountered.\n", tc->getThreadNum(), name());
    //tc->getCpuPtr()->reset();
}

void CoprocessorUnusableFault::invoke(ThreadContext *tc)
{
    panic("[tid:%i]: %s encountered.\n", tc->getThreadNum(), name());
}

void ReservedInstructionFault::invoke(ThreadContext *tc)
{
    panic("[tid:%i]: %s encountered.\n", tc->getThreadNum(), name());
}

void ThreadFault::invoke(ThreadContext *tc)
{
    panic("[tid:%i]: %s encountered.\n", tc->getThreadNum(), name());
}

void DspStateDisabledFault::invoke(ThreadContext *tc)
{
    panic("[tid:%i]: %s encountered.\n", tc->getThreadNum(), name());
}

} // namespace MipsISA

