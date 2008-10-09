/*
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
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
 */

#include "arch/locked_mem.hh"
#include "arch/mmaped_ipr.hh"
#include "arch/utility.hh"
#include "base/bigint.hh"
#include "cpu/exetrace.hh"
#include "cpu/simple/timing.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "params/TimingSimpleCPU.hh"
#include "sim/system.hh"

using namespace std;
using namespace TheISA;

Port *
TimingSimpleCPU::getPort(const std::string &if_name, int idx)
{
    if (if_name == "dcache_port")
        return &dcachePort;
    else if (if_name == "icache_port")
        return &icachePort;
    else
        panic("No Such Port\n");
}

void
TimingSimpleCPU::init()
{
    BaseCPU::init();
    cpuId = tc->readCpuId();
#if FULL_SYSTEM
    for (int i = 0; i < threadContexts.size(); ++i) {
        ThreadContext *tc = threadContexts[i];

        // initialize CPU, including PC
        TheISA::initCPU(tc, cpuId);
    }
#endif
}

Tick
TimingSimpleCPU::CpuPort::recvAtomic(PacketPtr pkt)
{
    panic("TimingSimpleCPU doesn't expect recvAtomic callback!");
    return curTick;
}

void
TimingSimpleCPU::CpuPort::recvFunctional(PacketPtr pkt)
{
    //No internal storage to update, jusst return
    return;
}

void
TimingSimpleCPU::CpuPort::recvStatusChange(Status status)
{
    if (status == RangeChange) {
        if (!snoopRangeSent) {
            snoopRangeSent = true;
            sendStatusChange(Port::RangeChange);
        }
        return;
    }

    panic("TimingSimpleCPU doesn't expect recvStatusChange callback!");
}


void
TimingSimpleCPU::CpuPort::TickEvent::schedule(PacketPtr _pkt, Tick t)
{
    pkt = _pkt;
    cpu->schedule(this, t);
}

TimingSimpleCPU::TimingSimpleCPU(TimingSimpleCPUParams *p)
    : BaseSimpleCPU(p), icachePort(this, p->clock), dcachePort(this, p->clock)
{
    _status = Idle;

    icachePort.snoopRangeSent = false;
    dcachePort.snoopRangeSent = false;

    ifetch_pkt = dcache_pkt = NULL;
    drainEvent = NULL;
    fetchEvent = NULL;
    previousTick = 0;
    changeState(SimObject::Running);
}


TimingSimpleCPU::~TimingSimpleCPU()
{
}

void
TimingSimpleCPU::serialize(ostream &os)
{
    SimObject::State so_state = SimObject::getState();
    SERIALIZE_ENUM(so_state);
    BaseSimpleCPU::serialize(os);
}

void
TimingSimpleCPU::unserialize(Checkpoint *cp, const string &section)
{
    SimObject::State so_state;
    UNSERIALIZE_ENUM(so_state);
    BaseSimpleCPU::unserialize(cp, section);
}

unsigned int
TimingSimpleCPU::drain(Event *drain_event)
{
    // TimingSimpleCPU is ready to drain if it's not waiting for
    // an access to complete.
    if (_status == Idle || _status == Running || _status == SwitchedOut) {
        changeState(SimObject::Drained);
        return 0;
    } else {
        changeState(SimObject::Draining);
        drainEvent = drain_event;
        return 1;
    }
}

void
TimingSimpleCPU::resume()
{
    DPRINTF(SimpleCPU, "Resume\n");
    if (_status != SwitchedOut && _status != Idle) {
        assert(system->getMemoryMode() == Enums::timing);

        // Delete the old event if it existed.
        if (fetchEvent) {
            if (fetchEvent->scheduled())
                deschedule(fetchEvent);

            delete fetchEvent;
        }

        fetchEvent = new FetchEvent(this, nextCycle());
    }

    changeState(SimObject::Running);
}

void
TimingSimpleCPU::switchOut()
{
    assert(_status == Running || _status == Idle);
    _status = SwitchedOut;
    numCycles += tickToCycles(curTick - previousTick);

    // If we've been scheduled to resume but are then told to switch out,
    // we'll need to cancel it.
    if (fetchEvent && fetchEvent->scheduled())
        deschedule(fetchEvent);
}


