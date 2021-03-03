/*
 * Copyright (c) 2011-2012, 2014, 2016, 2017, 2019-2020 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
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
 * Copyright (c) 2011 Regents of the University of California
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

#include "cpu/o3/cpu.hh"

#include "config/the_isa.hh"
#include "cpu/activity.hh"
#include "cpu/checker/cpu.hh"
#include "cpu/checker/thread_context.hh"
#include "cpu/o3/isa_specific.hh"
#include "cpu/o3/limits.hh"
#include "cpu/o3/thread_context.hh"
#include "cpu/simple_thread.hh"
#include "cpu/thread_context.hh"
#include "debug/Activity.hh"
#include "debug/Drain.hh"
#include "debug/O3CPU.hh"
#include "debug/Quiesce.hh"
#include "enums/MemoryMode.hh"
#include "sim/core.hh"
#include "sim/full_system.hh"
#include "sim/process.hh"
#include "sim/stat_control.hh"
#include "sim/system.hh"

struct BaseCPUParams;

FullO3CPU::FullO3CPU(const DerivO3CPUParams &params)
    : BaseCPU(params),
      mmu(params.mmu),
      tickEvent([this]{ tick(); }, "FullO3CPU tick",
                false, Event::CPU_Tick_Pri),
      threadExitEvent([this]{ exitThreads(); }, "FullO3CPU exit threads",
                false, Event::CPU_Exit_Pri),
#ifndef NDEBUG
      instcount(0),
#endif
      removeInstsThisCycle(false),
      fetch(this, params),
      decode(this, params),
      rename(this, params),
      iew(this, params),
      commit(this, params),

      /* It is mandatory that all SMT threads use the same renaming mode as
       * they are sharing registers and rename */
      vecMode(params.isa[0]->initVecRegRenameMode()),
      regFile(params.numPhysIntRegs,
              params.numPhysFloatRegs,
              params.numPhysVecRegs,
              params.numPhysVecPredRegs,
              params.numPhysCCRegs,
              params.isa[0]->regClasses(),
              vecMode),

      freeList(name() + ".freelist", &regFile),

      rob(this, params),

      scoreboard(name() + ".scoreboard", regFile.totalNumPhysRegs(),
              params.isa[0]->regClasses().at(IntRegClass).zeroReg()),

      isa(numThreads, NULL),

      timeBuffer(params.backComSize, params.forwardComSize),
      fetchQueue(params.backComSize, params.forwardComSize),
      decodeQueue(params.backComSize, params.forwardComSize),
      renameQueue(params.backComSize, params.forwardComSize),
      iewQueue(params.backComSize, params.forwardComSize),
      activityRec(name(), NumStages,
                  params.backComSize + params.forwardComSize,
                  params.activity),

      globalSeqNum(1),
      system(params.system),
      lastRunningCycle(curCycle()),
      cpuStats(this)
{
    fatal_if(FullSystem && params.numThreads > 1,
            "SMT is not supported in O3 in full system mode currently.");

    fatal_if(!FullSystem && params.numThreads < params.workload.size(),
            "More workload items (%d) than threads (%d) on CPU %s.",
            params.workload.size(), params.numThreads, name());

    if (!params.switched_out) {
        _status = Running;
    } else {
        _status = SwitchedOut;
    }

    if (params.checker) {
        BaseCPU *temp_checker = params.checker;
        checker = dynamic_cast<Checker<O3DynInstPtr> *>(temp_checker);
        checker->setIcachePort(&fetch.getInstPort());
        checker->setSystem(params.system);
    } else {
        checker = NULL;
    }

    if (!FullSystem) {
        thread.resize(numThreads);
        tids.resize(numThreads);
    }

    // The stages also need their CPU pointer setup.  However this
    // must be done at the upper level CPU because they have pointers
    // to the upper level CPU, and not this FullO3CPU.

    // Set up Pointers to the activeThreads list for each stage
    fetch.setActiveThreads(&activeThreads);
    decode.setActiveThreads(&activeThreads);
    rename.setActiveThreads(&activeThreads);
    iew.setActiveThreads(&activeThreads);
    commit.setActiveThreads(&activeThreads);

    // Give each of the stages the time buffer they will use.
    fetch.setTimeBuffer(&timeBuffer);
    decode.setTimeBuffer(&timeBuffer);
    rename.setTimeBuffer(&timeBuffer);
    iew.setTimeBuffer(&timeBuffer);
    commit.setTimeBuffer(&timeBuffer);

    // Also setup each of the stages' queues.
    fetch.setFetchQueue(&fetchQueue);
    decode.setFetchQueue(&fetchQueue);
    commit.setFetchQueue(&fetchQueue);
    decode.setDecodeQueue(&decodeQueue);
    rename.setDecodeQueue(&decodeQueue);
    rename.setRenameQueue(&renameQueue);
    iew.setRenameQueue(&renameQueue);
    iew.setIEWQueue(&iewQueue);
    commit.setIEWQueue(&iewQueue);
    commit.setRenameQueue(&renameQueue);

    commit.setIEWStage(&iew);
    rename.setIEWStage(&iew);
    rename.setCommitStage(&commit);

    ThreadID active_threads;
    if (FullSystem) {
        active_threads = 1;
    } else {
        active_threads = params.workload.size();

        if (active_threads > O3MaxThreads) {
            panic("Workload Size too large. Increase the 'O3MaxThreads' "
                  "constant in cpu/o3/limits.hh or edit your workload size.");
        }
    }

    // Make Sure That this a Valid Architeture
    assert(numThreads);
    const auto &regClasses = params.isa[0]->regClasses();

    assert(params.numPhysIntRegs >=
            numThreads * regClasses.at(IntRegClass).size());
    assert(params.numPhysFloatRegs >=
            numThreads * regClasses.at(FloatRegClass).size());
    assert(params.numPhysVecRegs >=
            numThreads * regClasses.at(VecRegClass).size());
    assert(params.numPhysVecPredRegs >=
            numThreads * regClasses.at(VecPredRegClass).size());
    assert(params.numPhysCCRegs >=
            numThreads * regClasses.at(CCRegClass).size());

    // Just make this a warning and go ahead anyway, to keep from having to
    // add checks everywhere.
    warn_if(regClasses.at(CCRegClass).size() == 0 && params.numPhysCCRegs != 0,
            "Non-zero number of physical CC regs specified, even though\n"
            "    ISA does not use them.");

    rename.setScoreboard(&scoreboard);
    iew.setScoreboard(&scoreboard);

    // Setup the rename map for whichever stages need it.
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        isa[tid] = dynamic_cast<TheISA::ISA *>(params.isa[tid]);
        assert(isa[tid]);
        assert(isa[tid]->initVecRegRenameMode() ==
                isa[0]->initVecRegRenameMode());

        commitRenameMap[tid].init(regClasses, &regFile, &freeList, vecMode);
        renameMap[tid].init(regClasses, &regFile, &freeList, vecMode);
    }

    // Initialize rename map to assign physical registers to the
    // architectural registers for active threads only.
    for (ThreadID tid = 0; tid < active_threads; tid++) {
        for (RegIndex ridx = 0; ridx < regClasses.at(IntRegClass).size();
                ++ridx) {
            // Note that we can't use the rename() method because we don't
            // want special treatment for the zero register at this point
            PhysRegIdPtr phys_reg = freeList.getIntReg();
            renameMap[tid].setEntry(RegId(IntRegClass, ridx), phys_reg);
            commitRenameMap[tid].setEntry(RegId(IntRegClass, ridx), phys_reg);
        }

        for (RegIndex ridx = 0; ridx < regClasses.at(FloatRegClass).size();
                ++ridx) {
            PhysRegIdPtr phys_reg = freeList.getFloatReg();
            renameMap[tid].setEntry(RegId(FloatRegClass, ridx), phys_reg);
            commitRenameMap[tid].setEntry(
                    RegId(FloatRegClass, ridx), phys_reg);
        }

        /* Here we need two 'interfaces' the 'whole register' and the
         * 'register element'. At any point only one of them will be
         * active. */
        const size_t numVecs = regClasses.at(VecRegClass).size();
        if (vecMode == Enums::Full) {
            /* Initialize the full-vector interface */
            for (RegIndex ridx = 0; ridx < numVecs; ++ridx) {
                RegId rid = RegId(VecRegClass, ridx);
                PhysRegIdPtr phys_reg = freeList.getVecReg();
                renameMap[tid].setEntry(rid, phys_reg);
                commitRenameMap[tid].setEntry(rid, phys_reg);
            }
        } else {
            /* Initialize the vector-element interface */
            const size_t numElems = regClasses.at(VecElemClass).size();
            const size_t elemsPerVec = numElems / numVecs;
            for (RegIndex ridx = 0; ridx < numVecs; ++ridx) {
                for (ElemIndex ldx = 0; ldx < elemsPerVec; ++ldx) {
                    RegId lrid = RegId(VecElemClass, ridx, ldx);
                    PhysRegIdPtr phys_elem = freeList.getVecElem();
                    renameMap[tid].setEntry(lrid, phys_elem);
                    commitRenameMap[tid].setEntry(lrid, phys_elem);
                }
            }
        }

        for (RegIndex ridx = 0; ridx < regClasses.at(VecPredRegClass).size();
                ++ridx) {
            PhysRegIdPtr phys_reg = freeList.getVecPredReg();
            renameMap[tid].setEntry(RegId(VecPredRegClass, ridx), phys_reg);
            commitRenameMap[tid].setEntry(
                    RegId(VecPredRegClass, ridx), phys_reg);
        }

        for (RegIndex ridx = 0; ridx < regClasses.at(CCRegClass).size();
                ++ridx) {
            PhysRegIdPtr phys_reg = freeList.getCCReg();
            renameMap[tid].setEntry(RegId(CCRegClass, ridx), phys_reg);
            commitRenameMap[tid].setEntry(RegId(CCRegClass, ridx), phys_reg);
        }
    }

    rename.setRenameMap(renameMap);
    commit.setRenameMap(commitRenameMap);
    rename.setFreeList(&freeList);

    // Setup the ROB for whichever stages need it.
    commit.setROB(&rob);

    lastActivatedCycle = 0;

    DPRINTF(O3CPU, "Creating O3CPU object.\n");

    // Setup any thread state.
    thread.resize(numThreads);

    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        if (FullSystem) {
            // SMT is not supported in FS mode yet.
            assert(numThreads == 1);
            thread[tid] = new O3ThreadState(this, 0, NULL);
        } else {
            if (tid < params.workload.size()) {
                DPRINTF(O3CPU, "Workload[%i] process is %#x", tid,
                        thread[tid]);
                thread[tid] = new O3ThreadState(this, tid,
                        params.workload[tid]);
            } else {
                //Allocate Empty thread so M5 can use later
                //when scheduling threads to CPU
                Process* dummy_proc = NULL;

                thread[tid] = new O3ThreadState(this, tid, dummy_proc);
            }
        }

        ThreadContext *tc;

        // Setup the TC that will serve as the interface to the threads/CPU.
        O3ThreadContext *o3_tc = new O3ThreadContext;

        tc = o3_tc;

        // If we're using a checker, then the TC should be the
        // CheckerThreadContext.
        if (params.checker) {
            tc = new CheckerThreadContext<O3ThreadContext>(o3_tc, checker);
        }

        o3_tc->cpu = this;
        o3_tc->thread = thread[tid];

        // Give the thread the TC.
        thread[tid]->tc = tc;

        // Add the TC to the CPU's list of TC's.
        threadContexts.push_back(tc);
    }

    // FullO3CPU always requires an interrupt controller.
    if (!params.switched_out && interrupts.empty()) {
        fatal("FullO3CPU %s has no interrupt controller.\n"
              "Ensure createInterruptController() is called.\n", name());
    }

    for (ThreadID tid = 0; tid < numThreads; tid++)
        thread[tid]->setFuncExeInst(0);
}

