
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
 * $Id: Sequencer.C 1.131 2006/11/06 17:41:01-06:00 bobba@gratiano.cs.wisc.edu $
 *
 */

#include "mem/ruby/common/Global.hh"
#include "mem/ruby/system/Sequencer.hh"
#include "mem/ruby/system/System.hh"
#include "mem/protocol/Protocol.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/system/CacheMemory.hh"
#include "mem/ruby/config/RubyConfig.hh"
//#include "mem/ruby/recorder/Tracer.hh"
#include "mem/ruby/slicc_interface/AbstractChip.hh"
#include "mem/protocol/Chip.hh"
#include "mem/ruby/tester/Tester.hh"
#include "mem/ruby/common/SubBlock.hh"
#include "mem/protocol/Protocol.hh"
#include "mem/gems_common/Map.hh"
#include "mem/packet.hh"

Sequencer::Sequencer(AbstractChip* chip_ptr, int version) {
  m_chip_ptr = chip_ptr;
  m_version = version;

  m_deadlock_check_scheduled = false;
  m_outstanding_count = 0;

  int smt_threads = RubyConfig::numberofSMTThreads();
  m_writeRequestTable_ptr = new Map<Address, CacheMsg>*[smt_threads];
  m_readRequestTable_ptr = new Map<Address, CacheMsg>*[smt_threads];

  m_packetTable_ptr = new Map<Address, Packet*>;

  for(int p=0; p < smt_threads; ++p){
    m_writeRequestTable_ptr[p] = new Map<Address, CacheMsg>;
    m_readRequestTable_ptr[p] = new Map<Address, CacheMsg>;
  }

}

Sequencer::~Sequencer() {
  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int i=0; i < smt_threads; ++i){
    if(m_writeRequestTable_ptr[i]){
      delete m_writeRequestTable_ptr[i];
    }
    if(m_readRequestTable_ptr[i]){
      delete m_readRequestTable_ptr[i];
    }
  }
  if(m_writeRequestTable_ptr){
    delete [] m_writeRequestTable_ptr;
  }
  if(m_readRequestTable_ptr){
    delete [] m_readRequestTable_ptr;
  }
}

void Sequencer::wakeup() {
  // Check for deadlock of any of the requests
  Time current_time = g_eventQueue_ptr->getTime();
  bool deadlock = false;

  // Check across all outstanding requests
  int smt_threads = RubyConfig::numberofSMTThreads();
  int total_outstanding = 0;
  for(int p=0; p < smt_threads; ++p){
    Vector<Address> keys = m_readRequestTable_ptr[p]->keys();
    for (int i=0; i<keys.size(); i++) {
      CacheMsg& request = m_readRequestTable_ptr[p]->lookup(keys[i]);
      if (current_time - request.getTime() >= g_DEADLOCK_THRESHOLD) {
        WARN_MSG("Possible Deadlock detected");
        WARN_EXPR(request);
        WARN_EXPR(m_chip_ptr->getID());
        WARN_EXPR(m_version);
        WARN_EXPR(keys.size());
        WARN_EXPR(current_time);
        WARN_EXPR(request.getTime());
        WARN_EXPR(current_time - request.getTime());
        WARN_EXPR(*m_readRequestTable_ptr[p]);
        ERROR_MSG("Aborting");
        deadlock = true;
      }
    }

    keys = m_writeRequestTable_ptr[p]->keys();
    for (int i=0; i<keys.size(); i++) {
      CacheMsg& request = m_writeRequestTable_ptr[p]->lookup(keys[i]);
      if (current_time - request.getTime() >= g_DEADLOCK_THRESHOLD) {
        WARN_MSG("Possible Deadlock detected");
        WARN_EXPR(request);
        WARN_EXPR(m_chip_ptr->getID());
        WARN_EXPR(m_version);
        WARN_EXPR(current_time);
        WARN_EXPR(request.getTime());
        WARN_EXPR(current_time - request.getTime());
        WARN_EXPR(keys.size());
        WARN_EXPR(*m_writeRequestTable_ptr[p]);
        ERROR_MSG("Aborting");
        deadlock = true;
      }
    }
    total_outstanding += m_writeRequestTable_ptr[p]->size() + m_readRequestTable_ptr[p]->size();
  }  // across all request tables
  assert(m_outstanding_count == total_outstanding);

  if (m_outstanding_count > 0) { // If there are still outstanding requests, keep checking
    g_eventQueue_ptr->scheduleEvent(this, g_DEADLOCK_THRESHOLD);
  } else {
    m_deadlock_check_scheduled = false;
  }
}

//returns the total number of requests
int Sequencer::getNumberOutstanding(){
  return m_outstanding_count;
}

