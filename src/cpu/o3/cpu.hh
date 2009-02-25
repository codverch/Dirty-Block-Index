/*
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
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
 * Authors: Kevin Lim
 *          Korey Sewell
 */

#ifndef __CPU_O3_CPU_HH__
#define __CPU_O3_CPU_HH__

#include <iostream>
#include <list>
#include <queue>
#include <set>
#include <vector>

#include "arch/types.hh"
#include "base/statistics.hh"
#include "base/timebuf.hh"
#include "config/full_system.hh"
#include "config/use_checker.hh"
#include "cpu/activity.hh"
#include "cpu/base.hh"
#include "cpu/simple_thread.hh"
#include "cpu/o3/comm.hh"
#include "cpu/o3/cpu_policy.hh"
#include "cpu/o3/scoreboard.hh"
#include "cpu/o3/thread_state.hh"
//#include "cpu/o3/thread_context.hh"
#include "sim/process.hh"

#include "params/DerivO3CPU.hh"

template <class>
class Checker;
class ThreadContext;
template <class>
class O3ThreadContext;

class Checkpoint;
class MemObject;
class Process;

class BaseCPUParams;

class BaseO3CPU : public BaseCPU
{
    //Stuff that's pretty ISA independent will go here.
  public:
    BaseO3CPU(BaseCPUParams *params);

    void regStats();
};

/**
 * FullO3CPU class, has each of the stages (fetch through commit)
 * within it, as well as all of the time buffers between stages.  The
 * tick() function for the CPU is defined here.
 */
template <class Impl>
class FullO3CPU : public BaseO3CPU
{
  public:
    // Typedefs from the Impl here.
    typedef typename Impl::CPUPol CPUPolicy;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::O3CPU O3CPU;

    typedef O3ThreadState<Impl> ImplState;
    typedef O3ThreadState<Impl> Thread;

    typedef typename std::list<DynInstPtr>::iterator ListIt;

    friend class O3ThreadContext<Impl>;

  public:
    enum Status {
        Running,
        Idle,
        Halted,
        Blocked,
        SwitchedOut
    };

    TheISA::ITB * itb;
    TheISA::DTB * dtb;

    /** Overall CPU status. */
    Status _status;

    /** Per-thread status in CPU, used for SMT.  */
    Status _threadStatus[Impl::MaxThreads];

  private:
    class TickEvent : public Event
    {
      private:
        /** Pointer to the CPU. */
        FullO3CPU<Impl> *cpu;

      public:
        /** Constructs a tick event. */
        TickEvent(FullO3CPU<Impl> *c);

        /** Processes a tick event, calling tick() on the CPU. */
        void process();
        /** Returns the description of the tick event. */
        const char *description() const;
    };

    /** The tick event used for scheduling CPU ticks. */
    TickEvent tickEvent;

    /** Schedule tick event, regardless of its current state. */
    void scheduleTickEvent(int delay)
    {
        if (tickEvent.squashed())
            reschedule(tickEvent, nextCycle(curTick + ticks(delay)));
        else if (!tickEvent.scheduled())
            schedule(tickEvent, nextCycle(curTick + ticks(delay)));
    }

    /** Unschedule tick event, regardless of its current state. */
    void unscheduleTickEvent()
    {
        if (tickEvent.scheduled())
            tickEvent.squash();
    }

    class ActivateThreadEvent : public Event
    {
      private:
        /** Number of Thread to Activate */
        int tid;

        /** Pointer to the CPU. */
        FullO3CPU<Impl> *cpu;

      public:
        /** Constructs the event. */
        ActivateThreadEvent();

        /** Initialize Event */
        void init(int thread_num, FullO3CPU<Impl> *thread_cpu);

        /** Processes the event, calling activateThread() on the CPU. */
        void process();

        /** Returns the description of the event. */
        const char *description() const;
    };

    /** Schedule thread to activate , regardless of its current state. */
    void scheduleActivateThreadEvent(int tid, int delay)
    {
        // Schedule thread to activate, regardless of its current state.
        if (activateThreadEvent[tid].squashed())
            reschedule(activateThreadEvent[tid],
                nextCycle(curTick + ticks(delay)));
        else if (!activateThreadEvent[tid].scheduled())
            schedule(activateThreadEvent[tid],
                nextCycle(curTick + ticks(delay)));
    }

    /** Unschedule actiavte thread event, regardless of its current state. */
    void unscheduleActivateThreadEvent(int tid)
    {
        if (activateThreadEvent[tid].scheduled())
            activateThreadEvent[tid].squash();
    }

#if !FULL_SYSTEM
    TheISA::IntReg getSyscallArg(int i, int tid);