void
TimingSimpleCPU::takeOverFrom(BaseCPU *oldCPU)
{
    BaseCPU::takeOverFrom(oldCPU, &icachePort, &dcachePort);

    // if any of this CPU's ThreadContexts are active, mark the CPU as
    // running and schedule its tick event.
    for (int i = 0; i < threadContexts.size(); ++i) {
        ThreadContext *tc = threadContexts[i];
        if (tc->status() == ThreadContext::Active && _status != Running) {
            _status = Running;
            break;
        }
    }

    if (_status != Running) {
        _status = Idle;
    }
    assert(threadContexts.size() == 1);
    cpuId = tc->readCpuId();
    previousTick = curTick;
}


void
TimingSimpleCPU::activateContext(int thread_num, int delay)
{
    DPRINTF(SimpleCPU, "ActivateContext %d (%d cycles)\n", thread_num, delay);

    assert(thread_num == 0);
    assert(thread);

    assert(_status == Idle);

    notIdleFraction++;
    _status = Running;

    // kick things off by initiating the fetch of the next instruction
    fetchEvent = new FetchEvent(this);
    schedule(fetchEvent, nextCycle(curTick + ticks(delay)));
}


void
TimingSimpleCPU::suspendContext(int thread_num)
{
    DPRINTF(SimpleCPU, "SuspendContext %d\n", thread_num);

    assert(thread_num == 0);
    assert(thread);

    assert(_status == Running);

    // just change status to Idle... if status != Running,
    // completeInst() will not initiate fetch of next instruction.

    notIdleFraction--;
    _status = Idle;
}


template <class T>
Fault
TimingSimpleCPU::read(Addr addr, T &data, unsigned flags)
{
    Request *req =
        new Request(/* asid */ 0, addr, sizeof(T), flags, thread->readPC(),
                    cpuId, /* thread ID */ 0);

    if (traceData) {
        traceData->setAddr(req->getVaddr());
    }

   // translate to physical address
    Fault fault = thread->translateDataReadReq(req);

    // Now do the access.
    if (fault == NoFault) {
        PacketPtr pkt =
            new Packet(req,
                       (req->isLocked() ?
                        MemCmd::LoadLockedReq : MemCmd::ReadReq),
                       Packet::Broadcast);
        pkt->dataDynamic<T>(new T);

        if (req->isMmapedIpr()) {
            Tick delay;
            delay = TheISA::handleIprRead(thread->getTC(), pkt);
            new IprEvent(pkt, this, nextCycle(curTick + delay));
            _status = DcacheWaitResponse;
            dcache_pkt = NULL;
        } else if (!dcachePort.sendTiming(pkt)) {
            _status = DcacheRetry;
            dcache_pkt = pkt;
        } else {
            _status = DcacheWaitResponse;
            // memory system takes ownership of packet
            dcache_pkt = NULL;
        }

        // This will need a new way to tell if it has a dcache attached.
        if (req->isUncacheable())
            recordEvent("Uncached Read");
    } else {
        delete req;
    }

    if (traceData) {
        traceData->setData(data);
    }
    return fault;
}

