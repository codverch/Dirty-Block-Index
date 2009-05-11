
/*
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
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

/*
 * MemoryControl.C
 *
 * Description:  This module simulates a basic DDR-style memory controller
 * (and can easily be extended to do FB-DIMM as well).
 *
 * This module models a single channel, connected to any number of
 * DIMMs with any number of ranks of DRAMs each.  If you want multiple
 * address/data channels, you need to instantiate multiple copies of
 * this module.
 *
 * Each memory request is placed in a queue associated with a specific
 * memory bank.  This queue is of finite size; if the queue is full
 * the request will back up in an (infinite) common queue and will
 * effectively throttle the whole system.  This sort of behavior is
 * intended to be closer to real system behavior than if we had an
 * infinite queue on each bank.  If you want the latter, just make
 * the bank queues unreasonably large.
 *
 * The head item on a bank queue is issued when all of the
 * following are true:
 *   the bank is available
 *   the address path to the DIMM is available
 *   the data path to or from the DIMM is available
 *
 * Note that we are not concerned about fixed offsets in time.  The bank
 * will not be used at the same moment as the address path, but since
 * there is no queue in the DIMM or the DRAM it will be used at a constant
 * number of cycles later, so it is treated as if it is used at the same
 * time.
 *
 * We are assuming closed bank policy; that is, we automatically close
 * each bank after a single read or write.  Adding an option for open
 * bank policy is for future work.
 *
 * We are assuming "posted CAS"; that is, we send the READ or WRITE
 * immediately after the ACTIVATE.  This makes scheduling the address
 * bus trivial; we always schedule a fixed set of cycles.  For DDR-400,
 * this is a set of two cycles; for some configurations such as
 * DDR-800 the parameter tRRD forces this to be set to three cycles.
 *
 * We assume a four-bit-time transfer on the data wires.  This is
 * the minimum burst length for DDR-2.  This would correspond
 * to (for example) a memory where each DIMM is 72 bits wide
 * and DIMMs are ganged in pairs to deliver 64 bytes at a shot.
 * This gives us the same occupancy on the data wires as on the
 * address wires (for the two-address-cycle case).
 *
 * The only non-trivial scheduling problem is the data wires.
 * A write will use the wires earlier in the operation than a read
 * will; typically one cycle earlier as seen at the DRAM, but earlier
 * by a worst-case round-trip wire delay when seen at the memory controller.
 * So, while reads from one rank can be scheduled back-to-back
 * every two cycles, and writes (to any rank) scheduled every two cycles,
 * when a read is followed by a write we need to insert a bubble.
 * Furthermore, consecutive reads from two different ranks may need
 * to insert a bubble due to skew between when one DRAM stops driving the
 * wires and when the other one starts.  (These bubbles are parameters.)
 *
 * This means that when some number of reads and writes are at the
 * heads of their queues, reads could starve writes, and/or reads
 * to the same rank could starve out other requests, since the others
 * would never see the data bus ready.
 * For this reason, we have implemented an anti-starvation feature.
 * A group of requests is marked "old", and a counter is incremented
 * each cycle as long as any request from that batch has not issued.
 * if the counter reaches twice the bank busy time, we hold off any
 * newer requests until all of the "old" requests have issued.
 *
 * We also model tFAW.  This is an obscure DRAM parameter that says
 * that no more than four activate requests can happen within a window
 * of a certain size.  For most configurations this does not come into play,
 * or has very little effect, but it could be used to throttle the power
 * consumption of the DRAM.  In this implementation (unlike in a DRAM
 * data sheet) TFAW is measured in memory bus cycles; i.e. if TFAW = 16
 * then no more than four activates may happen within any 16 cycle window.
 * Refreshes are included in the activates.
 *
 *
 * $Id: $
 *
 */

#include "mem/ruby/common/Global.hh"
#include "mem/gems_common/Map.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/slicc_interface/AbstractChip.hh"
#include "mem/ruby/system/System.hh"
#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"
#include "mem/ruby/slicc_interface/NetworkMessage.hh"
#include "mem/ruby/network/Network.hh"