// returns the total number of demand requests
int Sequencer::getNumberOutstandingDemand(){
  int smt_threads = RubyConfig::numberofSMTThreads();
  int total_demand = 0;
  for(int p=0; p < smt_threads; ++p){
    Vector<Address> keys = m_readRequestTable_ptr[p]->keys();
    for (int i=0; i< keys.size(); i++) {
      CacheMsg& request = m_readRequestTable_ptr[p]->lookup(keys[i]);
      if(request.getPrefetch() == PrefetchBit_No){
        total_demand++;
      }
    }

    keys = m_writeRequestTable_ptr[p]->keys();
    for (int i=0; i< keys.size(); i++) {
      CacheMsg& request = m_writeRequestTable_ptr[p]->lookup(keys[i]);
      if(request.getPrefetch() == PrefetchBit_No){
        total_demand++;
      }
    }
  }

  return total_demand;
}

int Sequencer::getNumberOutstandingPrefetch(){
  int smt_threads = RubyConfig::numberofSMTThreads();
  int total_prefetch = 0;
  for(int p=0; p < smt_threads; ++p){
    Vector<Address> keys = m_readRequestTable_ptr[p]->keys();
    for (int i=0; i< keys.size(); i++) {
      CacheMsg& request = m_readRequestTable_ptr[p]->lookup(keys[i]);
      if(request.getPrefetch() == PrefetchBit_Yes){
        total_prefetch++;
      }
    }

    keys = m_writeRequestTable_ptr[p]->keys();
    for (int i=0; i< keys.size(); i++) {
      CacheMsg& request = m_writeRequestTable_ptr[p]->lookup(keys[i]);
      if(request.getPrefetch() == PrefetchBit_Yes){
        total_prefetch++;
      }
    }
  }

  return total_prefetch;
}

bool Sequencer::isPrefetchRequest(const Address & lineaddr){
  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int p=0; p < smt_threads; ++p){
    // check load requests
    Vector<Address> keys = m_readRequestTable_ptr[p]->keys();
    for (int i=0; i< keys.size(); i++) {
      CacheMsg& request = m_readRequestTable_ptr[p]->lookup(keys[i]);
      if(line_address(request.getAddress()) == lineaddr){
        if(request.getPrefetch() == PrefetchBit_Yes){
          return true;
        }
        else{
          return false;
        }
      }
    }

    // check store requests
    keys = m_writeRequestTable_ptr[p]->keys();
    for (int i=0; i< keys.size(); i++) {
      CacheMsg& request = m_writeRequestTable_ptr[p]->lookup(keys[i]);
      if(line_address(request.getAddress()) == lineaddr){
        if(request.getPrefetch() == PrefetchBit_Yes){
          return true;
        }
        else{
          return false;
        }
      }
    }
  }
  // we should've found a matching request
  cout << "isRequestPrefetch() ERROR request NOT FOUND : " << lineaddr << endl;
  printProgress(cout);
  assert(0);
}

AccessModeType Sequencer::getAccessModeOfRequest(Address addr, int thread){
  if(m_readRequestTable_ptr[thread]->exist(line_address(addr))){
    CacheMsg& request = m_readRequestTable_ptr[thread]->lookup(addr);
    return request.getAccessMode();
  } else if(m_writeRequestTable_ptr[thread]->exist(line_address(addr))){
    CacheMsg& request = m_writeRequestTable_ptr[thread]->lookup(addr);
    return request.getAccessMode();
  } else {
    printProgress(cout);
    ERROR_MSG("Request not found in RequestTables");
  }
}

Address Sequencer::getLogicalAddressOfRequest(Address addr, int thread){
  assert(thread >= 0);
  if(m_readRequestTable_ptr[thread]->exist(line_address(addr))){
    CacheMsg& request = m_readRequestTable_ptr[thread]->lookup(addr);
    return request.getLogicalAddress();
  } else if(m_writeRequestTable_ptr[thread]->exist(line_address(addr))){
    CacheMsg& request = m_writeRequestTable_ptr[thread]->lookup(addr);
    return request.getLogicalAddress();
  } else {
    printProgress(cout);
    WARN_MSG("Request not found in RequestTables");
    WARN_MSG(addr);
    WARN_MSG(thread);
    ASSERT(0);
  }
}

// returns the ThreadID of the request
int Sequencer::getRequestThreadID(const Address & addr){
  int smt_threads = RubyConfig::numberofSMTThreads();
  int thread = -1;
  int num_found = 0;
  for(int p=0; p < smt_threads; ++p){
    if(m_readRequestTable_ptr[p]->exist(addr)){
      num_found++;
      thread = p;
    }
    if(m_writeRequestTable_ptr[p]->exist(addr)){
      num_found++;
      thread = p;
    }
  }
  if(num_found != 1){
    cout << "getRequestThreadID ERROR too many matching requests addr = " << addr << endl;
    printProgress(cout);
  }
  ASSERT(num_found == 1);
  ASSERT(thread != -1);

  return thread;
}