void
FullO3CPU::regProbePoints()
{
    BaseCPU::regProbePoints();

    ppInstAccessComplete = new ProbePointArg<PacketPtr>(
            getProbeManager(), "InstAccessComplete");
    ppDataAccessComplete = new ProbePointArg<
        std::pair<O3DynInstPtr, PacketPtr>>(
                getProbeManager(), "DataAccessComplete");

    fetch.regProbePoints();
    rename.regProbePoints();
    iew.regProbePoints();
    commit.regProbePoints();
}

FullO3CPU::FullO3CPUStats::FullO3CPUStats(FullO3CPU *cpu)
    : Stats::Group(cpu),
      ADD_STAT(timesIdled, Stats::Units::Count::get(),
               "Number of times that the entire CPU went into an idle state "
               "and unscheduled itself"),
      ADD_STAT(idleCycles, Stats::Units::Cycle::get(),
               "Total number of cycles that the CPU has spent unscheduled due "
               "to idling"),
      ADD_STAT(quiesceCycles, Stats::Units::Cycle::get(),
               "Total number of cycles that CPU has spent quiesced or waiting "
               "for an interrupt"),
      ADD_STAT(committedInsts, Stats::Units::Count::get(),
               "Number of Instructions Simulated"),
      ADD_STAT(committedOps, Stats::Units::Count::get(),
               "Number of Ops (including micro ops) Simulated"),
      ADD_STAT(cpi, Stats::Units::Rate<
                    Stats::Units::Cycle, Stats::Units::Count>::get(),
               "CPI: Cycles Per Instruction"),
      ADD_STAT(totalCpi, Stats::Units::Rate<
                    Stats::Units::Cycle, Stats::Units::Count>::get(),
               "CPI: Total CPI of All Threads"),
      ADD_STAT(ipc, Stats::Units::Rate<
                    Stats::Units::Count, Stats::Units::Cycle>::get(),
               "IPC: Instructions Per Cycle"),
      ADD_STAT(totalIpc, Stats::Units::Rate<
                    Stats::Units::Count, Stats::Units::Cycle>::get(),
               "IPC: Total IPC of All Threads"),
      ADD_STAT(intRegfileReads, Stats::Units::Count::get(),
               "Number of integer regfile reads"),
      ADD_STAT(intRegfileWrites, Stats::Units::Count::get(),
               "Number of integer regfile writes"),
      ADD_STAT(fpRegfileReads, Stats::Units::Count::get(),
               "Number of floating regfile reads"),
      ADD_STAT(fpRegfileWrites, Stats::Units::Count::get(),
               "Number of floating regfile writes"),
      ADD_STAT(vecRegfileReads, Stats::Units::Count::get(),
               "number of vector regfile reads"),
      ADD_STAT(vecRegfileWrites, Stats::Units::Count::get(),
               "number of vector regfile writes"),
      ADD_STAT(vecPredRegfileReads, Stats::Units::Count::get(),
               "number of predicate regfile reads"),
      ADD_STAT(vecPredRegfileWrites, Stats::Units::Count::get(),
               "number of predicate regfile writes"),
      ADD_STAT(ccRegfileReads, Stats::Units::Count::get(),
               "number of cc regfile reads"),
      ADD_STAT(ccRegfileWrites, Stats::Units::Count::get(),
               "number of cc regfile writes"),
      ADD_STAT(miscRegfileReads, Stats::Units::Count::get(),
               "number of misc regfile reads"),
      ADD_STAT(miscRegfileWrites, Stats::Units::Count::get(),
               "number of misc regfile writes")
{
    // Register any of the O3CPU's stats here.
    timesIdled
        .prereq(timesIdled);

    idleCycles
        .prereq(idleCycles);

    quiesceCycles
        .prereq(quiesceCycles);

    // Number of Instructions simulated
    // --------------------------------
    // Should probably be in Base CPU but need templated
    // O3MaxThreads so put in here instead
    committedInsts
        .init(cpu->numThreads)
        .flags(Stats::total);

    committedOps
        .init(cpu->numThreads)
        .flags(Stats::total);

    cpi
        .precision(6);
    cpi = cpu->baseStats.numCycles / committedInsts;

    totalCpi
        .precision(6);
    totalCpi = cpu->baseStats.numCycles / sum(committedInsts);

    ipc
        .precision(6);
    ipc = committedInsts / cpu->baseStats.numCycles;

    totalIpc
        .precision(6);
    totalIpc = sum(committedInsts) / cpu->baseStats.numCycles;

    intRegfileReads
        .prereq(intRegfileReads);

    intRegfileWrites
        .prereq(intRegfileWrites);

    fpRegfileReads
        .prereq(fpRegfileReads);

    fpRegfileWrites
        .prereq(fpRegfileWrites);

    vecRegfileReads
        .prereq(vecRegfileReads);

    vecRegfileWrites
        .prereq(vecRegfileWrites);

    vecPredRegfileReads
        .prereq(vecPredRegfileReads);

    vecPredRegfileWrites
        .prereq(vecPredRegfileWrites);

    ccRegfileReads
        .prereq(ccRegfileReads);

    ccRegfileWrites
        .prereq(ccRegfileWrites);

    miscRegfileReads
        .prereq(miscRegfileReads);

    miscRegfileWrites
        .prereq(miscRegfileWrites);
}

