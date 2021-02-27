/*
 * Copyright (c) 2010-2011 ARM Limited
 * All rights reserved
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
 * Copyright (c) 2004-2006 The Regents of The University of Michigan
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
 */

#ifndef __CPU_O3_DYN_INST_IMPL_HH__
#define __CPU_O3_DYN_INST_IMPL_HH__

#include <algorithm>

#include "cpu/o3/dyn_inst.hh"
#include "debug/DynInst.hh"
#include "debug/IQ.hh"
#include "debug/O3PipeView.hh"

template <class Impl>
BaseO3DynInst<Impl>::BaseO3DynInst(const StaticInstPtr &static_inst,
                                   const StaticInstPtr &_macroop,
                                   TheISA::PCState _pc,
                                   TheISA::PCState pred_pc,
                                   InstSeqNum seq_num,
                                   typename Impl::O3CPU *_cpu)
    : seqNum(seq_num), staticInst(static_inst), cpu(_cpu), pc(_pc),
      regs(staticInst->numSrcRegs(), staticInst->numDestRegs()),
      predPC(pred_pc), macroop(_macroop)
{
    this->regs.init();

    status.reset();

    instFlags.reset();
    instFlags[RecordResult] = true;
    instFlags[Predicate] = true;
    instFlags[MemAccPredicate] = true;

#ifndef NDEBUG
    ++cpu->instcount;

    if (cpu->instcount > 1500) {
#ifdef DEBUG
        cpu->dumpInsts();
        dumpSNList();
#endif
        assert(cpu->instcount <= 1500);
    }

    DPRINTF(DynInst,
        "DynInst: [sn:%lli] Instruction created. Instcount for %s = %i\n",
        seqNum, cpu->name(), cpu->instcount);
#endif

#ifdef DEBUG
    cpu->snList.insert(seqNum);
#endif

}

template <class Impl>
BaseO3DynInst<Impl>::BaseO3DynInst(const StaticInstPtr &_staticInst,
                                   const StaticInstPtr &_macroop)
    : BaseO3DynInst<Impl>(_staticInst, _macroop, {}, {}, 0, nullptr)
{}

template <class Impl>BaseO3DynInst<Impl>::~BaseO3DynInst()
{
#if TRACING_ON
    if (DTRACE(O3PipeView)) {
        Tick fetch = this->fetchTick;
        // fetchTick can be -1 if the instruction fetched outside the trace window.
        if (fetch != -1) {
            Tick val;
            // Print info needed by the pipeline activity viewer.
            DPRINTFR(O3PipeView, "O3PipeView:fetch:%llu:0x%08llx:%d:%llu:%s\n",
                     fetch,
                     this->instAddr(),
                     this->microPC(),
                     this->seqNum,
                     this->staticInst->disassemble(this->instAddr()));

            val = (this->decodeTick == -1) ? 0 : fetch + this->decodeTick;
            DPRINTFR(O3PipeView, "O3PipeView:decode:%llu\n", val);
            val = (this->renameTick == -1) ? 0 : fetch + this->renameTick;
            DPRINTFR(O3PipeView, "O3PipeView:rename:%llu\n", val);
            val = (this->dispatchTick == -1) ? 0 : fetch + this->dispatchTick;
            DPRINTFR(O3PipeView, "O3PipeView:dispatch:%llu\n", val);
            val = (this->issueTick == -1) ? 0 : fetch + this->issueTick;
            DPRINTFR(O3PipeView, "O3PipeView:issue:%llu\n", val);
            val = (this->completeTick == -1) ? 0 : fetch + this->completeTick;
            DPRINTFR(O3PipeView, "O3PipeView:complete:%llu\n", val);
            val = (this->commitTick == -1) ? 0 : fetch + this->commitTick;

            Tick valS = (this->storeTick == -1) ? 0 : fetch + this->storeTick;
            DPRINTFR(O3PipeView, "O3PipeView:retire:%llu:store:%llu\n", val, valS);
        }
    }
#endif

    delete [] memData;
    delete traceData;
    fault = NoFault;

#ifndef NDEBUG
    --cpu->instcount;

    DPRINTF(DynInst,
        "DynInst: [sn:%lli] Instruction destroyed. Instcount for %s = %i\n",
        seqNum, cpu->name(), cpu->instcount);
#endif
#ifdef DEBUG
    cpu->snList.erase(seqNum);
#endif
};


#ifdef DEBUG
template <class Impl>
void
BaseO3DynInst<Impl>::dumpSNList()
{
    std::set<InstSeqNum>::iterator sn_it = cpu->snList.begin();

    int count = 0;
    while (sn_it != cpu->snList.end()) {
        cprintf("%i: [sn:%lli] not destroyed\n", count, (*sn_it));
        count++;
        sn_it++;
    }
}
#endif

