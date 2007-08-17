/*
 * Copyright (c) 2006 The Regents of The University of Michigan
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
 * Authors: Nathan Binkert
 */

#include <Python.h>

#include <string>

#include "base/inifile.hh"
#include "base/output.hh"
#include "config/full_system.hh"
#include "mem/mem_object.hh"
#include "mem/port.hh"
#include "sim/sim_object.hh"

#if FULL_SYSTEM
#include "dev/etherdevice.hh"
#include "dev/etherobject.hh"
#include "dev/etherint.hh"
#endif

using namespace std;

/**
 * Look up a MemObject port.  Helper function for connectPorts().
 */
Port *
lookupPort(SimObject *so, const std::string &name, int i)
{
    MemObject *mo = dynamic_cast<MemObject *>(so);
    if (mo == NULL) {
        warn("error casting SimObject %s to MemObject", so->name());
        return NULL;
    }

    Port *p = mo->getPort(name, i);
    if (p == NULL)
        warn("error looking up port %s on object %s", name, so->name());
    return p;
}

#if FULL_SYSTEM

EtherInt *
lookupEthPort(SimObject *so, const std::string &name, int i)
{
    EtherObject *eo = dynamic_cast<EtherObject *>(so);
    EtherDevice *ed = dynamic_cast<EtherDevice *>(so);
    if (eo == NULL && ed == NULL) {
        warn("error casting SimObject %s", so->name());
        return NULL;
    }

    EtherInt *p = NULL;
    if (eo)
        p = eo->getEthPort(name, i);
    else
        p = ed->getEthPort(name, i);
    return p;
}
#endif

/**
 * Connect the described MemObject ports.  Called from Python via SWIG.
 * The indices i1 & i2 will be -1 for regular ports, >= 0 for vector ports.
 */
int
connectPorts(SimObject *o1, const std::string &name1, int i1,
             SimObject *o2, const std::string &name2, int i2)
{
    MemObject *mo1, *mo2;
    mo1 = dynamic_cast<MemObject*>(o1);
    mo2 = dynamic_cast<MemObject*>(o2);

#if FULL_SYSTEM
    EtherObject *eo1, *eo2;
    EtherDevice *ed1, *ed2;
    eo1 = dynamic_cast<EtherObject*>(o1);
    ed1 = dynamic_cast<EtherDevice*>(o1);

    eo2 = dynamic_cast<EtherObject*>(o2);
    ed2 = dynamic_cast<EtherDevice*>(o2);

    if ((eo1 || ed1) && (eo2 || ed2)) {
        EtherInt *p1 = lookupEthPort(o1, name1, i1);
        EtherInt *p2 = lookupEthPort(o2, name2, i2);

        if (p1 != NULL &&  p2 != NULL) {

            p1->setPeer(p2);
            p2->setPeer(p1);

            return 1;
        }
    }
#endif
    Port *p1 = lookupPort(o1, name1, i1);
    Port *p2 = lookupPort(o2, name2, i2);

    if (p1 == NULL || p2 == NULL) {
        warn("connectPorts: port lookup error");
        return 0;
    }

    p1->setPeer(p2);
    p2->setPeer(p1);

    return 1;
}

inline IniFile &
inifile()
{
    static IniFile inifile;
    return inifile;
}

/**
 * Pointer to the Python function that maps names to SimObjects.
 */
PyObject *resolveFunc = NULL;

/**
 * Convert a pointer to the Python object that SWIG wraps around a C++
 * SimObject pointer back to the actual C++ pointer.  See main.i.
 */
extern "C" SimObject *convertSwigSimObjectPtr(PyObject *);

SimObject *
resolveSimObject(const string &name)
{
    PyObject *pyPtr = PyEval_CallFunction(resolveFunc, "(s)", name.c_str());
    if (pyPtr == NULL) {
        PyErr_Print();
        panic("resolveSimObject: failure on call to Python for %s", name);
    }

    SimObject *simObj = convertSwigSimObjectPtr(pyPtr);
    if (simObj == NULL)
        panic("resolveSimObject: failure on pointer conversion for %s", name);

    return simObj;
}

/**
 * Load config.ini into C++ database.  Exported to Python via SWIG;
 * invoked from m5.instantiate().
 */
void
loadIniFile(PyObject *_resolveFunc)
{
    resolveFunc = _resolveFunc;

    // The configuration database is now complete; start processing it.
    inifile().load(simout.resolve("config.ini"));
}

