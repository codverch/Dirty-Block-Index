
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
 * RubySystem.cc
 *
 * Description: See System.hh
 *
 * $Id$
 *
 */


#include "mem/ruby/system/System.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/network/Network.hh"
#include "mem/ruby/recorder/Tracer.hh"
#include "mem/protocol/Protocol.hh"
#include "mem/ruby/buffers/MessageBuffer.hh"
#include "mem/ruby/system/Sequencer.hh"
#include "mem/ruby/system/DMASequencer.hh"
#include "mem/ruby/system/MemoryVector.hh"
#include "mem/ruby/slicc_interface/AbstractController.hh"
#include "mem/ruby/system/CacheMemory.hh"
#include "mem/ruby/system/DirectoryMemory.hh"
#include "mem/ruby/network/simple/Topology.hh"
#include "mem/ruby/network/simple/SimpleNetwork.hh"
#include "mem/ruby/system/RubyPort.hh"
//#include "mem/ruby/network/garnet-flexible-pipeline/GarnetNetwork.hh"
//#include "mem/ruby/network/garnet-fixed-pipeline/GarnetNetwork_d.hh"
#include "mem/ruby/system/MemoryControl.hh"

int RubySystem::m_random_seed;
bool RubySystem::m_randomization;
int RubySystem::m_tech_nm;
int RubySystem::m_freq_mhz;
int RubySystem::m_block_size_bytes;
int RubySystem::m_block_size_bits;
uint64 RubySystem::m_memory_size_bytes;
int RubySystem::m_memory_size_bits;

map< string, RubyPort* > RubySystem::m_ports;
map< string, CacheMemory* > RubySystem::m_caches;
map< string, DirectoryMemory* > RubySystem::m_directories;
map< string, Sequencer* > RubySystem::m_sequencers;
map< string, DMASequencer* > RubySystem::m_dma_sequencers;
map< string, AbstractController* > RubySystem::m_controllers;
map< string, MemoryControl* > RubySystem::m_memorycontrols;


Network* RubySystem::m_network_ptr;
map< string, Topology*> RubySystem::m_topologies;
Profiler* RubySystem::m_profiler_ptr;
Tracer* RubySystem::m_tracer_ptr;

MemoryVector* RubySystem::m_mem_vec_ptr;


RubySystem::RubySystem(const Params *p)
    : SimObject(p)
{
    if (g_system_ptr != NULL)
        fatal("Only one RubySystem object currently allowed.\n");

    m_random_seed = p->random_seed;
    srandom(m_random_seed);
    m_randomization = p->randomization;
    m_tech_nm = p->tech_nm;
    m_freq_mhz = p->freq_mhz;
    m_block_size_bytes = p->block_size_bytes;
    assert(is_power_of_2(m_block_size_bytes));
    m_block_size_bits = log_int(m_block_size_bytes);
    m_network_ptr = p->network;
    g_debug_ptr = p->debug;
    m_profiler_ptr = p->profiler;
    m_tracer_ptr = p->tracer;

    g_system_ptr = this;
    m_mem_vec_ptr = new MemoryVector;
}


void RubySystem::init()
{
  // calculate system-wide parameters
  m_memory_size_bytes = 0;
  DirectoryMemory* prev = NULL;
  for (map< string, DirectoryMemory*>::const_iterator it = m_directories.begin();
       it != m_directories.end(); it++) {
    if (prev != NULL)
      assert((*it).second->getSize() == prev->getSize()); // must be equal for proper address mapping
    m_memory_size_bytes += (*it).second->getSize();
    prev = (*it).second;
  }
  m_mem_vec_ptr->setSize(m_memory_size_bytes);
  m_memory_size_bits = log_int(m_memory_size_bytes);
}


RubySystem::~RubySystem()
{

}

void RubySystem::printSystemConfig(ostream & out)
{
  out << "RubySystem config:" << endl;
  out << "  random_seed: " << m_random_seed << endl;
  out << "  randomization: " << m_randomization << endl;
  out << "  tech_nm: " << m_tech_nm << endl;
  out << "  freq_mhz: " << m_freq_mhz << endl;
  out << "  block_size_bytes: " << m_block_size_bytes << endl;
  out << "  block_size_bits: " << m_block_size_bits << endl;
  out << "  memory_size_bytes: " << m_memory_size_bytes << endl;
  out << "  memory_size_bits: " << m_memory_size_bits << endl;

}