void
FullO3CPU::tick()
{
    DPRINTF(O3CPU, "\n\nFullO3CPU: Ticking main, FullO3CPU.\n");
    assert(!switchedOut());
    assert(drainState() != DrainState::Drained);

    ++baseStats.numCycles;
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

//    activity = false;

    //Tick each of the stages
    fetch.tick();

    decode.tick();

    rename.tick();

    iew.tick();

    commit.tick();

    // Now advance the time buffers
    timeBuffer.advance();

    fetchQueue.advance();
    decodeQueue.advance();
    renameQueue.advance();
    iewQueue.advance();

    activityRec.advance();

    if (removeInstsThisCycle) {
        cleanUpRemovedInsts();
    }

    if (!tickEvent.scheduled()) {
        if (_status == SwitchedOut) {
            DPRINTF(O3CPU, "Switched out!\n");
            // increment stat
            lastRunningCycle = curCycle();
        } else if (!activityRec.active() || _status == Idle) {
            DPRINTF(O3CPU, "Idle!\n");
            lastRunningCycle = curCycle();
            cpuStats.timesIdled++;
        } else {
            schedule(tickEvent, clockEdge(Cycles(1)));
            DPRINTF(O3CPU, "Scheduling next tick!\n");
        }
    }

    if (!FullSystem)
        updateThreadPriority();

    tryDrain();
}

void
FullO3CPU::init()
{
    BaseCPU::init();

    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        // Set noSquashFromTC so that the CPU doesn't squash when initially
        // setting up registers.
        thread[tid]->noSquashFromTC = true;
        // Initialise the ThreadContext's memory proxies
        thread[tid]->initMemProxies(thread[tid]->getTC());
    }

    // Clear noSquashFromTC.
    for (int tid = 0; tid < numThreads; ++tid)
        thread[tid]->noSquashFromTC = false;

    commit.setThreads(thread);
}

void
FullO3CPU::startup()
{
    BaseCPU::startup();

    fetch.startupStage();
    decode.startupStage();
    iew.startupStage();
    rename.startupStage();
    commit.startupStage();
}