// given a line address, return the request's physical address
Address Sequencer::getRequestPhysicalAddress(const Address & lineaddr){
  int smt_threads = RubyConfig::numberofSMTThreads();
  Address physaddr;
  int num_found = 0;
  for(int p=0; p < smt_threads; ++p){
    if(m_readRequestTable_ptr[p]->exist(lineaddr)){
      num_found++;
      physaddr = (m_readRequestTable_ptr[p]->lookup(lineaddr)).getAddress();
    }
    if(m_writeRequestTable_ptr[p]->exist(lineaddr)){
      num_found++;
      physaddr = (m_writeRequestTable_ptr[p]->lookup(lineaddr)).getAddress();
    }
  }
  if(num_found != 1){
    cout << "getRequestPhysicalAddress ERROR too many matching requests addr = " << lineaddr << endl;
    printProgress(cout);
  }
  ASSERT(num_found == 1);

  return physaddr;
}

void Sequencer::printProgress(ostream& out) const{

  int total_demand = 0;
  out << "Sequencer Stats Version " << m_version << endl;
  out << "Current time = " << g_eventQueue_ptr->getTime() << endl;
  out << "---------------" << endl;
  out << "outstanding requests" << endl;

  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int p=0; p < smt_threads; ++p){
    Vector<Address> rkeys = m_readRequestTable_ptr[p]->keys();
    int read_size = rkeys.size();
    out << "proc " << m_chip_ptr->getID() << " thread " << p << " Read Requests = " << read_size << endl;
    // print the request table
    for(int i=0; i < read_size; ++i){
      CacheMsg & request = m_readRequestTable_ptr[p]->lookup(rkeys[i]);
      out << "\tRequest[ " << i << " ] = " << request.getType() << " Address " << rkeys[i]  << " Posted " << request.getTime() << " PF " << request.getPrefetch() << endl;
      if( request.getPrefetch() == PrefetchBit_No ){
        total_demand++;
      }
    }

    Vector<Address> wkeys = m_writeRequestTable_ptr[p]->keys();
    int write_size = wkeys.size();
    out << "proc " << m_chip_ptr->getID() << " thread " << p << " Write Requests = " << write_size << endl;
    // print the request table
    for(int i=0; i < write_size; ++i){
      CacheMsg & request = m_writeRequestTable_ptr[p]->lookup(wkeys[i]);
      out << "\tRequest[ " << i << " ] = " << request.getType() << " Address " << wkeys[i]  << " Posted " << request.getTime() << " PF " << request.getPrefetch() << endl;
      if( request.getPrefetch() == PrefetchBit_No ){
        total_demand++;
      }
    }

    out << endl;
  }
  out << "Total Number Outstanding: " << m_outstanding_count << endl;
  out << "Total Number Demand     : " << total_demand << endl;
  out << "Total Number Prefetches : " << m_outstanding_count - total_demand << endl;
  out << endl;
  out << endl;

}

void Sequencer::printConfig(ostream& out) {
  if (TSO) {
    out << "sequencer: Sequencer - TSO" << endl;
  } else {
    out << "sequencer: Sequencer - SC" << endl;
  }
  out << "  max_outstanding_requests: " << g_SEQUENCER_OUTSTANDING_REQUESTS << endl;
}

bool Sequencer::empty() const {
  return m_outstanding_count == 0;
}

