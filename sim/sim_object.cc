/*
 * Copyright (c) 2003 The Regents of The University of Michigan
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

#include <assert.h>

#include "base/callback.hh"
#include "base/inifile.hh"
#include "base/misc.hh"
#include "base/trace.hh"
#include "sim/configfile.hh"
#include "sim/host.hh"
#include "sim/sim_object.hh"
#include "sim/sim_stats.hh"

using namespace std;


////////////////////////////////////////////////////////////////////////
//
// SimObject member definitions
//
////////////////////////////////////////////////////////////////////////

//
// static list of all SimObjects, used for initialization etc.
//
SimObject::SimObjectList SimObject::simObjectList;

//
// SimObject constructor: used to maintain static simObjectList
//
SimObject::SimObject(const string &_name)
    : objName(_name)
{
    simObjectList.push_back(this);
}

//
// no default statistics, so nothing to do in base implementation
//
void
SimObject::regStats()
{
}

void
SimObject::regFormulas()
{
}

void
SimObject::resetStats()
{
}

//
// no default extra output
//
void
SimObject::printExtraOutput(ostream &os)
{
}

//
// static function:
//   call regStats() on all SimObjects and then regFormulas() on all
//   SimObjects.
//
struct SimObjectResetCB : public Callback
{
    virtual void process() { SimObject::resetAllStats(); }
};

namespace {
    static SimObjectResetCB StatResetCB;
}

void
SimObject::regAllStats()
{
    SimObjectList::iterator i;
    SimObjectList::iterator end = simObjectList.end();

    /**
     * @todo change cprintfs to DPRINTFs
     */
    for (i = simObjectList.begin(); i != end; ++i) {
#ifdef STAT_DEBUG
        cprintf("registering stats for %s\n", (*i)->name());
#endif
        (*i)->regStats();
    }

    for (i = simObjectList.begin(); i != end; ++i) {
#ifdef STAT_DEBUG
        cprintf("registering formulas for %s\n", (*i)->name());
#endif
        (*i)->regFormulas();
    }

    Statistics::registerResetCallback(&StatResetCB);
}

//
// static function: call resetStats() on all SimObjects.
//
void
SimObject::resetAllStats()
{
    SimObjectList::iterator i = simObjectList.begin();
    SimObjectList::iterator end = simObjectList.end();

    for (; i != end; ++i) {
        SimObject *obj = *i;
        obj->resetStats();
    }
}

//
// static function: call printExtraOutput() on all SimObjects.
//
void
SimObject::printAllExtraOutput(ostream &os)
{
    SimObjectList::iterator i = simObjectList.begin();
    SimObjectList::iterator end = simObjectList.end();

    for (; i != end; ++i) {
        SimObject *obj = *i;
        obj->printExtraOutput(os);
   }
}
