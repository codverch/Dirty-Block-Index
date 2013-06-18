/*
 * Copyright (c) 2007 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
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

#ifndef __ARCH_X86_UTILITY_HH__
#define __ARCH_X86_UTILITY_HH__

#include "arch/x86/regs/misc.hh"
#include "arch/x86/types.hh"
#include "base/hashmap.hh"
#include "base/misc.hh"
#include "base/types.hh"
#include "cpu/static_inst.hh"
#include "cpu/thread_context.hh"
#include "sim/full_system.hh"

class ThreadContext;

namespace X86ISA
{

    inline PCState
    buildRetPC(const PCState &curPC, const PCState &callPC)
    {
        PCState retPC = callPC;
        retPC.uEnd();
        return retPC;
    }

    uint64_t
    getArgument(ThreadContext *tc, int &number, uint16_t size, bool fp);

    static inline bool
    inUserMode(ThreadContext *tc)
    {
        if (!FullSystem) {
            return true;
        } else {
            HandyM5Reg m5reg = tc->readMiscRegNoEffect(MISCREG_M5_REG);
            return m5reg.cpl == 3;
        }
    }

    /**
     * Function to insure ISA semantics about 0 registers.
     * @param tc The thread context.
     */
    template <class TC>
    void zeroRegisters(TC *tc);

    void initCPU(ThreadContext *tc, int cpuId);

    void startupCPU(ThreadContext *tc, int cpuId);

    void copyRegs(ThreadContext *src, ThreadContext *dest);

    void copyMiscRegs(ThreadContext *src, ThreadContext *dest);

    void skipFunction(ThreadContext *tc);

    inline void
    advancePC(PCState &pc, const StaticInstPtr inst)
    {
        inst->advancePC(pc);
    }

    inline uint64_t
    getExecutingAsid(ThreadContext *tc)
    {
        return 0;
    }


    /**
     * Reconstruct the rflags register from the internal gem5 register
     * state.
     *
     * gem5 stores rflags in several different registers to avoid
     * pipeline dependencies. In order to get the true rflags value,
     * we can't simply read the value of MISCREG_RFLAGS. Instead, we
     * need to read out various state from microcode registers and
     * merge that with MISCREG_RFLAGS.
     *
     * @param tc Thread context to read rflags from.
     * @return rflags as seen by the guest.
     */
    uint64_t getRFlags(ThreadContext *tc);

    /**
     * Set update the rflags register and internal gem5 state.
     *
     * @note This function does not update MISCREG_M5_REG. You might
     * need to update this register by writing anything to
     * MISCREG_M5_REG with side-effects.
     *
     * @see X86ISA::getRFlags()
     *
     * @param tc Thread context to update
     * @param val New rflags value to store in TC
     */
    void setRFlags(ThreadContext *tc, uint64_t val);

    /**
     * Extract the bit string representing a double value.
     */
    inline uint64_t getDoubleBits(double val) {
        return *(uint64_t *)(&val);
    }
}

#endif // __ARCH_X86_UTILITY_HH__