// Insert the request on the correct request table.  Return true if
// the entry was already present.
bool Sequencer::insertRequest(const CacheMsg& request) {
  int thread = request.getThreadID();
  assert(thread >= 0);
  int total_outstanding = 0;
  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int p=0; p < smt_threads; ++p){
    total_outstanding += m_writeRequestTable_ptr[p]->size() + m_readRequestTable_ptr[p]->size();
  }
  assert(m_outstanding_count == total_outstanding);

  // See if we should schedule a deadlock check
  if (m_deadlock_check_scheduled == false) {
    g_eventQueue_ptr->scheduleEvent(this, g_DEADLOCK_THRESHOLD);
    m_deadlock_check_scheduled = true;
  }

  if ((request.getType() == CacheRequestType_ST) ||
      (request.getType() == CacheRequestType_ATOMIC)) {
    if (m_writeRequestTable_ptr[thread]->exist(line_address(request.getAddress()))) {
      m_writeRequestTable_ptr[thread]->lookup(line_address(request.getAddress())) = request;
      return true;
    }
    m_writeRequestTable_ptr[thread]->allocate(line_address(request.getAddress()));
    m_writeRequestTable_ptr[thread]->lookup(line_address(request.getAddress())) = request;
    m_outstanding_count++;
  } else {
    if (m_readRequestTable_ptr[thread]->exist(line_address(request.getAddress()))) {
      m_readRequestTable_ptr[thread]->lookup(line_address(request.getAddress())) = request;
      return true;
    }
    m_readRequestTable_ptr[thread]->allocate(line_address(request.getAddress()));
    m_readRequestTable_ptr[thread]->lookup(line_address(request.getAddress())) = request;
    m_outstanding_count++;
  }

  g_system_ptr->getProfiler()->sequencerRequests(m_outstanding_count);

  total_outstanding = 0;
  for(int p=0; p < smt_threads; ++p){
    total_outstanding += m_writeRequestTable_ptr[p]->size() + m_readRequestTable_ptr[p]->size();
  }

  assert(m_outstanding_count == total_outstanding);
  return false;
}

void Sequencer::removeRequest(const CacheMsg& request) {
  int thread = request.getThreadID();
  assert(thread >= 0);
  int total_outstanding = 0;
  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int p=0; p < smt_threads; ++p){
    total_outstanding += m_writeRequestTable_ptr[p]->size() + m_readRequestTable_ptr[p]->size();
  }
  assert(m_outstanding_count == total_outstanding);

  if ((request.getType() == CacheRequestType_ST) ||
      (request.getType() == CacheRequestType_ATOMIC)) {
    m_writeRequestTable_ptr[thread]->deallocate(line_address(request.getAddress()));
  } else {
    m_readRequestTable_ptr[thread]->deallocate(line_address(request.getAddress()));
  }
  m_outstanding_count--;

  total_outstanding = 0;
  for(int p=0; p < smt_threads; ++p){
    total_outstanding += m_writeRequestTable_ptr[p]->size() + m_readRequestTable_ptr[p]->size();
  }
  assert(m_outstanding_count == total_outstanding);
}

void Sequencer::writeCallback(const Address& address) {
  DataBlock data;
  writeCallback(address, data);
}

void Sequencer::writeCallback(const Address& address, DataBlock& data) {
  // process oldest thread first
  int thread = -1;
  Time oldest_time = 0;
  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int t=0; t < smt_threads; ++t){
    if(m_writeRequestTable_ptr[t]->exist(address)){
      CacheMsg & request = m_writeRequestTable_ptr[t]->lookup(address);
      if(thread == -1 || (request.getTime() < oldest_time) ){
        thread = t;
        oldest_time = request.getTime();
      }
    }
  }
  // make sure we found an oldest thread
  ASSERT(thread != -1);

  CacheMsg & request = m_writeRequestTable_ptr[thread]->lookup(address);

  writeCallback(address, data, GenericMachineType_NULL, PrefetchBit_No, thread);
}

void Sequencer::writeCallback(const Address& address, DataBlock& data, GenericMachineType respondingMach, PrefetchBit pf, int thread) {

  assert(address == line_address(address));
  assert(thread >= 0);
  assert(m_writeRequestTable_ptr[thread]->exist(line_address(address)));

  writeCallback(address, data, respondingMach, thread);

}

void Sequencer::writeCallback(const Address& address, DataBlock& data, GenericMachineType respondingMach, int thread) {
  assert(address == line_address(address));
  assert(m_writeRequestTable_ptr[thread]->exist(line_address(address)));
  CacheMsg request = m_writeRequestTable_ptr[thread]->lookup(address);
  assert( request.getThreadID() == thread);
  removeRequest(request);

  assert((request.getType() == CacheRequestType_ST) ||
         (request.getType() == CacheRequestType_ATOMIC));

  hitCallback(request, data, respondingMach, thread);

}

void Sequencer::readCallback(const Address& address) {
  DataBlock data;
  readCallback(address, data);
}

void Sequencer::readCallback(const Address& address, DataBlock& data) {
  // process oldest thread first
  int thread = -1;
  Time oldest_time = 0;
  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int t=0; t < smt_threads; ++t){
    if(m_readRequestTable_ptr[t]->exist(address)){
      CacheMsg & request = m_readRequestTable_ptr[t]->lookup(address);
      if(thread == -1 || (request.getTime() < oldest_time) ){
        thread = t;
        oldest_time = request.getTime();
      }
    }
  }
  // make sure we found an oldest thread
  ASSERT(thread != -1);

  CacheMsg & request = m_readRequestTable_ptr[thread]->lookup(address);

  readCallback(address, data, GenericMachineType_NULL, PrefetchBit_No, thread);
}