#include "mem/ruby/common/Consumer.hh"

#include "mem/ruby/system/MemoryControl.hh"

#include <list>

class Consumer;

// Value to reset watchdog timer to.
// If we're idle for this many memory control cycles,
// shut down our clock (our rescheduling of ourselves).
// Refresh shuts down as well.
// When we restart, we'll be in a different phase
// with respect to ruby cycles, so this introduces
// a slight inaccuracy.  But it is necessary or the
// ruby tester never terminates because the event
// queue is never empty.
#define IDLECOUNT_MAX_VALUE 1000

// Output operator definition

ostream& operator<<(ostream& out, const MemoryControl& obj)
{
  obj.print(out);
  out << flush;
  return out;
}


// ****************************************************************

// CONSTRUCTOR

MemoryControl::MemoryControl (AbstractChip* chip_ptr, int version) {
  m_chip_ptr = chip_ptr;
  m_version = version;
  m_msg_counter = 0;

  m_debug = 0;
  //if (m_version == 0) m_debug = 1;

  m_mem_bus_cycle_multiplier = RubyConfig::memBusCycleMultiplier();
  m_banks_per_rank           = RubyConfig::banksPerRank();
  m_ranks_per_dimm           = RubyConfig::ranksPerDimm();
  m_dimms_per_channel        = RubyConfig::dimmsPerChannel();
  m_bank_bit_0               = RubyConfig::bankBit0();
  m_rank_bit_0               = RubyConfig::rankBit0();
  m_dimm_bit_0               = RubyConfig::dimmBit0();
  m_bank_queue_size          = RubyConfig::bankQueueSize();
  m_bank_busy_time           = RubyConfig::bankBusyTime();
  m_rank_rank_delay          = RubyConfig::rankRankDelay();
  m_read_write_delay         = RubyConfig::readWriteDelay();
  m_basic_bus_busy_time      = RubyConfig::basicBusBusyTime();
  m_mem_ctl_latency          = RubyConfig::memCtlLatency();
  m_refresh_period           = RubyConfig::refreshPeriod();
  m_memRandomArbitrate       = RubyConfig::memRandomArbitrate();
  m_tFaw                     = RubyConfig::tFaw();
  m_memFixedDelay            = RubyConfig::memFixedDelay();

  assert(m_tFaw <= 62); // must fit in a uint64 shift register

  m_total_banks = m_banks_per_rank * m_ranks_per_dimm * m_dimms_per_channel;
  m_total_ranks = m_ranks_per_dimm * m_dimms_per_channel;
  m_refresh_period_system = m_refresh_period / m_total_banks;

  m_bankQueues = new list<MemoryNode> [m_total_banks];
  assert(m_bankQueues);

  m_bankBusyCounter = new int [m_total_banks];
  assert(m_bankBusyCounter);

  m_oldRequest = new int [m_total_banks];
  assert(m_oldRequest);

  for (int i=0; i<m_total_banks; i++) {
    m_bankBusyCounter[i] = 0;
    m_oldRequest[i] = 0;
  }

  m_busBusyCounter_Basic = 0;
  m_busBusyCounter_Write = 0;
  m_busBusyCounter_ReadNewRank = 0;
  m_busBusy_WhichRank = 0;

  m_roundRobin = 0;
  m_refresh_count = 1;
  m_need_refresh = 0;
  m_refresh_bank = 0;
  m_awakened = 0;
  m_idleCount = 0;
  m_ageCounter = 0;

  // Each tfaw shift register keeps a moving bit pattern
  // which shows when recent activates have occurred.
  // m_tfaw_count keeps track of how many 1 bits are set
  // in each shift register.  When m_tfaw_count is >= 4,
  // new activates are not allowed.
  m_tfaw_shift = new uint64 [m_total_ranks];
  m_tfaw_count = new int [m_total_ranks];
  for (int i=0; i<m_total_ranks; i++) {
    m_tfaw_shift[i] = 0;
    m_tfaw_count[i] = 0;
  }
}