template <class Impl>
void
BaseO3DynInst<Impl>::dump()
{
    cprintf("T%d : %#08d `", threadNumber, pc.instAddr());
    std::cout << staticInst->disassemble(pc.instAddr());
    cprintf("'\n");
}

template <class Impl>
void
BaseO3DynInst<Impl>::dump(std::string &outstring)
{
    std::ostringstream s;
    s << "T" << threadNumber << " : 0x" << pc.instAddr() << " "
      << staticInst->disassemble(pc.instAddr());

    outstring = s.str();
}

template <class Impl>
void
BaseO3DynInst<Impl>::markSrcRegReady()
{
    DPRINTF(IQ, "[sn:%lli] has %d ready out of %d sources. RTI %d)\n",
            seqNum, readyRegs+1, numSrcRegs(), readyToIssue());
    if (++readyRegs == numSrcRegs()) {
        setCanIssue();
    }
}

template <class Impl>
void
BaseO3DynInst<Impl>::markSrcRegReady(RegIndex src_idx)
{
    regs.readySrcIdx(src_idx, true);
    markSrcRegReady();
}

template <class Impl>
bool
BaseO3DynInst<Impl>::eaSrcsReady() const
{
    // For now I am assuming that src registers 1..n-1 are the ones that the
    // EA calc depends on.  (i.e. src reg 0 is the source of the data to be
    // stored)

    for (int i = 1; i < numSrcRegs(); ++i) {
        if (!regs.readySrcIdx(i))
            return false;
    }

    return true;
}



template <class Impl>
void
BaseO3DynInst<Impl>::setSquashed()
{
    status.set(Squashed);

    if (!isPinnedRegsRenamed() || isPinnedRegsSquashDone())
        return;

    // This inst has been renamed already so it may go through rename
    // again (e.g. if the squash is due to memory access order violation).
    // Reset the write counters for all pinned destination register to ensure
    // that they are in a consistent state for a possible re-rename. This also
    // ensures that dest regs will be pinned to the same phys register if
    // re-rename happens.
    for (int idx = 0; idx < numDestRegs(); idx++) {
        PhysRegIdPtr phys_dest_reg = regs.renamedDestIdx(idx);
        if (phys_dest_reg->isPinned()) {
            phys_dest_reg->incrNumPinnedWrites();
            if (isPinnedRegsWritten())
                phys_dest_reg->incrNumPinnedWritesToComplete();
        }
    }
    setPinnedRegsSquashDone();
}

template <class Impl>
Fault
BaseO3DynInst<Impl>::execute()
{
    // @todo: Pretty convoluted way to avoid squashing from happening
    // when using the TC during an instruction's execution
    // (specifically for instructions that have side-effects that use
    // the TC).  Fix this.
    bool no_squash_from_TC = this->thread->noSquashFromTC;
    this->thread->noSquashFromTC = true;

    this->fault = this->staticInst->execute(this, this->traceData);

    this->thread->noSquashFromTC = no_squash_from_TC;

    return this->fault;
}

template <class Impl>
Fault
BaseO3DynInst<Impl>::initiateAcc()
{
    // @todo: Pretty convoluted way to avoid squashing from happening
    // when using the TC during an instruction's execution
    // (specifically for instructions that have side-effects that use
    // the TC).  Fix this.
    bool no_squash_from_TC = this->thread->noSquashFromTC;
    this->thread->noSquashFromTC = true;

    this->fault = this->staticInst->initiateAcc(this, this->traceData);

    this->thread->noSquashFromTC = no_squash_from_TC;

    return this->fault;
}

template <class Impl>
Fault
BaseO3DynInst<Impl>::completeAcc(PacketPtr pkt)
{
    // @todo: Pretty convoluted way to avoid squashing from happening
    // when using the TC during an instruction's execution
    // (specifically for instructions that have side-effects that use
    // the TC).  Fix this.
    bool no_squash_from_TC = this->thread->noSquashFromTC;
    this->thread->noSquashFromTC = true;

    if (this->cpu->checker) {
        if (this->isStoreConditional()) {
            this->reqToVerify->setExtraData(pkt->req->getExtraData());
        }
    }

    this->fault = this->staticInst->completeAcc(pkt, this, this->traceData);

    this->thread->noSquashFromTC = no_squash_from_TC;

    return this->fault;
}

template <class Impl>
void
BaseO3DynInst<Impl>::trap(const Fault &fault)
{
    this->cpu->trap(fault, this->threadNumber, this->staticInst);
}

#endif//__CPU_O3_DYN_INST_IMPL_HH__