void Sequencer::readCallback(const Address& address, DataBlock& data, GenericMachineType respondingMach, PrefetchBit pf, int thread) {

  assert(address == line_address(address));
  assert(m_readRequestTable_ptr[thread]->exist(line_address(address)));

  readCallback(address, data, respondingMach, thread);
}

void Sequencer::readCallback(const Address& address, DataBlock& data, GenericMachineType respondingMach, int thread) {
  assert(address == line_address(address));
  assert(m_readRequestTable_ptr[thread]->exist(line_address(address)));

  CacheMsg request = m_readRequestTable_ptr[thread]->lookup(address);
  assert( request.getThreadID() == thread );
  removeRequest(request);

  assert((request.getType() == CacheRequestType_LD) ||
         (request.getType() == CacheRequestType_IFETCH)
         );

  hitCallback(request, data, respondingMach, thread);
}

void Sequencer::hitCallback(const CacheMsg& request, DataBlock& data, GenericMachineType respondingMach, int thread) {
  int size = request.getSize();
  Address request_address = request.getAddress();
  Address request_logical_address = request.getLogicalAddress();
  Address request_line_address = line_address(request_address);
  CacheRequestType type = request.getType();
  int threadID = request.getThreadID();
  Time issued_time = request.getTime();
  int logical_proc_no = ((m_chip_ptr->getID() * RubyConfig::numberOfProcsPerChip()) + m_version) * RubyConfig::numberofSMTThreads() + threadID;

  DEBUG_MSG(SEQUENCER_COMP, MedPrio, size);

  // Set this cache entry to the most recently used
  if (type == CacheRequestType_IFETCH) {
    if (Protocol::m_TwoLevelCache) {
      if (m_chip_ptr->m_L1Cache_L1IcacheMemory_vec[m_version]->isTagPresent(request_line_address)) {
        m_chip_ptr->m_L1Cache_L1IcacheMemory_vec[m_version]->setMRU(request_line_address);
      }
    }
    else {
      if (m_chip_ptr->m_L1Cache_cacheMemory_vec[m_version]->isTagPresent(request_line_address)) {
        m_chip_ptr->m_L1Cache_cacheMemory_vec[m_version]->setMRU(request_line_address);
      }
    }
  } else {
    if (Protocol::m_TwoLevelCache) {
      if (m_chip_ptr->m_L1Cache_L1DcacheMemory_vec[m_version]->isTagPresent(request_line_address)) {
        m_chip_ptr->m_L1Cache_L1DcacheMemory_vec[m_version]->setMRU(request_line_address);
      }
    }
    else {
      if (m_chip_ptr->m_L1Cache_cacheMemory_vec[m_version]->isTagPresent(request_line_address)) {
        m_chip_ptr->m_L1Cache_cacheMemory_vec[m_version]->setMRU(request_line_address);
      }
    }
  }

  assert(g_eventQueue_ptr->getTime() >= issued_time);
  Time miss_latency = g_eventQueue_ptr->getTime() - issued_time;

  if (PROTOCOL_DEBUG_TRACE) {
    g_system_ptr->getProfiler()->profileTransition("Seq", (m_chip_ptr->getID()*RubyConfig::numberOfProcsPerChip()+m_version), -1, request.getAddress(), "", "Done", "",
                                                   int_to_string(miss_latency)+" cycles "+GenericMachineType_to_string(respondingMach)+" "+CacheRequestType_to_string(request.getType())+" "+PrefetchBit_to_string(request.getPrefetch()));
  }

  DEBUG_MSG(SEQUENCER_COMP, MedPrio, request_address);
  DEBUG_MSG(SEQUENCER_COMP, MedPrio, request.getPrefetch());
  if (request.getPrefetch() == PrefetchBit_Yes) {
    DEBUG_MSG(SEQUENCER_COMP, MedPrio, "return");
    g_system_ptr->getProfiler()->swPrefetchLatency(miss_latency, type, respondingMach);
    return; // Ignore the software prefetch, don't callback the driver
  }

  // Profile the miss latency for all non-zero demand misses
  if (miss_latency != 0) {
    g_system_ptr->getProfiler()->missLatency(miss_latency, type, respondingMach);

  }

  bool write =
    (type == CacheRequestType_ST) ||
    (type == CacheRequestType_ATOMIC);

  if (TSO && write) {
    m_chip_ptr->m_L1Cache_storeBuffer_vec[m_version]->callBack(line_address(request.getAddress()), data,
                                                               m_packetTable_ptr->lookup(request.getAddress()));
  } else {

    // Copy the correct bytes out of the cache line into the subblock
    SubBlock subblock(request_address, request_logical_address, size);
    subblock.mergeFrom(data);  // copy the correct bytes from DataBlock in the SubBlock

    // Scan the store buffer to see if there are any outstanding stores we need to collect
    if (TSO) {
      m_chip_ptr->m_L1Cache_storeBuffer_vec[m_version]->updateSubBlock(subblock);
    }

    // Call into the Driver and let it read and/or modify the sub-block
    Packet* pkt = m_packetTable_ptr->lookup(request.getAddress());

    // update data if this is a store/atomic

    /*
    if (pkt->req->isCondSwap()) {
      L1Cache_Entry entry = m_L1Cache_vec[m_version]->lookup(Address(pkt->req->physAddr()));
      DataBlk datablk = entry->getDataBlk();
      uint8_t *orig_data = datablk.getArray();
      if ( datablk.equal(pkt->req->getExtraData()) )
        datablk->setArray(pkt->getData());
      pkt->setData(orig_data);
    }
    */

    g_system_ptr->getDriver()->hitCallback(pkt);
    m_packetTable_ptr->remove(request.getAddress());

    // If the request was a Store or Atomic, apply the changes in the SubBlock to the DataBlock
    // (This is only triggered for the non-TSO case)
    if (write) {
      assert(!TSO);
      subblock.mergeTo(data);    // copy the correct bytes from SubBlock into the DataBlock
    }
  }
}