void
FullO3CPU::activateThread(ThreadID tid)
{
    std::list<ThreadID>::iterator isActive =
        std::find(activeThreads.begin(), activeThreads.end(), tid);

    DPRINTF(O3CPU, "[tid:%i] Calling activate thread.\n", tid);
    assert(!switchedOut());

    if (isActive == activeThreads.end()) {
        DPRINTF(O3CPU, "[tid:%i] Adding to active threads list\n",
                tid);

        activeThreads.push_back(tid);
    }
}

void
FullO3CPU::deactivateThread(ThreadID tid)
{
    // hardware transactional memory
    // shouldn't deactivate thread in the middle of a transaction
    assert(!commit.executingHtmTransaction(tid));

    //Remove From Active List, if Active
    std::list<ThreadID>::iterator thread_it =
        std::find(activeThreads.begin(), activeThreads.end(), tid);

    DPRINTF(O3CPU, "[tid:%i] Calling deactivate thread.\n", tid);
    assert(!switchedOut());

    if (thread_it != activeThreads.end()) {
        DPRINTF(O3CPU,"[tid:%i] Removing from active threads list\n",
                tid);
        activeThreads.erase(thread_it);
    }

    fetch.deactivateThread(tid);
    commit.deactivateThread(tid);
}

Counter
FullO3CPU::totalInsts() const
{
    Counter total(0);

    ThreadID size = thread.size();
    for (ThreadID i = 0; i < size; i++)
        total += thread[i]->numInst;

    return total;
}

Counter
FullO3CPU::totalOps() const
{
    Counter total(0);

    ThreadID size = thread.size();
    for (ThreadID i = 0; i < size; i++)
        total += thread[i]->numOp;

    return total;
}

void
FullO3CPU::activateContext(ThreadID tid)
{
    assert(!switchedOut());

    // Needs to set each stage to running as well.
    activateThread(tid);

    // We don't want to wake the CPU if it is drained. In that case,
    // we just want to flag the thread as active and schedule the tick
    // event from drainResume() instead.
    if (drainState() == DrainState::Drained)
        return;

    // If we are time 0 or if the last activation time is in the past,
    // schedule the next tick and wake up the fetch unit
    if (lastActivatedCycle == 0 || lastActivatedCycle < curTick()) {
        scheduleTickEvent(Cycles(0));

        // Be sure to signal that there's some activity so the CPU doesn't
        // deschedule itself.
        activityRec.activity();
        fetch.wakeFromQuiesce();

        Cycles cycles(curCycle() - lastRunningCycle);
        // @todo: This is an oddity that is only here to match the stats
        if (cycles != 0)
            --cycles;
        cpuStats.quiesceCycles += cycles;

        lastActivatedCycle = curTick();

        _status = Running;

        BaseCPU::activateContext(tid);
    }
}

void
FullO3CPU::suspendContext(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Suspending Thread Context.\n", tid);
    assert(!switchedOut());

    deactivateThread(tid);

    // If this was the last thread then unschedule the tick event.
    if (activeThreads.size() == 0) {
        unscheduleTickEvent();
        lastRunningCycle = curCycle();
        _status = Idle;
    }

    DPRINTF(Quiesce, "Suspending Context\n");

    BaseCPU::suspendContext(tid);
}

void
FullO3CPU::haltContext(ThreadID tid)
{
    //For now, this is the same as deallocate
    DPRINTF(O3CPU,"[tid:%i] Halt Context called. Deallocating\n", tid);
    assert(!switchedOut());

    deactivateThread(tid);
    removeThread(tid);

    // If this was the last thread then unschedule the tick event.
    if (activeThreads.size() == 0) {
        if (tickEvent.scheduled())
        {
            unscheduleTickEvent();
        }
        lastRunningCycle = curCycle();
        _status = Idle;
    }
    updateCycleCounters(BaseCPU::CPU_STATE_SLEEP);
}

void
FullO3CPU::insertThread(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Initializing thread into CPU");
    // Will change now that the PC and thread state is internal to the CPU
    // and not in the ThreadContext.
    ThreadContext *src_tc;
    if (FullSystem)
        src_tc = system->threads[tid];
    else
        src_tc = tcBase(tid);

    //Bind Int Regs to Rename Map
    const auto &regClasses = isa[tid]->regClasses();

    for (RegIndex idx = 0; idx < regClasses.at(IntRegClass).size(); idx++) {
        PhysRegIdPtr phys_reg = freeList.getIntReg();
        renameMap[tid].setEntry(RegId(IntRegClass, idx), phys_reg);
        scoreboard.setReg(phys_reg);
    }

    //Bind Float Regs to Rename Map
    for (RegIndex idx = 0; idx < regClasses.at(FloatRegClass).size(); idx++) {
        PhysRegIdPtr phys_reg = freeList.getFloatReg();
        renameMap[tid].setEntry(RegId(FloatRegClass, idx), phys_reg);
        scoreboard.setReg(phys_reg);
    }

    //Bind condition-code Regs to Rename Map
    for (RegIndex idx = 0; idx < regClasses.at(CCRegClass).size(); idx++) {
        PhysRegIdPtr phys_reg = freeList.getCCReg();
        renameMap[tid].setEntry(RegId(CCRegClass, idx), phys_reg);
        scoreboard.setReg(phys_reg);
    }

    //Copy Thread Data Into RegFile
    //copyFromTC(tid);

    //Set PC/NPC/NNPC
    pcState(src_tc->pcState(), tid);

    src_tc->setStatus(ThreadContext::Active);

    activateContext(tid);

    //Reset ROB/IQ/LSQ Entries
    commit.rob->resetEntries();
}

void
FullO3CPU::removeThread(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Removing thread context from CPU.\n", tid);

    // Copy Thread Data From RegFile
    // If thread is suspended, it might be re-allocated
    // copyToTC(tid);


    // @todo: 2-27-2008: Fix how we free up rename mappings
    // here to alleviate the case for double-freeing registers
    // in SMT workloads.

    // clear all thread-specific states in each stage of the pipeline
    // since this thread is going to be completely removed from the CPU
    commit.clearStates(tid);
    fetch.clearStates(tid);
    decode.clearStates(tid);
    rename.clearStates(tid);
    iew.clearStates(tid);

    // Flush out any old data from the time buffers.
    for (int i = 0; i < timeBuffer.getSize(); ++i) {
        timeBuffer.advance();
        fetchQueue.advance();
        decodeQueue.advance();
        renameQueue.advance();
        iewQueue.advance();
    }

    // at this step, all instructions in the pipeline should be already
    // either committed successfully or squashed. All thread-specific
    // queues in the pipeline must be empty.
    assert(iew.instQueue.getCount(tid) == 0);
    assert(iew.ldstQueue.getCount(tid) == 0);
    assert(commit.rob->isEmpty(tid));

    // Reset ROB/IQ/LSQ Entries

    // Commented out for now.  This should be possible to do by
    // telling all the pipeline stages to drain first, and then
    // checking until the drain completes.  Once the pipeline is
    // drained, call resetEntries(). - 10-09-06 ktlim
/*
    if (activeThreads.size() >= 1) {
        commit.rob->resetEntries();
        iew.resetEntries();
    }
*/
}