Fault
TimingSimpleCPU::translateDataReadAddr(Addr vaddr, Addr &paddr,
        int size, unsigned flags)
{
    Request *req =
        new Request(0, vaddr, size, flags, thread->readPC(), cpuId, 0);

    if (traceData) {
        traceData->setAddr(vaddr);
    }

    Fault fault = thread->translateDataWriteReq(req);

    if (fault == NoFault)
        paddr = req->getPaddr();

    delete req;
    return fault;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

template
Fault
TimingSimpleCPU::read(Addr addr, Twin64_t &data, unsigned flags);

template
Fault
TimingSimpleCPU::read(Addr addr, Twin32_t &data, unsigned flags);

template
Fault
TimingSimpleCPU::read(Addr addr, uint64_t &data, unsigned flags);

template
Fault
TimingSimpleCPU::read(Addr addr, uint32_t &data, unsigned flags);

template
Fault
TimingSimpleCPU::read(Addr addr, uint16_t &data, unsigned flags);

template
Fault
TimingSimpleCPU::read(Addr addr, uint8_t &data, unsigned flags);

#endif //DOXYGEN_SHOULD_SKIP_THIS

template<>
Fault
TimingSimpleCPU::read(Addr addr, double &data, unsigned flags)
{
    return read(addr, *(uint64_t*)&data, flags);
}

template<>
Fault
TimingSimpleCPU::read(Addr addr, float &data, unsigned flags)
{
    return read(addr, *(uint32_t*)&data, flags);
}


template<>
Fault
TimingSimpleCPU::read(Addr addr, int32_t &data, unsigned flags)
{
    return read(addr, (uint32_t&)data, flags);
}


template <class T>
Fault
TimingSimpleCPU::write(T data, Addr addr, unsigned flags, uint64_t *res)
{
    Request *req =
        new Request(/* asid */ 0, addr, sizeof(T), flags, thread->readPC(),
                    cpuId, /* thread ID */ 0);

    if (traceData) {
        traceData->setAddr(req->getVaddr());
    }

    // translate to physical address
    Fault fault = thread->translateDataWriteReq(req);

    // Now do the access.
    if (fault == NoFault) {
        MemCmd cmd = MemCmd::WriteReq; // default
        bool do_access = true;  // flag to suppress cache access

        if (req->isLocked()) {
            cmd = MemCmd::StoreCondReq;
            do_access = TheISA::handleLockedWrite(thread, req);
        } else if (req->isSwap()) {
            cmd = MemCmd::SwapReq;
            if (req->isCondSwap()) {
                assert(res);
                req->setExtraData(*res);
            }
        }

        // Note: need to allocate dcache_pkt even if do_access is
        // false, as it's used unconditionally to call completeAcc().
        assert(dcache_pkt == NULL);
        dcache_pkt = new Packet(req, cmd, Packet::Broadcast);
        dcache_pkt->allocate();
        dcache_pkt->set(data);

        if (do_access) {
            if (req->isMmapedIpr()) {
                Tick delay;
                dcache_pkt->set(htog(data));
                delay = TheISA::handleIprWrite(thread->getTC(), dcache_pkt);
                new IprEvent(dcache_pkt, this, nextCycle(curTick + delay));
                _status = DcacheWaitResponse;
                dcache_pkt = NULL;
            } else if (!dcachePort.sendTiming(dcache_pkt)) {
                _status = DcacheRetry;
            } else {
                _status = DcacheWaitResponse;
                // memory system takes ownership of packet
                dcache_pkt = NULL;
            }
        }
        // This will need a new way to tell if it's hooked up to a cache or not.
        if (req->isUncacheable())
            recordEvent("Uncached Write");
    } else {
        delete req;
    }

    if (traceData) {
        traceData->setData(data);
    }

    // If the write needs to have a fault on the access, consider calling
    // changeStatus() and changing it to "bad addr write" or something.
    return fault;
}

Fault
TimingSimpleCPU::translateDataWriteAddr(Addr vaddr, Addr &paddr,
        int size, unsigned flags)
{
    Request *req =
        new Request(0, vaddr, size, flags, thread->readPC(), cpuId, 0);

    if (traceData) {
        traceData->setAddr(vaddr);
    }

    Fault fault = thread->translateDataWriteReq(req);

    if (fault == NoFault)
        paddr = req->getPaddr();

    delete req;
    return fault;
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS
template
Fault
TimingSimpleCPU::write(Twin32_t data, Addr addr,
                       unsigned flags, uint64_t *res);

template
Fault
TimingSimpleCPU::write(Twin64_t data, Addr addr,
                       unsigned flags, uint64_t *res);

template
Fault
TimingSimpleCPU::write(uint64_t data, Addr addr,
                       unsigned flags, uint64_t *res);

template
Fault
TimingSimpleCPU::write(uint32_t data, Addr addr,
                       unsigned flags, uint64_t *res);

template
Fault
TimingSimpleCPU::write(uint16_t data, Addr addr,
                       unsigned flags, uint64_t *res);

template
Fault
TimingSimpleCPU::write(uint8_t data, Addr addr,
                       unsigned flags, uint64_t *res);

#endif //DOXYGEN_SHOULD_SKIP_THIS

template<>
Fault
TimingSimpleCPU::write(double data, Addr addr, unsigned flags, uint64_t *res)
{
    return write(*(uint64_t*)&data, addr, flags, res);
}

template<>
Fault
TimingSimpleCPU::write(float data, Addr addr, unsigned flags, uint64_t *res)
{
    return write(*(uint32_t*)&data, addr, flags, res);
}


template<>
Fault
TimingSimpleCPU::write(int32_t data, Addr addr, unsigned flags, uint64_t *res)
{
    return write((uint32_t)data, addr, flags, res);
}


void
TimingSimpleCPU::fetch()
{
    DPRINTF(SimpleCPU, "Fetch\n");

    if (!curStaticInst || !curStaticInst->isDelayedCommit())
        checkForInterrupts();

    checkPcEventQueue();

    Request *ifetch_req = new Request();
    ifetch_req->setThreadContext(cpuId, /* thread ID */ 0);
    Fault fault = setupFetchRequest(ifetch_req);

    ifetch_pkt = new Packet(ifetch_req, MemCmd::ReadReq, Packet::Broadcast);
    ifetch_pkt->dataStatic(&inst);

    if (fault == NoFault) {
        if (!icachePort.sendTiming(ifetch_pkt)) {
            // Need to wait for retry
            _status = IcacheRetry;
        } else {
            // Need to wait for cache to respond
            _status = IcacheWaitResponse;
            // ownership of packet transferred to memory system
            ifetch_pkt = NULL;
        }
    } else {
        delete ifetch_req;
        delete ifetch_pkt;
        // fetch fault: advance directly to next instruction (fault handler)
        advanceInst(fault);
    }

    numCycles += tickToCycles(curTick - previousTick);
    previousTick = curTick;
}


void
TimingSimpleCPU::advanceInst(Fault fault)
{
    advancePC(fault);

    if (_status == Running) {
        // kick off fetch of next instruction... callback from icache
        // response will cause that instruction to be executed,
        // keeping the CPU running.
        fetch();
    }
}


void
TimingSimpleCPU::completeIfetch(PacketPtr pkt)
{
    DPRINTF(SimpleCPU, "Complete ICache Fetch\n");

    // received a response from the icache: execute the received
    // instruction
    assert(!pkt->isError());
    assert(_status == IcacheWaitResponse);

    _status = Running;

    numCycles += tickToCycles(curTick - previousTick);
    previousTick = curTick;

    if (getState() == SimObject::Draining) {
        delete pkt->req;
        delete pkt;

        completeDrain();
        return;
    }

    preExecute();
    if (curStaticInst->isMemRef() && !curStaticInst->isDataPrefetch()) {
        // load or store: just send to dcache
        Fault fault = curStaticInst->initiateAcc(this, traceData);
        if (_status != Running) {
            // instruction will complete in dcache response callback
            assert(_status == DcacheWaitResponse || _status == DcacheRetry);
            assert(fault == NoFault);
        } else {
            if (fault == NoFault) {
                // Note that ARM can have NULL packets if the instruction gets
                // squashed due to predication
                // early fail on store conditional: complete now
                assert(dcache_pkt != NULL || THE_ISA == ARM_ISA);

                fault = curStaticInst->completeAcc(dcache_pkt, this,
                                                   traceData);
                if (dcache_pkt != NULL)
                {
                    delete dcache_pkt->req;
                    delete dcache_pkt;
                    dcache_pkt = NULL;
                }

                // keep an instruction count
                if (fault == NoFault)
                    countInst();
            } else if (traceData) {
                // If there was a fault, we shouldn't trace this instruction.
                delete traceData;
                traceData = NULL;
            }

            postExecute();
            // @todo remove me after debugging with legion done
            if (curStaticInst && (!curStaticInst->isMicroop() ||
                        curStaticInst->isFirstMicroop()))
                instCnt++;
            advanceInst(fault);
        }
    } else {
        // non-memory instruction: execute completely now
        Fault fault = curStaticInst->execute(this, traceData);

        // keep an instruction count
        if (fault == NoFault)
            countInst();
        else if (traceData) {
            // If there was a fault, we shouldn't trace this instruction.
            delete traceData;
            traceData = NULL;
        }

        postExecute();
        // @todo remove me after debugging with legion done
        if (curStaticInst && (!curStaticInst->isMicroop() ||
                    curStaticInst->isFirstMicroop()))
            instCnt++;
        advanceInst(fault);
    }

    delete pkt->req;
    delete pkt;
}

void
TimingSimpleCPU::IcachePort::ITickEvent::process()
{
    cpu->completeIfetch(pkt);
}

bool
TimingSimpleCPU::IcachePort::recvTiming(PacketPtr pkt)
{
    if (pkt->isResponse() && !pkt->wasNacked()) {
        // delay processing of returned data until next CPU clock edge
        Tick next_tick = cpu->nextCycle(curTick);

        if (next_tick == curTick)
            cpu->completeIfetch(pkt);
        else
            tickEvent.schedule(pkt, next_tick);

        return true;
    }
    else if (pkt->wasNacked()) {
        assert(cpu->_status == IcacheWaitResponse);
        pkt->reinitNacked();
        if (!sendTiming(pkt)) {
            cpu->_status = IcacheRetry;
            cpu->ifetch_pkt = pkt;
        }
    }
    //Snooping a Coherence Request, do nothing
    return true;
}

void
TimingSimpleCPU::IcachePort::recvRetry()
{
    // we shouldn't get a retry unless we have a packet that we're
    // waiting to transmit
    assert(cpu->ifetch_pkt != NULL);
    assert(cpu->_status == IcacheRetry);
    PacketPtr tmp = cpu->ifetch_pkt;
    if (sendTiming(tmp)) {
        cpu->_status = IcacheWaitResponse;
        cpu->ifetch_pkt = NULL;
    }
}

void
TimingSimpleCPU::completeDataAccess(PacketPtr pkt)
{
    // received a response from the dcache: complete the load or store
    // instruction
    assert(!pkt->isError());
    assert(_status == DcacheWaitResponse);
    _status = Running;

    numCycles += tickToCycles(curTick - previousTick);
    previousTick = curTick;

    Fault fault = curStaticInst->completeAcc(pkt, this, traceData);

    // keep an instruction count
    if (fault == NoFault)
        countInst();
    else if (traceData) {
        // If there was a fault, we shouldn't trace this instruction.
        delete traceData;
        traceData = NULL;
    }

    // the locked flag may be cleared on the response packet, so check
    // pkt->req and not pkt to see if it was a load-locked
    if (pkt->isRead() && pkt->req->isLocked()) {
        TheISA::handleLockedRead(thread, pkt->req);
    }

    delete pkt->req;
    delete pkt;

    postExecute();

    if (getState() == SimObject::Draining) {
        advancePC(fault);
        completeDrain();

        return;
    }

    advanceInst(fault);
}


void
TimingSimpleCPU::completeDrain()
{
    DPRINTF(Config, "Done draining\n");
    changeState(SimObject::Drained);
    drainEvent->process();
}

void
TimingSimpleCPU::DcachePort::setPeer(Port *port)
{
    Port::setPeer(port);

#if FULL_SYSTEM
    // Update the ThreadContext's memory ports (Functional/Virtual
    // Ports)
    cpu->tcBase()->connectMemPorts(cpu->tcBase());
#endif
}

bool
TimingSimpleCPU::DcachePort::recvTiming(PacketPtr pkt)
{
    if (pkt->isResponse() && !pkt->wasNacked()) {
        // delay processing of returned data until next CPU clock edge
        Tick next_tick = cpu->nextCycle(curTick);

        if (next_tick == curTick)
            cpu->completeDataAccess(pkt);
        else
            tickEvent.schedule(pkt, next_tick);

        return true;
    }
    else if (pkt->wasNacked()) {
        assert(cpu->_status == DcacheWaitResponse);
        pkt->reinitNacked();
        if (!sendTiming(pkt)) {
            cpu->_status = DcacheRetry;
            cpu->dcache_pkt = pkt;
        }
    }
    //Snooping a Coherence Request, do nothing
    return true;
}

void
TimingSimpleCPU::DcachePort::DTickEvent::process()
{
    cpu->completeDataAccess(pkt);
}

void
TimingSimpleCPU::DcachePort::recvRetry()
{
    // we shouldn't get a retry unless we have a packet that we're
    // waiting to transmit
    assert(cpu->dcache_pkt != NULL);
    assert(cpu->_status == DcacheRetry);
    PacketPtr tmp = cpu->dcache_pkt;
    if (sendTiming(tmp)) {
        cpu->_status = DcacheWaitResponse;
        // memory system takes ownership of packet
        cpu->dcache_pkt = NULL;
    }
}

TimingSimpleCPU::IprEvent::IprEvent(Packet *_pkt, TimingSimpleCPU *_cpu,
    Tick t)
    : pkt(_pkt), cpu(_cpu)
{
    cpu->schedule(this, t);
}

void
TimingSimpleCPU::IprEvent::process()
{
    cpu->completeDataAccess(pkt);
}

const char *
TimingSimpleCPU::IprEvent::description() const
{
    return "Timing Simple CPU Delay IPR event";
}


void
TimingSimpleCPU::printAddr(Addr a)
{
    dcachePort.printAddr(a);
}


////////////////////////////////////////////////////////////////////////
//
//  TimingSimpleCPU Simulation Object
//
TimingSimpleCPU *
TimingSimpleCPUParams::create()
{
    numThreads = 1;
#if !FULL_SYSTEM
    if (workload.size() != 1)
        panic("only one workload allowed");
#endif
    return new TimingSimpleCPU(this);
}