void Sequencer::printDebug(){
  //notify driver of debug
  g_system_ptr->getDriver()->printDebug();
}

//dsm: breaks build, delayed
// Returns true if the sequencer already has a load or store outstanding
bool
Sequencer::isReady(const Packet* pkt) const
{

  int cpu_number = pkt->req->contextId();
  la_t logical_addr = pkt->req->getVaddr();
  pa_t physical_addr = pkt->req->getPaddr();
  CacheRequestType type_of_request;
  if ( pkt->req->isInstFetch() ) {
    type_of_request = CacheRequestType_IFETCH;
  } else if ( pkt->req->isLocked() || pkt->req->isSwap() ) {
    type_of_request = CacheRequestType_ATOMIC;
  } else if ( pkt->isRead() ) {
    type_of_request = CacheRequestType_LD;
  } else if ( pkt->isWrite() ) {
    type_of_request = CacheRequestType_ST;
  } else {
    assert(false);
  }
  int thread = pkt->req->threadId();

  CacheMsg request(Address( physical_addr ),
                   Address( physical_addr ),
                   type_of_request,
                   Address(0),
                   AccessModeType_UserMode,   // User/supervisor mode
                   0,   // Size in bytes of request
                   PrefetchBit_No,  // Not a prefetch
                   0,              // Version number
                   Address(logical_addr),   // Virtual Address
                   thread              // SMT thread
                   );
  return isReady(request);
}

bool
Sequencer::isReady(const CacheMsg& request) const
{
  if (m_outstanding_count >= g_SEQUENCER_OUTSTANDING_REQUESTS) {
    //cout << "TOO MANY OUTSTANDING: " << m_outstanding_count << " " << g_SEQUENCER_OUTSTANDING_REQUESTS << " VER " << m_version << endl;
    //printProgress(cout);
    return false;
  }

  // This code allows reads to be performed even when we have a write
  // request outstanding for the line
  bool write =
    (request.getType() == CacheRequestType_ST) ||
    (request.getType() == CacheRequestType_ATOMIC);

  // LUKE - disallow more than one request type per address
  //     INVARIANT: at most one request type per address, per processor
  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int p=0; p < smt_threads; ++p){
    if( m_writeRequestTable_ptr[p]->exist(line_address(request.getAddress())) ||
        m_readRequestTable_ptr[p]->exist(line_address(request.getAddress())) ){
      //cout << "OUTSTANDING REQUEST EXISTS " << p << " VER " << m_version << endl;
      //printProgress(cout);
      return false;
    }
  }

  if (TSO) {
    return m_chip_ptr->m_L1Cache_storeBuffer_vec[m_version]->isReady();
  }
  return true;
}