    /** Used to shift args for indirect syscall. */
    void setSyscallArg(int i, TheISA::IntReg val, int tid);
#endif

    /** The tick event used for scheduling CPU ticks. */
    ActivateThreadEvent activateThreadEvent[Impl::MaxThreads];

    class DeallocateContextEvent : public Event
    {
      private:
        /** Number of Thread to deactivate */
        int tid;

        /** Should the thread be removed from the CPU? */
        bool remove;

        /** Pointer to the CPU. */
        FullO3CPU<Impl> *cpu;

      public:
        /** Constructs the event. */
        DeallocateContextEvent();

        /** Initialize Event */
        void init(int thread_num, FullO3CPU<Impl> *thread_cpu);

        /** Processes the event, calling activateThread() on the CPU. */
        void process();

        /** Sets whether the thread should also be removed from the CPU. */
        void setRemove(bool _remove) { remove = _remove; }

        /** Returns the description of the event. */
        const char *description() const;
    };

    /** Schedule cpu to deallocate thread context.*/
    void scheduleDeallocateContextEvent(int tid, bool remove, int delay)
    {
        // Schedule thread to activate, regardless of its current state.
        if (deallocateContextEvent[tid].squashed())
            reschedule(deallocateContextEvent[tid],
                nextCycle(curTick + ticks(delay)));
        else if (!deallocateContextEvent[tid].scheduled())
            schedule(deallocateContextEvent[tid],
                nextCycle(curTick + ticks(delay)));
    }

    /** Unschedule thread deallocation in CPU */
    void unscheduleDeallocateContextEvent(int tid)
    {
        if (deallocateContextEvent[tid].scheduled())
            deallocateContextEvent[tid].squash();
    }

    /** The tick event used for scheduling CPU ticks. */
    DeallocateContextEvent deallocateContextEvent[Impl::MaxThreads];

  public:
    /** Constructs a CPU with the given parameters. */
    FullO3CPU(DerivO3CPUParams *params);
    /** Destructor. */
    ~FullO3CPU();

    /** Registers statistics. */
    void regStats();

    void demapPage(Addr vaddr, uint64_t asn)
    {
        this->itb->demapPage(vaddr, asn);
        this->dtb->demapPage(vaddr, asn);
    }

    void demapInstPage(Addr vaddr, uint64_t asn)
    {
        this->itb->demapPage(vaddr, asn);
    }

    void demapDataPage(Addr vaddr, uint64_t asn)
    {
        this->dtb->demapPage(vaddr, asn);
    }

    /** Returns a specific port. */
    Port *getPort(const std::string &if_name, int idx);

    /** Ticks CPU, calling tick() on each stage, and checking the overall
     *  activity to see if the CPU should deschedule itself.
     */
    void tick();

    /** Initialize the CPU */
    void init();

    /** Returns the Number of Active Threads in the CPU */
    int numActiveThreads()
    { return activeThreads.size(); }

    /** Add Thread to Active Threads List */
    void activateThread(unsigned tid);

    /** Remove Thread from Active Threads List */
    void deactivateThread(unsigned tid);

    /** Setup CPU to insert a thread's context */
    void insertThread(unsigned tid);

    /** Remove all of a thread's context from CPU */
    void removeThread(unsigned tid);

    /** Count the Total Instructions Committed in the CPU. */
    virtual Counter totalInstructions() const
    {
        Counter total(0);

        for (int i=0; i < thread.size(); i++)
            total += thread[i]->numInst;

        return total;
    }

    /** Add Thread to Active Threads List. */
    void activateContext(int tid, int delay);

    /** Remove Thread from Active Threads List */
    void suspendContext(int tid);

    /** Remove Thread from Active Threads List &&
     *  Possibly Remove Thread Context from CPU.
     */
    bool deallocateContext(int tid, bool remove, int delay = 1);

    /** Remove Thread from Active Threads List &&
     *  Remove Thread Context from CPU.
     */
    void haltContext(int tid);

    /** Activate a Thread When CPU Resources are Available. */
    void activateWhenReady(int tid);

    /** Add or Remove a Thread Context in the CPU. */
    void doContextSwitch();

    /** Update The Order In Which We Process Threads. */
    void updateThreadPriority();

    /** Serialize state. */
    virtual void serialize(std::ostream &os);

    /** Unserialize from a checkpoint. */
    virtual void unserialize(Checkpoint *cp, const std::string &section);