void
FullO3CPU::setVectorsAsReady(ThreadID tid)
{
    const auto &regClasses = isa[tid]->regClasses();

    const size_t numVecs = regClasses.at(VecRegClass).size();
    if (vecMode == Enums::Elem) {
        const size_t numElems = regClasses.at(VecElemClass).size();
        const size_t elemsPerVec = numElems / numVecs;
        for (auto v = 0; v < numVecs; v++) {
            for (auto e = 0; e < elemsPerVec; e++) {
                scoreboard.setReg(commitRenameMap[tid].lookup(
                            RegId(VecElemClass, v, e)));
            }
        }
    } else if (vecMode == Enums::Full) {
        for (auto v = 0; v < numVecs; v++) {
            scoreboard.setReg(commitRenameMap[tid].lookup(
                        RegId(VecRegClass, v)));
        }
    }
}

void
FullO3CPU::switchRenameMode(ThreadID tid, UnifiedFreeList* freelist)
{
    auto pc = pcState(tid);

    // new_mode is the new vector renaming mode
    auto new_mode = isa[tid]->vecRegRenameMode(thread[tid]->getTC());

    // We update vecMode only if there has been a change
    if (new_mode != vecMode) {
        vecMode = new_mode;

        renameMap[tid].switchMode(vecMode);
        commitRenameMap[tid].switchMode(vecMode);
        renameMap[tid].switchFreeList(freelist);
        setVectorsAsReady(tid);
    }
}

Fault
FullO3CPU::getInterrupts()
{
    // Check if there are any outstanding interrupts
    return interrupts[0]->getInterrupt();
}

void
FullO3CPU::processInterrupts(const Fault &interrupt)
{
    // Check for interrupts here.  For now can copy the code that
    // exists within isa_fullsys_traits.hh.  Also assume that thread 0
    // is the one that handles the interrupts.
    // @todo: Possibly consolidate the interrupt checking code.
    // @todo: Allow other threads to handle interrupts.

    assert(interrupt != NoFault);
    interrupts[0]->updateIntrInfo();

    DPRINTF(O3CPU, "Interrupt %s being handled\n", interrupt->name());
    trap(interrupt, 0, nullptr);
}

void
FullO3CPU::trap(const Fault &fault, ThreadID tid, const StaticInstPtr &inst)
{
    // Pass the thread's TC into the invoke method.
    fault->invoke(threadContexts[tid], inst);
}

void
FullO3CPU::serializeThread(CheckpointOut &cp, ThreadID tid) const
{
    thread[tid]->serialize(cp);
}

void
FullO3CPU::unserializeThread(CheckpointIn &cp, ThreadID tid)
{
    thread[tid]->unserialize(cp);
}

DrainState
FullO3CPU::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    // If the CPU isn't doing anything, then return immediately.
    if (switchedOut())
        return DrainState::Drained;

    DPRINTF(Drain, "Draining...\n");

    // We only need to signal a drain to the commit stage as this
    // initiates squashing controls the draining. Once the commit
    // stage commits an instruction where it is safe to stop, it'll
    // squash the rest of the instructions in the pipeline and force
    // the fetch stage to stall. The pipeline will be drained once all
    // in-flight instructions have retired.
    commit.drain();

    // Wake the CPU and record activity so everything can drain out if
    // the CPU was not able to immediately drain.
    if (!isCpuDrained())  {
        // If a thread is suspended, wake it up so it can be drained
        for (auto t : threadContexts) {
            if (t->status() == ThreadContext::Suspended){
                DPRINTF(Drain, "Currently suspended so activate %i \n",
                        t->threadId());
                t->activate();
                // As the thread is now active, change the power state as well
                activateContext(t->threadId());
            }
        }

        wakeCPU();
        activityRec.activity();

        DPRINTF(Drain, "CPU not drained\n");

        return DrainState::Draining;
    } else {
        DPRINTF(Drain, "CPU is already drained\n");
        if (tickEvent.scheduled())
            deschedule(tickEvent);

        // Flush out any old data from the time buffers.  In
        // particular, there might be some data in flight from the
        // fetch stage that isn't visible in any of the CPU buffers we
        // test in isCpuDrained().
        for (int i = 0; i < timeBuffer.getSize(); ++i) {
            timeBuffer.advance();
            fetchQueue.advance();
            decodeQueue.advance();
            renameQueue.advance();
            iewQueue.advance();
        }

        drainSanityCheck();
        return DrainState::Drained;
    }
}

bool
FullO3CPU::tryDrain()
{
    if (drainState() != DrainState::Draining || !isCpuDrained())
        return false;

    if (tickEvent.scheduled())
        deschedule(tickEvent);

    DPRINTF(Drain, "CPU done draining, processing drain event\n");
    signalDrainDone();

    return true;
}

void
FullO3CPU::drainSanityCheck() const
{
    assert(isCpuDrained());
    fetch.drainSanityCheck();
    decode.drainSanityCheck();
    rename.drainSanityCheck();
    iew.drainSanityCheck();
    commit.drainSanityCheck();
}

bool
FullO3CPU::isCpuDrained() const
{
    bool drained(true);

    if (!instList.empty() || !removeList.empty()) {
        DPRINTF(Drain, "Main CPU structures not drained.\n");
        drained = false;
    }

    if (!fetch.isDrained()) {
        DPRINTF(Drain, "Fetch not drained.\n");
        drained = false;
    }

    if (!decode.isDrained()) {
        DPRINTF(Drain, "Decode not drained.\n");
        drained = false;
    }

    if (!rename.isDrained()) {
        DPRINTF(Drain, "Rename not drained.\n");
        drained = false;
    }

    if (!iew.isDrained()) {
        DPRINTF(Drain, "IEW not drained.\n");
        drained = false;
    }

    if (!commit.isDrained()) {
        DPRINTF(Drain, "Commit not drained.\n");
        drained = false;
    }

    return drained;
}

