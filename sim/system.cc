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

#include "cpu/exec_context.hh"
#include "targetarch/vtophys.hh"
#include "sim/param.hh"
#include "sim/system.hh"

using namespace std;

vector<System *> System::systemList;

int System::numSystemsRunning = 0;

System::System(const std::string _name,
               const int _init_param,
               MemoryController *_memCtrl,
               PhysicalMemory *_physmem)
    : SimObject(_name),
      init_param(_init_param),
      memCtrl(_memCtrl),
      physmem(_physmem)
{
    // add self to global system list
    systemList.push_back(this);
}


System::~System()
{
}


int
System::registerExecContext(ExecContext *xc)
{
    int myIndex = execContexts.size();
    execContexts.push_back(xc);
    return myIndex;
}


void
System::replaceExecContext(int xcIndex, ExecContext *xc)
{
    if (xcIndex >= execContexts.size()) {
        panic("replaceExecContext: bad xcIndex, %d >= %d\n",
              xcIndex, execContexts.size());
    }

    execContexts[xcIndex] = xc;
}


void
System::printSystems()
{
    vector<System *>::iterator i = systemList.begin();
    vector<System *>::iterator end = systemList.end();
    for (; i != end; ++i) {
        System *sys = *i;
        cerr << "System " << sys->name() << ": " << hex << sys << endl;
    }
}

extern "C"
void
printSystems()
{
    System::printSystems();
}

DEFINE_SIM_OBJECT_CLASS_NAME("System", System)