// DESTRUCTOR

MemoryControl::~MemoryControl () {
  delete [] m_bankQueues;
  delete [] m_bankBusyCounter;
  delete [] m_oldRequest;
}


// PUBLIC METHODS

// enqueue new request from directory

void MemoryControl::enqueue (const MsgPtr& message, int latency) {
  Time current_time = g_eventQueue_ptr->getTime();
  Time arrival_time = current_time + latency;
  const MemoryMsg* memMess = dynamic_cast<const MemoryMsg*>(message.ref());
  physical_address_t addr = memMess->getAddress().getAddress();
  MemoryRequestType type = memMess->getType();
  bool is_mem_read = (type == MemoryRequestType_MEMORY_READ);
  MemoryNode thisReq(arrival_time, message, addr, is_mem_read, !is_mem_read);
  enqueueMemRef(thisReq);
}

// Alternate entry point used when we already have a MemoryNode structure built.

void MemoryControl::enqueueMemRef (MemoryNode& memRef) {
  m_msg_counter++;
  memRef.m_msg_counter = m_msg_counter;
  Time arrival_time = memRef.m_time;
  uint64 at = arrival_time;
  bool is_mem_read = memRef.m_is_mem_read;
  physical_address_t addr = memRef.m_addr;
  int bank = getBank(addr);
  if (m_debug) {
    printf("New memory request%7d: 0x%08llx %c arrived at %10lld  ", m_msg_counter, addr, is_mem_read? 'R':'W', at);
    printf("bank =%3x\n", bank);
  }
  g_system_ptr->getProfiler()->profileMemReq(bank);
  m_input_queue.push_back(memRef);
  if (!m_awakened) {
    g_eventQueue_ptr->scheduleEvent(this, 1);
    m_awakened = 1;
  }
}



// dequeue, peek, and isReady are used to transfer completed requests
// back to the directory

void MemoryControl::dequeue () {
  assert(isReady());
  m_response_queue.pop_front();
}


const Message* MemoryControl::peek () {
  MemoryNode node = peekNode();
  Message* msg_ptr = node.m_msgptr.ref();
  assert(msg_ptr != NULL);
  return msg_ptr;
}


MemoryNode MemoryControl::peekNode () {
  assert(isReady());
  MemoryNode req = m_response_queue.front();
  uint64 returnTime = req.m_time;
  if (m_debug) {
    printf("Old memory request%7d: 0x%08llx %c peeked at  %10lld\n",
        req.m_msg_counter, req.m_addr, req.m_is_mem_read? 'R':'W', returnTime);
  }
  return req;
}


bool MemoryControl::isReady () {
  return ((!m_response_queue.empty()) &&
          (m_response_queue.front().m_time <= g_eventQueue_ptr->getTime()));
}

void MemoryControl::setConsumer (Consumer* consumer_ptr) {
  m_consumer_ptr = consumer_ptr;
}

void MemoryControl::print (ostream& out) const {
}


void MemoryControl::printConfig (ostream& out) {
  out << "Memory Control " << m_version << ":" << endl;
  out << "  Ruby cycles per memory cycle: " << m_mem_bus_cycle_multiplier << endl;
  out << "  Basic read latency: " << m_mem_ctl_latency << endl;
  if (m_memFixedDelay) {
    out << "  Fixed Latency mode:  Added cycles = " << m_memFixedDelay << endl;
  } else {
    out << "  Bank busy time: " << BANK_BUSY_TIME << " memory cycles" << endl;
    out << "  Memory channel busy time: " << m_basic_bus_busy_time << endl;
    out << "  Dead cycles between reads to different ranks: " << m_rank_rank_delay << endl;
    out << "  Dead cycle between a read and a write: " << m_read_write_delay << endl;
    out << "  tFaw (four-activate) window: " << m_tFaw << endl;
  }
  out << "  Banks per rank: " << m_banks_per_rank << endl;
  out << "  Ranks per DIMM: " << m_ranks_per_dimm << endl;
  out << "  DIMMs per channel:  " << m_dimms_per_channel << endl;
  out << "  LSB of bank field in address: " << m_bank_bit_0 << endl;
  out << "  LSB of rank field in address: " << m_rank_bit_0 << endl;
  out << "  LSB of DIMM field in address: " << m_dimm_bit_0 << endl;
  out << "  Max size of each bank queue: " << m_bank_queue_size << endl;
  out << "  Refresh period (within one bank): " << m_refresh_period << endl;
  out << "  Arbitration randomness: " << m_memRandomArbitrate << endl;
}


