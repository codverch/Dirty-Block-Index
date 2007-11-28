/*
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
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
 * Authors: Steve Reinhardt
 */

/**
 * @file
 * Port object definitions.
 */
#include <cstring>

#include "base/chunk_generator.hh"
#include "base/trace.hh"
#include "mem/mem_object.hh"
#include "mem/port.hh"

class defaultPeerPortClass: public Port
{
  protected:
    void blowUp()
    {
        fatal("Unconnected port!");
    }

  public:
    defaultPeerPortClass() : Port("default_port")
    {}

    bool recvTiming(PacketPtr)
    {
        blowUp();
        return false;
    }

    Tick recvAtomic(PacketPtr)
    {
        blowUp();
        return 0;
    }

    void recvFunctional(PacketPtr)
    {
        blowUp();
    }

    void recvStatusChange(Status)
    {
        blowUp();
    }

    int deviceBlockSize()
    {
        blowUp();
        return 0;
    }

    void getDeviceAddressRanges(AddrRangeList &, bool &)
    {
        blowUp();
    }

    bool isDefaultPort() { return true; }

} defaultPeerPort;

Port::Port() : peer(&defaultPeerPort), owner(NULL)
{
}

Port::Port(const std::string &_name, MemObject *_owner) :
    portName(_name), peer(&defaultPeerPort), owner(_owner)
{
}

void
Port::setPeer(Port *port)
{
    DPRINTF(Config, "setting peer to %s\n", port->name());
    peer = port;
}

void
Port::removeConn()
{
    if (peer->getOwner())
        peer->getOwner()->deletePortRefs(peer);
    peer = NULL;
}

void
Port::blobHelper(Addr addr, uint8_t *p, int size, MemCmd cmd)
{
    Request req;

    for (ChunkGenerator gen(addr, size, peerBlockSize());
         !gen.done(); gen.next()) {
        req.setPhys(gen.addr(), gen.size(), 0);
        Packet pkt(&req, cmd, Packet::Broadcast);
        pkt.dataStatic(p);
        sendFunctional(&pkt);
        p += gen.size();
    }
}

void
Port::writeBlob(Addr addr, uint8_t *p, int size)
{
    blobHelper(addr, p, size, MemCmd::WriteReq);
}

void
Port::readBlob(Addr addr, uint8_t *p, int size)
{
    blobHelper(addr, p, size, MemCmd::ReadReq);
}

void
Port::memsetBlob(Addr addr, uint8_t val, int size)
{
    // quick and dirty...
    uint8_t *buf = new uint8_t[size];

    std::memset(buf, val, size);
    blobHelper(addr, buf, size, MemCmd::WriteReq);

    delete [] buf;
}
