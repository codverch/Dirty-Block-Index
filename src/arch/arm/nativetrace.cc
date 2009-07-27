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

#include "arch/arm/isa_traits.hh"
#include "arch/arm/miscregs.hh"
#include "arch/arm/nativetrace.hh"
#include "cpu/thread_context.hh"
#include "params/ArmNativeTrace.hh"

namespace Trace {

#if TRACING_ON
static const char *regNames[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "fp", "r12", "sp", "lr", "pc",
    "cpsr"
};
#endif

void
Trace::ArmNativeTrace::ThreadState::update(NativeTrace *parent)
{
    oldState = state[current];
    current = (current + 1) % 2;
    newState = state[current];

    parent->read(newState, sizeof(newState[0]) * STATE_NUMVALS);
    for (int i = 0; i < STATE_NUMVALS; i++) {
        newState[i] = ArmISA::gtoh(newState[i]);
        changed[i] = (oldState[i] != newState[i]);
    }
}

void
Trace::ArmNativeTrace::ThreadState::update(ThreadContext *tc)
{
    oldState = state[current];
    current = (current + 1) % 2;
    newState = state[current];

    // Regular int regs
    for (int i = 0; i < 15; i++) {
        newState[i] = tc->readIntReg(i);
        changed[i] = (oldState[i] != newState[i]);
    }

    //R15, aliased with the PC
    newState[STATE_PC] = tc->readNextPC();
    changed[STATE_PC] = (newState[STATE_PC] != oldState[STATE_PC]);

    //CPSR
    newState[STATE_CPSR] = tc->readMiscReg(MISCREG_CPSR);
    changed[STATE_CPSR] = (newState[STATE_CPSR] != oldState[STATE_CPSR]);
}

void
Trace::ArmNativeTrace::check(NativeTraceRecord *record)
{
    nState.update(this);
    mState.update(record->getThread());

    bool errorFound = false;
    // Regular int regs
    for (int i = 0; i < STATE_NUMVALS; i++) {
        if (nState.changed[i] || mState.changed[i]) {
            const char *vergence = "  ";
            if (mState.oldState[i] == nState.oldState[i] &&
                mState.newState[i] != nState.newState[i]) {
                vergence = "<>";
            } else if (mState.oldState[i] != nState.oldState[i] &&
                       mState.newState[i] == nState.newState[i]) {
                vergence = "><";
            }
            if (!nState.changed[i]) {
                DPRINTF(ExecRegDelta, "%s [%5s] "\
                                      "Native:         %#010x         "\
                                      "M5:     %#010x => %#010x\n",
                                      vergence, regNames[i],
                                      nState.newState[i],
                                      mState.oldState[i], mState.newState[i]);
                errorFound = true;
            } else if (!mState.changed[i]) {
                DPRINTF(ExecRegDelta, "%s [%5s] "\
                                      "Native: %#010x => %#010x "\
                                      "M5:             %#010x        \n",
                                      vergence, regNames[i],
                                      nState.oldState[i], nState.newState[i],
                                      mState.newState[i]);
                errorFound = true;
            } else if (mState.oldState[i] != nState.oldState[i] ||
                       mState.newState[i] != nState.newState[i]) {
                DPRINTF(ExecRegDelta, "%s [%5s] "\
                                      "Native: %#010x => %#010x "\
                                      "M5:     %#010x => %#010x\n",
                                      vergence, regNames[i],
                                      nState.oldState[i], nState.newState[i],
                                      mState.oldState[i], mState.newState[i]);
                errorFound = true;
            }
        }
    }
    if (errorFound) {
        StaticInstPtr inst = record->getStaticInst();
        assert(inst);
        bool ran = true;
        if (inst->isMicroop()) {
            ran = false;
            inst = record->getMacroStaticInst();
        }
        assert(inst);
        record->traceInst(inst, ran);
    }
}

} /* namespace Trace */

////////////////////////////////////////////////////////////////////////
//
//  ExeTracer Simulation Object
//
Trace::ArmNativeTrace *
ArmNativeTraceParams::create()
{
    return new Trace::ArmNativeTrace(this);
};