void MemoryControl::setDebug (int debugFlag) {
  m_debug = debugFlag;
}


// ****************************************************************

// PRIVATE METHODS

// Queue up a completed request to send back to directory

void MemoryControl::enqueueToDirectory (MemoryNode req, int latency) {
  Time arrival_time = g_eventQueue_ptr->getTime()
                    + (latency * m_mem_bus_cycle_multiplier);
  req.m_time = arrival_time;
  m_response_queue.push_back(req);

  // schedule the wake up
  g_eventQueue_ptr->scheduleEventAbsolute(m_consumer_ptr, arrival_time);
}



// getBank returns an integer that is unique for each
// bank across this memory controller.

int MemoryControl::getBank (physical_address_t addr) {
  int dimm = (addr >> m_dimm_bit_0) & (m_dimms_per_channel - 1);
  int rank = (addr >> m_rank_bit_0) & (m_ranks_per_dimm - 1);
  int bank = (addr >> m_bank_bit_0) & (m_banks_per_rank - 1);
  return (dimm * m_ranks_per_dimm * m_banks_per_rank)
       + (rank * m_banks_per_rank)
       + bank;
}

// getRank returns an integer that is unique for each rank
// and independent of individual bank.

int MemoryControl::getRank (int bank) {
  int rank = (bank / m_banks_per_rank);
  assert (rank < (m_ranks_per_dimm * m_dimms_per_channel));
  return rank;
}


// queueReady determines if the head item in a bank queue
// can be issued this cycle

bool MemoryControl::queueReady (int bank) {
  if ((m_bankBusyCounter[bank] > 0) && !m_memFixedDelay) {
    g_system_ptr->getProfiler()->profileMemBankBusy();
    //if (m_debug) printf("  bank %x busy %d\n", bank, m_bankBusyCounter[bank]);
    return false;
  }
  if (m_memRandomArbitrate >= 2) {
    if ((random() % 100) < m_memRandomArbitrate) {
      g_system_ptr->getProfiler()->profileMemRandBusy();
      return false;
    }
  }
  if (m_memFixedDelay) return true;
  if ((m_ageCounter > (2 * m_bank_busy_time)) && !m_oldRequest[bank]) {
    g_system_ptr->getProfiler()->profileMemNotOld();
    return false;
  }
  if (m_busBusyCounter_Basic == m_basic_bus_busy_time) {
    // Another bank must have issued this same cycle.
    // For profiling, we count this as an arb wait rather than
    // a bus wait.  This is a little inaccurate since it MIGHT
    // have also been blocked waiting for a read-write or a
    // read-read instead, but it's pretty close.
    g_system_ptr->getProfiler()->profileMemArbWait(1);
    return false;
  }
  if (m_busBusyCounter_Basic > 0) {
    g_system_ptr->getProfiler()->profileMemBusBusy();
    return false;
  }
  int rank = getRank(bank);
  if (m_tfaw_count[rank] >= ACTIVATE_PER_TFAW) {
    g_system_ptr->getProfiler()->profileMemTfawBusy();
    return false;
  }
  bool write = !m_bankQueues[bank].front().m_is_mem_read;
  if (write && (m_busBusyCounter_Write > 0)) {
    g_system_ptr->getProfiler()->profileMemReadWriteBusy();
    return false;
  }
  if (!write && (rank != m_busBusy_WhichRank)
             && (m_busBusyCounter_ReadNewRank > 0)) {
    g_system_ptr->getProfiler()->profileMemDataBusBusy();
    return false;
  }
  return true;
}


