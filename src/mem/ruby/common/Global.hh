
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
 * $Id$
 *
 * */

#ifndef GLOBAL_H
#define GLOBAL_H

#ifdef SINGLE_LEVEL_CACHE
const bool TWO_LEVEL_CACHE = false;
#define L1I_CACHE_MEMBER_VARIABLE m_L1Cache_cacheMemory_vec[m_version] // currently all protocols require L1s == nodes
#define L1D_CACHE_MEMBER_VARIABLE m_L1Cache_cacheMemory_vec[m_version] // "
#define L2_CACHE_MEMBER_VARIABLE m_L1Cache_cacheMemory_vec[m_version] // "
#define L2_CACHE_VARIABLE m_L1Cache_cacheMemory_vec
#else
const bool TWO_LEVEL_CACHE = true;
#ifdef IS_CMP
#define L1I_CACHE_MEMBER_VARIABLE m_L1Cache_L1IcacheMemory_vec[m_version]
#define L1D_CACHE_MEMBER_VARIABLE m_L1Cache_L1DcacheMemory_vec[m_version]
#define L2_CACHE_MEMBER_VARIABLE m_L2Cache_L2cacheMemory_vec[m_version]
#define L2_CACHE_VARIABLE m_L2Cache_L2cacheMemory_vec
#else  // not IS_CMP
#define L1I_CACHE_MEMBER_VARIABLE m_L1Cache_L1IcacheMemory_vec[m_version] // currently all protocols require L1s == nodes
#define L1D_CACHE_MEMBER_VARIABLE m_L1Cache_L1DcacheMemory_vec[m_version] // "
// #define L2_CACHE_MEMBER_VARIABLE m_L1Cache_L2cacheMemory_vec[m_version] // old exclusive caches don't support L2s != nodes
#define L2_CACHE_MEMBER_VARIABLE m_L1Cache_cacheMemory_vec[m_version] // old exclusive caches don't support L2s != nodes
#define L2_CACHE_VARIABLE m_L1Cache_L2cacheMemory_vec
#endif // IS_CMP
#endif //SINGLE_LEVEL_CACHE

#define DIRECTORY_MEMBER_VARIABLE m_Directory_directory_vec[m_version]
#define TBE_TABLE_MEMBER_VARIABLE m_L1Cache_TBEs_vec[m_version]

typedef unsigned char uint8;
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef signed char int8;
typedef int int32;
typedef long long int64;

typedef long long integer_t;
typedef unsigned long long uinteger_t;

typedef int64 Time;
typedef uint64 physical_address_t;
typedef uint64 la_t;
typedef uint64 pa_t;
typedef integer_t simtime_t;

// external includes for all classes
#include "mem/gems_common/std-includes.hh"
#include "mem/ruby/common/Debug.hh"

// simple type declarations
typedef Time LogicalTime;
typedef int64 Index;            // what the address bit ripper returns
typedef int word;               // one word of a cache line
typedef int SwitchID;
typedef int LinkID;

class RubyEventQueue;
extern RubyEventQueue* g_eventQueue_ptr;

class RubySystem;
extern RubySystem* g_system_ptr;

class Debug;
extern Debug* g_debug_ptr;

// FIXME:  this is required by the contructor of Directory_Entry.h.  It can't go
// into slicc_util.h because it opens a can of ugly worms
extern inline int max_tokens()
{
  return 1024;
}


#endif //GLOBAL_H