  public:
#if !FULL_SYSTEM
    /** Executes a syscall.
     * @todo: Determine if this needs to be virtual.
     */
    void syscall(int64_t callnum, int tid);

    /** Sets the return value of a syscall. */
    void setSyscallReturn(SyscallReturn return_value, int tid);

#endif

    /** Starts draining the CPU's pipeline of all instructions in
     * order to stop all memory accesses. */
    virtual unsigned int drain(Event *drain_event);

    /** Resumes execution after a drain. */
    virtual void resume();

    /** Signals to this CPU that a stage has completed switching out. */
    void signalDrained();

    /** Switches out this CPU. */
    virtual void switchOut();

    /** Takes over from another CPU. */
    virtual void takeOverFrom(BaseCPU *oldCPU);

    /** Get the current instruction sequence number, and increment it. */
    InstSeqNum getAndIncrementInstSeq()
    { return globalSeqNum++; }

    /** Traps to handle given fault. */
    void trap(Fault fault, unsigned tid);

#if FULL_SYSTEM
    /** HW return from error interrupt. */
    Fault hwrei(unsigned tid);

    bool simPalCheck(int palFunc, unsigned tid);

    /** Returns the Fault for any valid interrupt. */
    Fault getInterrupts();

    /** Processes any an interrupt fault. */
    void processInterrupts(Fault interrupt);

    /** Halts the CPU. */
    void halt() { panic("Halt not implemented!\n"); }

    /** Update the Virt and Phys ports of all ThreadContexts to
     * reflect change in memory connections. */
    void updateMemPorts();

    /** Check if this address is a valid instruction address. */
    bool validInstAddr(Addr addr) { return true; }

    /** Check if this address is a valid data address. */
    bool validDataAddr(Addr addr) { return true; }

    /** Get instruction asid. */
    int getInstAsid(unsigned tid)
    { return regFile.miscRegs[tid].getInstAsid(); }

    /** Get data asid. */
    int getDataAsid(unsigned tid)
    { return regFile.miscRegs[tid].getDataAsid(); }
#else
    /** Get instruction asid. */
    int getInstAsid(unsigned tid)
    { return thread[tid]->getInstAsid(); }

    /** Get data asid. */
    int getDataAsid(unsigned tid)
    { return thread[tid]->getDataAsid(); }

#endif

    /** Register accessors.  Index refers to the physical register index. */

    /** Reads a miscellaneous register. */
    TheISA::MiscReg readMiscRegNoEffect(int misc_reg, unsigned tid);

    /** Reads a misc. register, including any side effects the read
     * might have as defined by the architecture.
     */
    TheISA::MiscReg readMiscReg(int misc_reg, unsigned tid);

    /** Sets a miscellaneous register. */
    void setMiscRegNoEffect(int misc_reg, const TheISA::MiscReg &val, unsigned tid);

    /** Sets a misc. register, including any side effects the write
     * might have as defined by the architecture.
     */
    void setMiscReg(int misc_reg, const TheISA::MiscReg &val,
            unsigned tid);

    uint64_t readIntReg(int reg_idx);

    TheISA::FloatReg readFloatReg(int reg_idx);

    TheISA::FloatReg readFloatReg(int reg_idx, int width);

    TheISA::FloatRegBits readFloatRegBits(int reg_idx);

    TheISA::FloatRegBits readFloatRegBits(int reg_idx, int width);

    void setIntReg(int reg_idx, uint64_t val);

    void setFloatReg(int reg_idx, TheISA::FloatReg val);

    void setFloatReg(int reg_idx, TheISA::FloatReg val, int width);

    void setFloatRegBits(int reg_idx, TheISA::FloatRegBits val);

    void setFloatRegBits(int reg_idx, TheISA::FloatRegBits val, int width);

    uint64_t readArchIntReg(int reg_idx, unsigned tid);

    float readArchFloatRegSingle(int reg_idx, unsigned tid);

    double readArchFloatRegDouble(int reg_idx, unsigned tid);

    uint64_t readArchFloatRegInt(int reg_idx, unsigned tid);

    /** Architectural register accessors.  Looks up in the commit
     * rename table to obtain the true physical index of the
     * architected register first, then accesses that physical
     * register.
     */
    void setArchIntReg(int reg_idx, uint64_t val, unsigned tid);

    void setArchFloatRegSingle(int reg_idx, float val, unsigned tid);

    void setArchFloatRegDouble(int reg_idx, double val, unsigned tid);

    void setArchFloatRegInt(int reg_idx, uint64_t val, unsigned tid);

