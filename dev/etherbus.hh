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

/* @file
 * Device module for modelling an ethernet hub
 */

#ifndef __ETHERBUS_H__
#define __ETHERBUS_H__

#include "eventq.hh"
#include "etherpkt.hh"
#include "sim_object.hh"

class EtherDump;
class EtherInt;
class EtherBus : public SimObject
{
  protected:
    typedef std::list<EtherInt *> devlist_t;
    devlist_t devlist;
    double ticks_per_byte;
    bool loopback;

  protected:
    class DoneEvent : public Event
    {
      protected:
        EtherBus *bus;

      public:
        DoneEvent(EventQueue *q, EtherBus *b)
            : Event(q), bus(b) {}
        virtual void process() { bus->txDone(); }
        virtual const char *description() { return "ethernet bus completion"; }
    };

    DoneEvent event;
    PacketPtr packet;
    EtherInt *sender;
    EtherDump *dump;

  public:
    EtherBus(const std::string &name, double ticks_per_byte, bool loopback,
             EtherDump *dump);
    virtual ~EtherBus() {}

    void txDone();
    void reg(EtherInt *dev);
    bool busy() const { return (bool)packet; }
    bool send(EtherInt *sender, PacketPtr packet);
};

#endif // __ETHERBUS_H__