void
FullO3CPU::commitDrained(ThreadID tid)
{
    fetch.drainStall(tid);
}

void
FullO3CPU::drainResume()
{
    if (switchedOut())
        return;

    DPRINTF(Drain, "Resuming...\n");
    verifyMemoryMode();

    fetch.drainResume();
    commit.drainResume();

    _status = Idle;
    for (ThreadID i = 0; i < thread.size(); i++) {
        if (thread[i]->status() == ThreadContext::Active) {
            DPRINTF(Drain, "Activating thread: %i\n", i);
            activateThread(i);
            _status = Running;
        }
    }

    assert(!tickEvent.scheduled());
    if (_status == Running)
        schedule(tickEvent, nextCycle());

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();
}

void
FullO3CPU::switchOut()
{
    DPRINTF(O3CPU, "Switching out\n");
    BaseCPU::switchOut();

    activityRec.reset();

    _status = SwitchedOut;

    if (checker)
        checker->switchOut();
}

void
FullO3CPU::takeOverFrom(BaseCPU *oldCPU)
{
    BaseCPU::takeOverFrom(oldCPU);

    fetch.takeOverFrom();
    decode.takeOverFrom();
    rename.takeOverFrom();
    iew.takeOverFrom();
    commit.takeOverFrom();

    assert(!tickEvent.scheduled());

    auto *oldO3CPU = dynamic_cast<FullO3CPU *>(oldCPU);
    if (oldO3CPU)
        globalSeqNum = oldO3CPU->globalSeqNum;

    lastRunningCycle = curCycle();
    _status = Idle;
}

void
FullO3CPU::verifyMemoryMode() const
{
    if (!system->isTimingMode()) {
        fatal("The O3 CPU requires the memory system to be in "
              "'timing' mode.\n");
    }
}

RegVal
FullO3CPU::readMiscRegNoEffect(int misc_reg, ThreadID tid) const
{
    return isa[tid]->readMiscRegNoEffect(misc_reg);
}

RegVal
FullO3CPU::readMiscReg(int misc_reg, ThreadID tid)
{
    cpuStats.miscRegfileReads++;
    return isa[tid]->readMiscReg(misc_reg);
}

void
FullO3CPU::setMiscRegNoEffect(int misc_reg, RegVal val, ThreadID tid)
{
    isa[tid]->setMiscRegNoEffect(misc_reg, val);
}

void
FullO3CPU::setMiscReg(int misc_reg, RegVal val, ThreadID tid)
{
    cpuStats.miscRegfileWrites++;
    isa[tid]->setMiscReg(misc_reg, val);
}

RegVal
FullO3CPU::readIntReg(PhysRegIdPtr phys_reg)
{
    cpuStats.intRegfileReads++;
    return regFile.readIntReg(phys_reg);
}

RegVal
FullO3CPU::readFloatReg(PhysRegIdPtr phys_reg)
{
    cpuStats.fpRegfileReads++;
    return regFile.readFloatReg(phys_reg);
}

const TheISA::VecRegContainer&
FullO3CPU::readVecReg(PhysRegIdPtr phys_reg) const
{
    cpuStats.vecRegfileReads++;
    return regFile.readVecReg(phys_reg);
}

TheISA::VecRegContainer&
FullO3CPU::getWritableVecReg(PhysRegIdPtr phys_reg)
{
    cpuStats.vecRegfileWrites++;
    return regFile.getWritableVecReg(phys_reg);
}

const TheISA::VecElem&
FullO3CPU::readVecElem(PhysRegIdPtr phys_reg) const
{
    cpuStats.vecRegfileReads++;
    return regFile.readVecElem(phys_reg);
}

const TheISA::VecPredRegContainer&
FullO3CPU::readVecPredReg(PhysRegIdPtr phys_reg) const
{
    cpuStats.vecPredRegfileReads++;
    return regFile.readVecPredReg(phys_reg);
}

TheISA::VecPredRegContainer&
FullO3CPU::getWritableVecPredReg(PhysRegIdPtr phys_reg)
{
    cpuStats.vecPredRegfileWrites++;
    return regFile.getWritableVecPredReg(phys_reg);
}

RegVal
FullO3CPU::readCCReg(PhysRegIdPtr phys_reg)
{
    cpuStats.ccRegfileReads++;
    return regFile.readCCReg(phys_reg);
}

void
FullO3CPU::setIntReg(PhysRegIdPtr phys_reg, RegVal val)
{
    cpuStats.intRegfileWrites++;
    regFile.setIntReg(phys_reg, val);
}

void
FullO3CPU::setFloatReg(PhysRegIdPtr phys_reg, RegVal val)
{
    cpuStats.fpRegfileWrites++;
    regFile.setFloatReg(phys_reg, val);
}

void
FullO3CPU::setVecReg(PhysRegIdPtr phys_reg, const TheISA::VecRegContainer& val)
{
    cpuStats.vecRegfileWrites++;
    regFile.setVecReg(phys_reg, val);
}

void
FullO3CPU::setVecElem(PhysRegIdPtr phys_reg, const TheISA::VecElem& val)
{
    cpuStats.vecRegfileWrites++;
    regFile.setVecElem(phys_reg, val);
}

void
FullO3CPU::setVecPredReg(PhysRegIdPtr phys_reg,
                               const TheISA::VecPredRegContainer& val)
{
    cpuStats.vecPredRegfileWrites++;
    regFile.setVecPredReg(phys_reg, val);
}

void
FullO3CPU::setCCReg(PhysRegIdPtr phys_reg, RegVal val)
{
    cpuStats.ccRegfileWrites++;
    regFile.setCCReg(phys_reg, val);
}

RegVal
FullO3CPU::readArchIntReg(int reg_idx, ThreadID tid)
{
    cpuStats.intRegfileReads++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
            RegId(IntRegClass, reg_idx));

    return regFile.readIntReg(phys_reg);
}

RegVal
FullO3CPU::readArchFloatReg(int reg_idx, ThreadID tid)
{
    cpuStats.fpRegfileReads++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
        RegId(FloatRegClass, reg_idx));

    return regFile.readFloatReg(phys_reg);
}

const TheISA::VecRegContainer&
FullO3CPU::readArchVecReg(int reg_idx, ThreadID tid) const
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecRegClass, reg_idx));
    return readVecReg(phys_reg);
}

TheISA::VecRegContainer&
FullO3CPU::getWritableArchVecReg(int reg_idx, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecRegClass, reg_idx));
    return getWritableVecReg(phys_reg);
}