    /** Reads the commit PC of a specific thread. */
    Addr readPC(unsigned tid);

    /** Sets the commit PC of a specific thread. */
    void setPC(Addr new_PC, unsigned tid);

    /** Reads the commit micro PC of a specific thread. */
    Addr readMicroPC(unsigned tid);

    /** Sets the commmit micro PC of a specific thread. */
    void setMicroPC(Addr new_microPC, unsigned tid);

    /** Reads the next PC of a specific thread. */
    Addr readNextPC(unsigned tid);

    /** Sets the next PC of a specific thread. */
    void setNextPC(Addr val, unsigned tid);

    /** Reads the next NPC of a specific thread. */
    Addr readNextNPC(unsigned tid);

    /** Sets the next NPC of a specific thread. */
    void setNextNPC(Addr val, unsigned tid);

    /** Reads the commit next micro PC of a specific thread. */
    Addr readNextMicroPC(unsigned tid);

    /** Sets the commit next micro PC of a specific thread. */
    void setNextMicroPC(Addr val, unsigned tid);

    /** Initiates a squash of all in-flight instructions for a given
     * thread.  The source of the squash is an external update of
     * state through the TC.
     */
    void squashFromTC(unsigned tid);

    /** Function to add instruction onto the head of the list of the
     *  instructions.  Used when new instructions are fetched.
     */
    ListIt addInst(DynInstPtr &inst);

    /** Function to tell the CPU that an instruction has completed. */
    void instDone(unsigned tid);

    /** Add Instructions to the CPU Remove List*/
    void addToRemoveList(DynInstPtr &inst);

    /** Remove an instruction from the front end of the list.  There's
     *  no restriction on location of the instruction.
     */
    void removeFrontInst(DynInstPtr &inst);

    /** Remove all instructions that are not currently in the ROB.
     *  There's also an option to not squash delay slot instructions.*/
    void removeInstsNotInROB(unsigned tid);

    /** Remove all instructions younger than the given sequence number. */
    void removeInstsUntil(const InstSeqNum &seq_num,unsigned tid);

    /** Removes the instruction pointed to by the iterator. */
    inline void squashInstIt(const ListIt &instIt, const unsigned &tid);

    /** Cleans up all instructions on the remove list. */
    void cleanUpRemovedInsts();

    /** Debug function to print all instructions on the list. */
    void dumpInsts();

  public:
#ifndef NDEBUG
    /** Count of total number of dynamic instructions in flight. */
    int instcount;
#endif

    /** List of all the instructions in flight. */
    std::list<DynInstPtr> instList;

    /** List of all the instructions that will be removed at the end of this
     *  cycle.
     */
    std::queue<ListIt> removeList;

#ifdef DEBUG
    /** Debug structure to keep track of the sequence numbers still in
     * flight.
     */
    std::set<InstSeqNum> snList;
#endif

    /** Records if instructions need to be removed this cycle due to
     *  being retired or squashed.
     */
    bool removeInstsThisCycle;

  protected:
    /** The fetch stage. */
    typename CPUPolicy::Fetch fetch;

    /** The decode stage. */
    typename CPUPolicy::Decode decode;

    /** The dispatch stage. */
    typename CPUPolicy::Rename rename;

    /** The issue/execute/writeback stages. */
    typename CPUPolicy::IEW iew;

    /** The commit stage. */
    typename CPUPolicy::Commit commit;

    /** The register file. */
    typename CPUPolicy::RegFile regFile;

    /** The free list. */
    typename CPUPolicy::FreeList freeList;

    /** The rename map. */
    typename CPUPolicy::RenameMap renameMap[Impl::MaxThreads];

    /** The commit rename map. */
    typename CPUPolicy::RenameMap commitRenameMap[Impl::MaxThreads];

    /** The re-order buffer. */
    typename CPUPolicy::ROB rob;

    /** Active Threads List */
    std::list<unsigned> activeThreads;

    /** Integer Register Scoreboard */
    Scoreboard scoreboard;

  public:
    /** Enum to give each stage a specific index, so when calling
     *  activateStage() or deactivateStage(), they can specify which stage
     *  is being activated/deactivated.
     */
    enum StageIdx {
        FetchIdx,
        DecodeIdx,
        RenameIdx,
        IEWIdx,
        CommitIdx,
        NumStages };

    /** Typedefs from the Impl to get the structs that each of the
     *  time buffers should use.
     */
    typedef typename CPUPolicy::TimeStruct TimeStruct;