void RubySystem::printConfig(ostream& out)
{
  out << "\n================ Begin RubySystem Configuration Print ================\n\n";
  printSystemConfig(out);
  for (map<string, AbstractController*>::const_iterator it = m_controllers.begin();
       it != m_controllers.end(); it++) {
    (*it).second->printConfig(out);
  }
  for (map<string, CacheMemory*>::const_iterator it = m_caches.begin();
       it != m_caches.end(); it++) {
    (*it).second->printConfig(out);
  }
  DirectoryMemory::printGlobalConfig(out);
  for (map<string, DirectoryMemory*>::const_iterator it = m_directories.begin();
       it != m_directories.end(); it++) {
    (*it).second->printConfig(out);
  }
  for (map<string, Sequencer*>::const_iterator it = m_sequencers.begin();
       it != m_sequencers.end(); it++) {
    (*it).second->printConfig(out);
  }

  m_network_ptr->printConfig(out);
  m_profiler_ptr->printConfig(out);

  out << "\n================ End RubySystem Configuration Print ================\n\n";
}

void RubySystem::printStats(ostream& out)
{

  const time_t T = time(NULL);
  tm *localTime = localtime(&T);
  char buf[100];
  strftime(buf, 100, "%b/%d/%Y %H:%M:%S", localTime);

  out << "Real time: " << buf << endl;

  m_profiler_ptr->printStats(out);
  m_network_ptr->printStats(out);
  for (map<string, Sequencer*>::const_iterator it = m_sequencers.begin();
       it != m_sequencers.end(); it++) {
    (*it).second->printStats(out);
  }
  for (map<string, CacheMemory*>::const_iterator it = m_caches.begin();
       it != m_caches.end(); it++) {
    (*it).second->printStats(out);
  }
  for (map<string, AbstractController*>::const_iterator it = m_controllers.begin();
       it != m_controllers.end(); it++) {
    (*it).second->printStats(out);
  }
}

void RubySystem::clearStats() const
{
  m_profiler_ptr->clearStats();
  m_network_ptr->clearStats();
  for (map<string, CacheMemory*>::const_iterator it = m_caches.begin();
       it != m_caches.end(); it++) {
    (*it).second->clearStats();
  }
  for (map<string, AbstractController*>::const_iterator it = m_controllers.begin();
       it != m_controllers.end(); it++) {
    (*it).second->clearStats();
  }
}

void RubySystem::recordCacheContents(CacheRecorder& tr) const
{

}

#ifdef CHECK_COHERENCE
// This code will check for cases if the given cache block is exclusive in
// one node and shared in another-- a coherence violation
//
// To use, the SLICC specification must call sequencer.checkCoherence(address)
// when the controller changes to a state with new permissions.  Do this
// in setState.  The SLICC spec must also define methods "isBlockShared"
// and "isBlockExclusive" that are specific to that protocol
//
void RubySystem::checkGlobalCoherenceInvariant(const Address& addr  )  {
  /*
  NodeID exclusive = -1;
  bool sharedDetected = false;
  NodeID lastShared = -1;

  for (int i = 0; i < m_chip_vector.size(); i++) {

    if (m_chip_vector[i]->isBlockExclusive(addr)) {
      if (exclusive != -1) {
        // coherence violation
        WARN_EXPR(exclusive);
        WARN_EXPR(m_chip_vector[i]->getID());
        WARN_EXPR(addr);
        WARN_EXPR(g_eventQueue_ptr->getTime());
        ERROR_MSG("Coherence Violation Detected -- 2 exclusive chips");
      }
      else if (sharedDetected) {
        WARN_EXPR(lastShared);
        WARN_EXPR(m_chip_vector[i]->getID());
        WARN_EXPR(addr);
        WARN_EXPR(g_eventQueue_ptr->getTime());
        ERROR_MSG("Coherence Violation Detected -- exclusive chip with >=1 shared");
      }
      else {
        exclusive = m_chip_vector[i]->getID();
      }
    }
    else if (m_chip_vector[i]->isBlockShared(addr)) {
      sharedDetected = true;
      lastShared = m_chip_vector[i]->getID();

      if (exclusive != -1) {
        WARN_EXPR(lastShared);
        WARN_EXPR(exclusive);
        WARN_EXPR(addr);
        WARN_EXPR(g_eventQueue_ptr->getTime());
        ERROR_MSG("Coherence Violation Detected -- exclusive chip with >=1 shared");
      }
    }
  }
  */
}
#endif


RubySystem *
RubySystemParams::create()
{
    return new RubySystem(this);
}