const TheISA::VecElem&
FullO3CPU::readArchVecElem(
        const RegIndex& reg_idx, const ElemIndex& ldx, ThreadID tid) const
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                                RegId(VecElemClass, reg_idx, ldx));
    return readVecElem(phys_reg);
}

const TheISA::VecPredRegContainer&
FullO3CPU::readArchVecPredReg(int reg_idx, ThreadID tid) const
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecPredRegClass, reg_idx));
    return readVecPredReg(phys_reg);
}

TheISA::VecPredRegContainer&
FullO3CPU::getWritableArchVecPredReg(int reg_idx, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecPredRegClass, reg_idx));
    return getWritableVecPredReg(phys_reg);
}

RegVal
FullO3CPU::readArchCCReg(int reg_idx, ThreadID tid)
{
    cpuStats.ccRegfileReads++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
        RegId(CCRegClass, reg_idx));

    return regFile.readCCReg(phys_reg);
}

void
FullO3CPU::setArchIntReg(int reg_idx, RegVal val, ThreadID tid)
{
    cpuStats.intRegfileWrites++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
            RegId(IntRegClass, reg_idx));

    regFile.setIntReg(phys_reg, val);
}

void
FullO3CPU::setArchFloatReg(int reg_idx, RegVal val, ThreadID tid)
{
    cpuStats.fpRegfileWrites++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
            RegId(FloatRegClass, reg_idx));

    regFile.setFloatReg(phys_reg, val);
}

void
FullO3CPU::setArchVecReg(int reg_idx, const TheISA::VecRegContainer& val,
        ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecRegClass, reg_idx));
    setVecReg(phys_reg, val);
}

void
FullO3CPU::setArchVecElem(const RegIndex& reg_idx, const ElemIndex& ldx,
                                const TheISA::VecElem& val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecElemClass, reg_idx, ldx));
    setVecElem(phys_reg, val);
}

void
FullO3CPU::setArchVecPredReg(int reg_idx,
        const TheISA::VecPredRegContainer& val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecPredRegClass, reg_idx));
    setVecPredReg(phys_reg, val);
}

void
FullO3CPU::setArchCCReg(int reg_idx, RegVal val, ThreadID tid)
{
    cpuStats.ccRegfileWrites++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
            RegId(CCRegClass, reg_idx));

    regFile.setCCReg(phys_reg, val);
}

TheISA::PCState
FullO3CPU::pcState(ThreadID tid)
{
    return commit.pcState(tid);
}

void
FullO3CPU::pcState(const TheISA::PCState &val, ThreadID tid)
{
    commit.pcState(val, tid);
}

Addr
FullO3CPU::instAddr(ThreadID tid)
{
    return commit.instAddr(tid);
}

Addr
FullO3CPU::nextInstAddr(ThreadID tid)
{
    return commit.nextInstAddr(tid);
}

MicroPC
FullO3CPU::microPC(ThreadID tid)
{
    return commit.microPC(tid);
}

void
FullO3CPU::squashFromTC(ThreadID tid)
{
    thread[tid]->noSquashFromTC = true;
    commit.generateTCEvent(tid);
}

FullO3CPU::ListIt
FullO3CPU::addInst(const O3DynInstPtr &inst)
{
    instList.push_back(inst);

    return --(instList.end());
}

void
FullO3CPU::instDone(ThreadID tid, const O3DynInstPtr &inst)
{
    // Keep an instruction count.
    if (!inst->isMicroop() || inst->isLastMicroop()) {
        thread[tid]->numInst++;
        thread[tid]->threadStats.numInsts++;
        cpuStats.committedInsts[tid]++;

        // Check for instruction-count-based events.
        thread[tid]->comInstEventQueue.serviceEvents(thread[tid]->numInst);
    }
    thread[tid]->numOp++;
    thread[tid]->threadStats.numOps++;
    cpuStats.committedOps[tid]++;

    probeInstCommit(inst->staticInst, inst->instAddr());
}

void
FullO3CPU::removeFrontInst(const O3DynInstPtr &inst)
{
    DPRINTF(O3CPU, "Removing committed instruction [tid:%i] PC %s "
            "[sn:%lli]\n",
            inst->threadNumber, inst->pcState(), inst->seqNum);

    removeInstsThisCycle = true;

    // Remove the front instruction.
    removeList.push(inst->getInstListIt());
}

void
FullO3CPU::removeInstsNotInROB(ThreadID tid)
{
    DPRINTF(O3CPU, "Thread %i: Deleting instructions from instruction"
            " list.\n", tid);

    ListIt end_it;

    bool rob_empty = false;

    if (instList.empty()) {
        return;
    } else if (rob.isEmpty(tid)) {
        DPRINTF(O3CPU, "ROB is empty, squashing all insts.\n");
        end_it = instList.begin();
        rob_empty = true;
    } else {
        end_it = (rob.readTailInst(tid))->getInstListIt();
        DPRINTF(O3CPU, "ROB is not empty, squashing insts not in ROB.\n");
    }

    removeInstsThisCycle = true;

    ListIt inst_it = instList.end();

    inst_it--;

    // Walk through the instruction list, removing any instructions
    // that were inserted after the given instruction iterator, end_it.
    while (inst_it != end_it) {
        assert(!instList.empty());

        squashInstIt(inst_it, tid);

        inst_it--;
    }

    // If the ROB was empty, then we actually need to remove the first
    // instruction as well.
    if (rob_empty) {
        squashInstIt(inst_it, tid);
    }
}

void
FullO3CPU::removeInstsUntil(const InstSeqNum &seq_num, ThreadID tid)
{
    assert(!instList.empty());

    removeInstsThisCycle = true;

    ListIt inst_iter = instList.end();

    inst_iter--;

    DPRINTF(O3CPU, "Deleting instructions from instruction "
            "list that are from [tid:%i] and above [sn:%lli] (end=%lli).\n",
            tid, seq_num, (*inst_iter)->seqNum);

    while ((*inst_iter)->seqNum > seq_num) {

        bool break_loop = (inst_iter == instList.begin());

        squashInstIt(inst_iter, tid);

        inst_iter--;

        if (break_loop)
            break;
    }
}

