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

#include "python/swig/pyevent.hh"
#include "sim/async.hh"

PythonEvent::PythonEvent(PyObject *obj, Priority priority)
    : Event(priority), object(obj)
{
    if (object == NULL)
        panic("Passed in invalid object");

    Py_INCREF(object);

    setFlags(AutoDelete);
}

PythonEvent::~PythonEvent()
{
    Py_DECREF(object);
}

void
PythonEvent::process()
{
    PyObject *args = PyTuple_New(0);
    PyObject *result = PyObject_Call(object, args, NULL);
    Py_DECREF(args);

    if (result) {
        // Nothing to do just decrement the reference count
        Py_DECREF(result);
    } else {
        // Somethign should be done to signal back to the main interpreter
        // that there's been an exception.
        async_event = true;
        async_exception = true;
    }
}

Event *
createCountedDrain()
{
    return new CountedDrainEvent();
}

void
cleanupCountedDrain(Event *counted_drain)
{
    CountedDrainEvent *event =
        dynamic_cast<CountedDrainEvent *>(counted_drain);
    if (event == NULL) {
        fatal("Called cleanupCountedDrain() on an event that was not "
              "a CountedDrainEvent.");
    }
    assert(event->getCount() == 0);
    delete event;
}

#if 0
Event *
create(PyObject *object, Event::Priority priority)
{
    return new PythonEvent(object, priority);
}

void
destroy(Event *event)
{
    delete event;
}
#endif