// issueRefresh checks to see if this bank has a refresh scheduled
// and, if so, does the refresh and returns true

bool MemoryControl::issueRefresh (int bank) {
  if (!m_need_refresh || (m_refresh_bank != bank)) return false;
  if (m_bankBusyCounter[bank] > 0) return false;
  // Note that m_busBusyCounter will prevent multiple issues during
  // the same cycle, as well as on different but close cycles:
  if (m_busBusyCounter_Basic > 0) return false;
  int rank = getRank(bank);
  if (m_tfaw_count[rank] >= ACTIVATE_PER_TFAW) return false;

  // Issue it:

  //if (m_debug) {
    //uint64 current_time = g_eventQueue_ptr->getTime();
    //printf("    Refresh bank %3x at %lld\n", bank, current_time);
  //}
  g_system_ptr->getProfiler()->profileMemRefresh();
  m_need_refresh--;
  m_refresh_bank++;
  if (m_refresh_bank >= m_total_banks) m_refresh_bank = 0;
  m_bankBusyCounter[bank] = m_bank_busy_time;
  m_busBusyCounter_Basic = m_basic_bus_busy_time;
  m_busBusyCounter_Write = m_basic_bus_busy_time;
  m_busBusyCounter_ReadNewRank = m_basic_bus_busy_time;
  markTfaw(rank);
  return true;
}


// Mark the activate in the tFaw shift register
void MemoryControl::markTfaw (int rank) {
  if (m_tFaw) {
    m_tfaw_shift[rank] |= (1 << (m_tFaw-1));
    m_tfaw_count[rank]++;
  }
}


// Issue a memory request:  Activate the bank,
// reserve the address and data buses, and queue
// the request for return to the requesting
// processor after a fixed latency.

void MemoryControl::issueRequest (int bank) {
  int rank = getRank(bank);
  MemoryNode req = m_bankQueues[bank].front();
  m_bankQueues[bank].pop_front();
  if (m_debug) {
    uint64 current_time = g_eventQueue_ptr->getTime();
    printf("    Mem issue request%7d: 0x%08llx %c         at %10lld  bank =%3x\n",
        req.m_msg_counter, req.m_addr, req.m_is_mem_read? 'R':'W', current_time, bank);
  }
  if (req.m_msgptr.ref() != NULL) {  // don't enqueue L3 writebacks
    enqueueToDirectory(req, m_mem_ctl_latency + m_memFixedDelay);
  }
  m_oldRequest[bank] = 0;
  markTfaw(rank);
  m_bankBusyCounter[bank] = m_bank_busy_time;
  m_busBusy_WhichRank = rank;
  if (req.m_is_mem_read) {
    g_system_ptr->getProfiler()->profileMemRead();
    m_busBusyCounter_Basic = m_basic_bus_busy_time;
    m_busBusyCounter_Write = m_basic_bus_busy_time + m_read_write_delay;
    m_busBusyCounter_ReadNewRank = m_basic_bus_busy_time + m_rank_rank_delay;
  } else {
    g_system_ptr->getProfiler()->profileMemWrite();
    m_busBusyCounter_Basic = m_basic_bus_busy_time;
    m_busBusyCounter_Write = m_basic_bus_busy_time;
    m_busBusyCounter_ReadNewRank = m_basic_bus_busy_time;
  }
}


// executeCycle:  This function is called once per memory clock cycle
// to simulate all the periodic hardware.