    typedef typename CPUPolicy::FetchStruct FetchStruct;

    typedef typename CPUPolicy::DecodeStruct DecodeStruct;

    typedef typename CPUPolicy::RenameStruct RenameStruct;

    typedef typename CPUPolicy::IEWStruct IEWStruct;

    /** The main time buffer to do backwards communication. */
    TimeBuffer<TimeStruct> timeBuffer;

    /** The fetch stage's instruction queue. */
    TimeBuffer<FetchStruct> fetchQueue;

    /** The decode stage's instruction queue. */
    TimeBuffer<DecodeStruct> decodeQueue;

    /** The rename stage's instruction queue. */
    TimeBuffer<RenameStruct> renameQueue;

    /** The IEW stage's instruction queue. */
    TimeBuffer<IEWStruct> iewQueue;

  private:
    /** The activity recorder; used to tell if the CPU has any
     * activity remaining or if it can go to idle and deschedule
     * itself.
     */
    ActivityRecorder activityRec;

  public:
    /** Records that there was time buffer activity this cycle. */
    void activityThisCycle() { activityRec.activity(); }

    /** Changes a stage's status to active within the activity recorder. */
    void activateStage(const StageIdx idx)
    { activityRec.activateStage(idx); }

    /** Changes a stage's status to inactive within the activity recorder. */
    void deactivateStage(const StageIdx idx)
    { activityRec.deactivateStage(idx); }

    /** Wakes the CPU, rescheduling the CPU if it's not already active. */
    void wakeCPU();

#if FULL_SYSTEM
    virtual void wakeup();
#endif

    /** Gets a free thread id. Use if thread ids change across system. */
    int getFreeTid();

  public:
    /** Returns a pointer to a thread context. */
    ThreadContext *tcBase(unsigned tid)
    {
        return thread[tid]->getTC();
    }

    /** The global sequence number counter. */
    InstSeqNum globalSeqNum;//[Impl::MaxThreads];

#if USE_CHECKER
    /** Pointer to the checker, which can dynamically verify
     * instruction results at run time.  This can be set to NULL if it
     * is not being used.
     */
    Checker<DynInstPtr> *checker;
#endif

#if FULL_SYSTEM
    /** Pointer to the system. */
    System *system;

    /** Pointer to physical memory. */
    PhysicalMemory *physmem;
#endif

    /** Event to call process() on once draining has completed. */
    Event *drainEvent;

    /** Counter of how many stages have completed draining. */
    int drainCount;

    /** Pointers to all of the threads in the CPU. */
    std::vector<Thread *> thread;

    /** Whether or not the CPU should defer its registration. */
    bool deferRegistration;

    /** Is there a context switch pending? */
    bool contextSwitch;

    /** Threads Scheduled to Enter CPU */
    std::list<int> cpuWaitList;

    /** The cycle that the CPU was last running, used for statistics. */
    Tick lastRunningCycle;

    /** The cycle that the CPU was last activated by a new thread*/
    Tick lastActivatedCycle;

    /** Number of Threads CPU can process */
    unsigned numThreads;

    /** Mapping for system thread id to cpu id */
    std::map<unsigned,unsigned> threadMap;

    /** Available thread ids in the cpu*/
    std::vector<unsigned> tids;

    /** CPU read function, forwards read to LSQ. */
    template <class T>
    Fault read(RequestPtr &req, T &data, int load_idx)
    {
        return this->iew.ldstQueue.read(req, data, load_idx);
    }

    /** CPU write function, forwards write to LSQ. */
    template <class T>
    Fault write(RequestPtr &req, T &data, int store_idx)
    {
        return this->iew.ldstQueue.write(req, data, store_idx);
    }

    Addr lockAddr;

    /** Temporary fix for the lock flag, works in the UP case. */
    bool lockFlag;

    /** Stat for total number of times the CPU is descheduled. */
    Stats::Scalar<> timesIdled;
    /** Stat for total number of cycles the CPU spends descheduled. */
    Stats::Scalar<> idleCycles;
    /** Stat for the number of committed instructions per thread. */
    Stats::Vector<> committedInsts;
    /** Stat for the total number of committed instructions. */
    Stats::Scalar<> totalCommittedInsts;
    /** Stat for the CPI per thread. */
    Stats::Formula cpi;
    /** Stat for the total CPI. */
    Stats::Formula totalCpi;
    /** Stat for the IPC per thread. */
    Stats::Formula ipc;
    /** Stat for the total IPC. */
    Stats::Formula totalIpc;
};

#endif // __CPU_O3_CPU_HH__