void
FullO3CPU::squashInstIt(const ListIt &instIt, ThreadID tid)
{
    if ((*instIt)->threadNumber == tid) {
        DPRINTF(O3CPU, "Squashing instruction, "
                "[tid:%i] [sn:%lli] PC %s\n",
                (*instIt)->threadNumber,
                (*instIt)->seqNum,
                (*instIt)->pcState());

        // Mark it as squashed.
        (*instIt)->setSquashed();

        // @todo: Formulate a consistent method for deleting
        // instructions from the instruction list
        // Remove the instruction from the list.
        removeList.push(instIt);
    }
}

void
FullO3CPU::cleanUpRemovedInsts()
{
    while (!removeList.empty()) {
        DPRINTF(O3CPU, "Removing instruction, "
                "[tid:%i] [sn:%lli] PC %s\n",
                (*removeList.front())->threadNumber,
                (*removeList.front())->seqNum,
                (*removeList.front())->pcState());

        instList.erase(removeList.front());

        removeList.pop();
    }

    removeInstsThisCycle = false;
}
/*
void
FullO3CPU::removeAllInsts()
{
    instList.clear();
}
*/
void
FullO3CPU::dumpInsts()
{
    int num = 0;

    ListIt inst_list_it = instList.begin();

    cprintf("Dumping Instruction List\n");

    while (inst_list_it != instList.end()) {
        cprintf("Instruction:%i\nPC:%#x\n[tid:%i]\n[sn:%lli]\nIssued:%i\n"
                "Squashed:%i\n\n",
                num, (*inst_list_it)->instAddr(), (*inst_list_it)->threadNumber,
                (*inst_list_it)->seqNum, (*inst_list_it)->isIssued(),
                (*inst_list_it)->isSquashed());
        inst_list_it++;
        ++num;
    }
}
/*
void
FullO3CPU::wakeDependents(const O3DynInstPtr &inst)
{
    iew.wakeDependents(inst);
}
*/
void
FullO3CPU::wakeCPU()
{
    if (activityRec.active() || tickEvent.scheduled()) {
        DPRINTF(Activity, "CPU already running.\n");
        return;
    }

    DPRINTF(Activity, "Waking up CPU\n");

    Cycles cycles(curCycle() - lastRunningCycle);
    // @todo: This is an oddity that is only here to match the stats
    if (cycles > 1) {
        --cycles;
        cpuStats.idleCycles += cycles;
        baseStats.numCycles += cycles;
    }

    schedule(tickEvent, clockEdge());
}

void
FullO3CPU::wakeup(ThreadID tid)
{
    if (thread[tid]->status() != ThreadContext::Suspended)
        return;

    wakeCPU();

    DPRINTF(Quiesce, "Suspended Processor woken\n");
    threadContexts[tid]->activate();
}

ThreadID
FullO3CPU::getFreeTid()
{
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (!tids[tid]) {
            tids[tid] = true;
            return tid;
        }
    }

    return InvalidThreadID;
}

void
FullO3CPU::updateThreadPriority()
{
    if (activeThreads.size() > 1) {
        //DEFAULT TO ROUND ROBIN SCHEME
        //e.g. Move highest priority to end of thread list
        std::list<ThreadID>::iterator list_begin = activeThreads.begin();

        unsigned high_thread = *list_begin;

        activeThreads.erase(list_begin);

        activeThreads.push_back(high_thread);
    }
}

void
FullO3CPU::addThreadToExitingList(ThreadID tid)
{
    DPRINTF(O3CPU, "Thread %d is inserted to exitingThreads list\n", tid);

    // the thread trying to exit can't be already halted
    assert(tcBase(tid)->status() != ThreadContext::Halted);

    // make sure the thread has not been added to the list yet
    assert(exitingThreads.count(tid) == 0);

    // add the thread to exitingThreads list to mark that this thread is
    // trying to exit. The boolean value in the pair denotes if a thread is
    // ready to exit. The thread is not ready to exit until the corresponding
    // exit trap event is processed in the future. Until then, it'll be still
    // an active thread that is trying to exit.
    exitingThreads.emplace(std::make_pair(tid, false));
}

bool
FullO3CPU::isThreadExiting(ThreadID tid) const
{
    return exitingThreads.count(tid) == 1;
}

void
FullO3CPU::scheduleThreadExitEvent(ThreadID tid)
{
    assert(exitingThreads.count(tid) == 1);

    // exit trap event has been processed. Now, the thread is ready to exit
    // and be removed from the CPU.
    exitingThreads[tid] = true;

    // we schedule a threadExitEvent in the next cycle to properly clean
    // up the thread's states in the pipeline. threadExitEvent has lower
    // priority than tickEvent, so the cleanup will happen at the very end
    // of the next cycle after all pipeline stages complete their operations.
    // We want all stages to complete squashing instructions before doing
    // the cleanup.
    if (!threadExitEvent.scheduled()) {
        schedule(threadExitEvent, nextCycle());
    }
}

void
FullO3CPU::exitThreads()
{
    // there must be at least one thread trying to exit
    assert(exitingThreads.size() > 0);

    // terminate all threads that are ready to exit
    auto it = exitingThreads.begin();
    while (it != exitingThreads.end()) {
        ThreadID thread_id = it->first;
        bool readyToExit = it->second;

        if (readyToExit) {
            DPRINTF(O3CPU, "Exiting thread %d\n", thread_id);
            haltContext(thread_id);
            tcBase(thread_id)->setStatus(ThreadContext::Halted);
            it = exitingThreads.erase(it);
        } else {
            it++;
        }
    }
}

void
FullO3CPU::htmSendAbortSignal(ThreadID tid, uint64_t htm_uid,
     HtmFailureFaultCause cause)
{
    const Addr addr = 0x0ul;
    const int size = 8;
    const Request::Flags flags =
      Request::PHYSICAL|Request::STRICT_ORDER|Request::HTM_ABORT;

    // O3-specific actions
    iew.ldstQueue.resetHtmStartsStops(tid);
    commit.resetHtmStartsStops(tid);

    // notify l1 d-cache (ruby) that core has aborted transaction
    RequestPtr req =
        std::make_shared<Request>(addr, size, flags, _dataRequestorId);

    req->taskId(taskId());
    req->setContext(thread[tid]->contextId());
    req->setHtmAbortCause(cause);

    assert(req->isHTMAbort());

    PacketPtr abort_pkt = Packet::createRead(req);
    uint8_t *memData = new uint8_t[8];
    assert(memData);
    abort_pkt->dataStatic(memData);
    abort_pkt->setHtmTransactional(htm_uid);

    // TODO include correct error handling here
    if (!iew.ldstQueue.getDataPort().sendTimingReq(abort_pkt)) {
        panic("HTM abort signal was not sent to the memory subsystem.");
    }
}
