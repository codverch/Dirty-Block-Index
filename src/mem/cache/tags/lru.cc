/*
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
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
 * Authors: Erik Hallnor
 */

/**
 * @file
 * Definitions of LRU tag store.
 */

#include <string>

#include "mem/cache/base.hh"
#include "base/intmath.hh"
#include "mem/cache/tags/lru.hh"
#include "sim/core.hh"

using namespace std;

LRUBlk*
CacheSet::findBlk(Addr tag) const
{
    for (int i = 0; i < assoc; ++i) {
        if (blks[i]->tag == tag && blks[i]->isValid()) {
            return blks[i];
        }
    }
    return 0;
}


void
CacheSet::moveToHead(LRUBlk *blk)
{
    // nothing to do if blk is already head
    if (blks[0] == blk)
        return;

    // write 'next' block into blks[i], moving up from MRU toward LRU
    // until we overwrite the block we moved to head.

    // start by setting up to write 'blk' into blks[0]
    int i = 0;
    LRUBlk *next = blk;

    do {
        assert(i < assoc);
        // swap blks[i] and next
        LRUBlk *tmp = blks[i];
        blks[i] = next;
        next = tmp;
        ++i;
    } while (next != blk);
}


// create and initialize a LRU/MRU cache structure
LRU::LRU(int _numSets, int _blkSize, int _assoc, int _hit_latency) :
    numSets(_numSets), blkSize(_blkSize), assoc(_assoc), hitLatency(_hit_latency)
{
    // Check parameters
    if (blkSize < 4 || !isPowerOf2(blkSize)) {
        fatal("Block size must be at least 4 and a power of 2");
    }
    if (numSets <= 0 || !isPowerOf2(numSets)) {
        fatal("# of sets must be non-zero and a power of 2");
    }
    if (assoc <= 0) {
        fatal("associativity must be greater than zero");
    }
    if (hitLatency <= 0) {
        fatal("access latency must be greater than zero");
    }

    LRUBlk  *blk;
    int i, j, blkIndex;

    blkMask = blkSize - 1;
    setShift = floorLog2(blkSize);
    setMask = numSets - 1;
    tagShift = setShift + floorLog2(numSets);
    warmedUp = false;
    /** @todo Make warmup percentage a parameter. */
    warmupBound = numSets * assoc;

    sets = new CacheSet[numSets];
    blks = new LRUBlk[numSets * assoc];
    // allocate data storage in one big chunk
    dataBlks = new uint8_t[numSets*assoc*blkSize];

    blkIndex = 0;       // index into blks array
    for (i = 0; i < numSets; ++i) {
        sets[i].assoc = assoc;

        sets[i].blks = new LRUBlk*[assoc];

        // link in the data blocks
        for (j = 0; j < assoc; ++j) {
            // locate next cache block
            blk = &blks[blkIndex];
            blk->data = &dataBlks[blkSize*blkIndex];
            ++blkIndex;

            // invalidate new cache block
            blk->status = 0;

            //EGH Fix Me : do we need to initialize blk?

            // Setting the tag to j is just to prevent long chains in the hash
            // table; won't matter because the block is invalid
            blk->tag = j;
            blk->whenReady = 0;
            blk->isTouched = false;
            blk->size = blkSize;
            sets[i].blks[j]=blk;
            blk->set = i;
        }
    }
}

LRU::~LRU()
{
    delete [] dataBlks;
    delete [] blks;
    delete [] sets;
}

LRUBlk*
LRU::accessBlock(Addr addr, int &lat)
{
    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    LRUBlk *blk = sets[set].findBlk(tag);
    lat = hitLatency;
    if (blk != NULL) {
        // move this block to head of the MRU list
        sets[set].moveToHead(blk);
        DPRINTF(CacheRepl, "set %x: moving blk %x to MRU\n",
                set, regenerateBlkAddr(tag, set));
        if (blk->whenReady > curTick
            && blk->whenReady - curTick > hitLatency) {
            lat = blk->whenReady - curTick;
        }
        blk->refCount += 1;
    }

    return blk;
}


LRUBlk*
LRU::findBlock(Addr addr) const
{
    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    LRUBlk *blk = sets[set].findBlk(tag);
    return blk;
}

LRUBlk*
LRU::findVictim(Addr addr, PacketList &writebacks)
{
    unsigned set = extractSet(addr);
    // grab a replacement candidate
    LRUBlk *blk = sets[set].blks[assoc-1];
    if (blk->isValid()) {
        replacements[0]++;
        totalRefs += blk->refCount;
        ++sampledRefs;
        blk->refCount = 0;

        DPRINTF(CacheRepl, "set %x: selecting blk %x for replacement\n",
                set, regenerateBlkAddr(blk->tag, set));
    }
    return blk;
}

void
LRU::insertBlock(Addr addr, LRU::BlkType *blk)
{
    if (!blk->isTouched) {
        tagsInUse++;
        blk->isTouched = true;
        if (!warmedUp && tagsInUse.value() >= warmupBound) {
            warmedUp = true;
            warmupCycle = curTick;
        }
    }

    // Set tag for new block.  Caller is responsible for setting status.
    blk->tag = extractTag(addr);

    unsigned set = extractSet(addr);
    sets[set].moveToHead(blk);
}

void
LRU::invalidateBlk(LRU::BlkType *blk)
{
    if (blk) {
        blk->status = 0;
        blk->isTouched = false;
        blk->clearLoadLocks();
        tagsInUse--;
    }
}

void
LRU::cleanupRefs()
{
    for (int i = 0; i < numSets*assoc; ++i) {
        if (blks[i].isValid()) {
            totalRefs += blks[i].refCount;
            ++sampledRefs;
        }
    }
}