//dsm: breaks build, delayed
// Called by Driver (Simics or Tester).
void
Sequencer::makeRequest(Packet* pkt)
{
  int cpu_number = pkt->req->contextId();
  la_t logical_addr = pkt->req->getVaddr();
  pa_t physical_addr = pkt->req->getPaddr();
  int request_size = pkt->getSize();
  CacheRequestType type_of_request;
  PrefetchBit prefetch;
  bool write = false;
  if ( pkt->req->isInstFetch() ) {
    type_of_request = CacheRequestType_IFETCH;
  } else if ( pkt->req->isLocked() || pkt->req->isSwap() ) {
    type_of_request = CacheRequestType_ATOMIC;
    write = true;
  } else if ( pkt->isRead() ) {
    type_of_request = CacheRequestType_LD;
  } else if ( pkt->isWrite() ) {
    type_of_request = CacheRequestType_ST;
    write = true;
  } else {
    assert(false);
  }
  if (pkt->req->isPrefetch()) {
    prefetch = PrefetchBit_Yes;
  } else {
    prefetch = PrefetchBit_No;
  }
  la_t virtual_pc = pkt->req->getPC();
  int isPriv = false;  // TODO: get permission data
  int thread = pkt->req->threadId();

  AccessModeType access_mode = AccessModeType_UserMode; // TODO: get actual permission

  CacheMsg request(Address( physical_addr ),
                   Address( physical_addr ),
                   type_of_request,
                   Address(virtual_pc),
                   access_mode,   // User/supervisor mode
                   request_size,   // Size in bytes of request
                   prefetch,
                   0,              // Version number
                   Address(logical_addr),   // Virtual Address
                   thread         // SMT thread
                   );

  if ( TSO && write && !pkt->req->isPrefetch() ) {
    assert(m_chip_ptr->m_L1Cache_storeBuffer_vec[m_version]->isReady());
    m_chip_ptr->m_L1Cache_storeBuffer_vec[m_version]->insertStore(pkt, request);
    return;
  }

  m_packetTable_ptr->insert(Address( physical_addr ), pkt);

  doRequest(request);
}

bool Sequencer::doRequest(const CacheMsg& request) {
  bool hit = false;
  // Check the fast path
  DataBlock* data_ptr;

  int thread = request.getThreadID();

  hit = tryCacheAccess(line_address(request.getAddress()),
                       request.getType(),
                       request.getProgramCounter(),
                       request.getAccessMode(),
                       request.getSize(),
                       data_ptr);

  if (hit && (request.getType() == CacheRequestType_IFETCH || !REMOVE_SINGLE_CYCLE_DCACHE_FAST_PATH) ) {
    DEBUG_MSG(SEQUENCER_COMP, MedPrio, "Fast path hit");
    hitCallback(request, *data_ptr, GenericMachineType_L1Cache, thread);
    return true;
  }

  if (TSO && (request.getType() == CacheRequestType_LD || request.getType() == CacheRequestType_IFETCH)) {

    // See if we can satisfy the load entirely from the store buffer
    SubBlock subblock(line_address(request.getAddress()), request.getSize());
    if (m_chip_ptr->m_L1Cache_storeBuffer_vec[m_version]->trySubBlock(subblock)) {
      DataBlock dummy;
      hitCallback(request, dummy, GenericMachineType_NULL, thread);  // Call with an 'empty' datablock, since the data is in the store buffer
      return true;
    }
  }

  DEBUG_MSG(SEQUENCER_COMP, MedPrio, "Fast path miss");
  issueRequest(request);
  return hit;
}

void Sequencer::issueRequest(const CacheMsg& request) {
  bool found = insertRequest(request);

  if (!found) {
    CacheMsg msg = request;
    msg.getAddress() = line_address(request.getAddress()); // Make line address

    // Fast Path L1 misses are profiled here - all non-fast path misses are profiled within the generated protocol code
    if (!REMOVE_SINGLE_CYCLE_DCACHE_FAST_PATH) {
      g_system_ptr->getProfiler()->addPrimaryStatSample(msg, m_chip_ptr->getID());
    }

    if (PROTOCOL_DEBUG_TRACE) {
      g_system_ptr->getProfiler()->profileTransition("Seq", (m_chip_ptr->getID()*RubyConfig::numberOfProcsPerChip() + m_version), -1, msg.getAddress(),"", "Begin", "", CacheRequestType_to_string(request.getType()));
    }

#if 0
    // Commented out by nate binkert because I removed the trace stuff
    if (g_system_ptr->getTracer()->traceEnabled()) {
      g_system_ptr->getTracer()->traceRequest((m_chip_ptr->getID()*RubyConfig::numberOfProcsPerChip()+m_version), msg.getAddress(), msg.getProgramCounter(),
                                              msg.getType(), g_eventQueue_ptr->getTime());
    }
#endif

    Time latency = 0;  // initialzed to an null value

    latency = SEQUENCER_TO_CONTROLLER_LATENCY;

    // Send the message to the cache controller
    assert(latency > 0);
    m_chip_ptr->m_L1Cache_mandatoryQueue_vec[m_version]->enqueue(msg, latency);

  }  // !found
}