void MemoryControl::executeCycle () {
  // Keep track of time by counting down the busy counters:
  for (int bank=0; bank < m_total_banks; bank++) {
    if (m_bankBusyCounter[bank] > 0) m_bankBusyCounter[bank]--;
  }
  if (m_busBusyCounter_Write > 0) m_busBusyCounter_Write--;
  if (m_busBusyCounter_ReadNewRank > 0) m_busBusyCounter_ReadNewRank--;
  if (m_busBusyCounter_Basic > 0) m_busBusyCounter_Basic--;

  // Count down the tFAW shift registers:
  for (int rank=0; rank < m_total_ranks; rank++) {
    if (m_tfaw_shift[rank] & 1) m_tfaw_count[rank]--;
    m_tfaw_shift[rank] >>= 1;
  }

  // After time period expires, latch an indication that we need a refresh.
  // Disable refresh if in memFixedDelay mode.
  if (!m_memFixedDelay) m_refresh_count--;
  if (m_refresh_count == 0) {
    m_refresh_count = m_refresh_period_system;
    assert (m_need_refresh < 10);  // Are we overrunning our ability to refresh?
    m_need_refresh++;
  }

  // If this batch of requests is all done, make a new batch:
  m_ageCounter++;
  int anyOld = 0;
  for (int bank=0; bank < m_total_banks; bank++) {
    anyOld |= m_oldRequest[bank];
  }
  if (!anyOld) {
    for (int bank=0; bank < m_total_banks; bank++) {
      if (!m_bankQueues[bank].empty()) m_oldRequest[bank] = 1;
    }
    m_ageCounter = 0;
  }

  // If randomness desired, re-randomize round-robin position each cycle
  if (m_memRandomArbitrate) {
    m_roundRobin = random() % m_total_banks;
  }


  // For each channel, scan round-robin, and pick an old, ready
  // request and issue it.  Treat a refresh request as if it
  // were at the head of its bank queue.  After we issue something,
  // keep scanning the queues just to gather statistics about
  // how many are waiting.  If in memFixedDelay mode, we can issue
  // more than one request per cycle.

  int queueHeads = 0;
  int banksIssued = 0;
  for (int i = 0; i < m_total_banks; i++) {
    m_roundRobin++;
    if (m_roundRobin >= m_total_banks) m_roundRobin = 0;
    issueRefresh(m_roundRobin);
    int qs = m_bankQueues[m_roundRobin].size();
    if (qs > 1) {
      g_system_ptr->getProfiler()->profileMemBankQ(qs-1);
    }
    if (qs > 0) {
      m_idleCount = IDLECOUNT_MAX_VALUE; // we're not idle if anything is queued
      queueHeads++;
      if (queueReady(m_roundRobin)) {
        issueRequest(m_roundRobin);
        banksIssued++;
        if (m_memFixedDelay) {
          g_system_ptr->getProfiler()->profileMemWaitCycles(m_memFixedDelay);
        }
      }
    }
  }

  // memWaitCycles is a redundant catch-all for the specific counters in queueReady
  g_system_ptr->getProfiler()->profileMemWaitCycles(queueHeads - banksIssued);

  // Check input queue and move anything to bank queues if not full.
  // Since this is done here at the end of the cycle, there will always
  // be at least one cycle of latency in the bank queue.
  // We deliberately move at most one request per cycle (to simulate
  // typical hardware).  Note that if one bank queue fills up, other
  // requests can get stuck behind it here.

  if (!m_input_queue.empty()) {
    m_idleCount = IDLECOUNT_MAX_VALUE; // we're not idle if anything is pending
    MemoryNode req = m_input_queue.front();
    int bank = getBank(req.m_addr);
    if (m_bankQueues[bank].size() < m_bank_queue_size) {
      m_input_queue.pop_front();
      m_bankQueues[bank].push_back(req);
    }
    g_system_ptr->getProfiler()->profileMemInputQ(m_input_queue.size());
  }
}


// wakeup:  This function is called once per memory controller clock cycle.

void MemoryControl::wakeup () {

  // execute everything
  executeCycle();

  m_idleCount--;
  if (m_idleCount <= 0) {
    m_awakened = 0;
  } else {
    // Reschedule ourselves so that we run every memory cycle:
    g_eventQueue_ptr->scheduleEvent(this, m_mem_bus_cycle_multiplier);
  }
}