bool Sequencer::tryCacheAccess(const Address& addr, CacheRequestType type,
                               const Address& pc, AccessModeType access_mode,
                               int size, DataBlock*& data_ptr) {
  if (type == CacheRequestType_IFETCH) {
    if (Protocol::m_TwoLevelCache) {
      return m_chip_ptr->m_L1Cache_L1IcacheMemory_vec[m_version]->tryCacheAccess(line_address(addr), type, data_ptr);
    }
    else {
      return m_chip_ptr->m_L1Cache_cacheMemory_vec[m_version]->tryCacheAccess(line_address(addr), type, data_ptr);
    }
  } else {
    if (Protocol::m_TwoLevelCache) {
      return m_chip_ptr->m_L1Cache_L1DcacheMemory_vec[m_version]->tryCacheAccess(line_address(addr), type, data_ptr);
    }
    else {
      return m_chip_ptr->m_L1Cache_cacheMemory_vec[m_version]->tryCacheAccess(line_address(addr), type, data_ptr);
    }
  }
}

void Sequencer::resetRequestTime(const Address& addr, int thread){
  assert(thread >= 0);
  //reset both load and store requests, if they exist
  if(m_readRequestTable_ptr[thread]->exist(line_address(addr))){
    CacheMsg& request = m_readRequestTable_ptr[thread]->lookup(addr);
    if( request.m_AccessMode != AccessModeType_UserMode){
      cout << "resetRequestType ERROR read request addr = " << addr << " thread = "<< thread << " is SUPERVISOR MODE" << endl;
      printProgress(cout);
    }
    //ASSERT(request.m_AccessMode == AccessModeType_UserMode);
    request.setTime(g_eventQueue_ptr->getTime());
  }
  if(m_writeRequestTable_ptr[thread]->exist(line_address(addr))){
    CacheMsg& request = m_writeRequestTable_ptr[thread]->lookup(addr);
    if( request.m_AccessMode != AccessModeType_UserMode){
      cout << "resetRequestType ERROR write request addr = " << addr << " thread = "<< thread << " is SUPERVISOR MODE" << endl;
      printProgress(cout);
    }
    //ASSERT(request.m_AccessMode == AccessModeType_UserMode);
    request.setTime(g_eventQueue_ptr->getTime());
  }
}

// removes load request from queue
void Sequencer::removeLoadRequest(const Address & addr, int thread){
  removeRequest(getReadRequest(addr, thread));
}

void Sequencer::removeStoreRequest(const Address & addr, int thread){
  removeRequest(getWriteRequest(addr, thread));
}

// returns the read CacheMsg
CacheMsg & Sequencer::getReadRequest( const Address & addr, int thread ){
  Address temp = addr;
  assert(thread >= 0);
  assert(temp == line_address(temp));
  assert(m_readRequestTable_ptr[thread]->exist(addr));
  return m_readRequestTable_ptr[thread]->lookup(addr);
}

CacheMsg & Sequencer::getWriteRequest( const Address & addr, int thread){
  Address temp = addr;
  assert(thread >= 0);
  assert(temp == line_address(temp));
  assert(m_writeRequestTable_ptr[thread]->exist(addr));
  return m_writeRequestTable_ptr[thread]->lookup(addr);
}

void Sequencer::print(ostream& out) const {
  out << "[Sequencer: " << m_chip_ptr->getID()
      << ", outstanding requests: " << m_outstanding_count;

  int smt_threads = RubyConfig::numberofSMTThreads();
  for(int p=0; p < smt_threads; ++p){
    out << ", read request table[ " << p << " ]: " << *m_readRequestTable_ptr[p]
        << ", write request table[ " << p << " ]: " << *m_writeRequestTable_ptr[p];
  }
  out << "]";
}

// this can be called from setState whenever coherence permissions are upgraded
// when invoked, coherence violations will be checked for the given block
void Sequencer::checkCoherence(const Address& addr) {
#ifdef CHECK_COHERENCE
  g_system_ptr->checkGlobalCoherenceInvariant(addr);
#endif
}

bool Sequencer::getRubyMemoryValue(const Address& addr, char* value,
                                   unsigned int size_in_bytes ) {
  for(unsigned int i=0; i < size_in_bytes; i++) {
    std::cerr << __FILE__ << "(" << __LINE__ << "): Not implemented. " << std::endl;
    value[i] = 0; // _read_physical_memory( m_chip_ptr->getID()*RubyConfig::numberOfProcsPerChip()+m_version,
                  //                          addr.getAddress() + i, 1 );
  }
  return false; // Do nothing?
}

bool Sequencer::setRubyMemoryValue(const Address& addr, char *value,
                                   unsigned int size_in_bytes) {
  char test_buffer[64];

  return false; // Do nothing?
}

