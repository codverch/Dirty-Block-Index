/*
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
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

/** @file
 * Emulation of the Tsunami CChip CSRs
 */

#include <deque>
#include <string>
#include <vector>

#include "arch/alpha/ev5.hh"
#include "base/trace.hh"
#include "dev/tsunami_cchip.hh"
#include "dev/tsunamireg.h"
#include "dev/tsunami.hh"
#include "mem/port.hh"
#include "cpu/exec_context.hh"
#include "cpu/intr_control.hh"
#include "sim/builder.hh"
#include "sim/system.hh"

using namespace std;
//Should this be AlphaISA?
using namespace TheISA;

TsunamiCChip::TsunamiCChip(Params *p)
    : BasicPioDevice(p), tsunami(p->tsunami)
{
    pioSize = 0xfffffff;

    drir = 0;
    ipint = 0;
    itint = 0;

    for (int x = 0; x < Tsunami::Max_CPUs; x++)
    {
        dim[x] = 0;
        dir[x] = 0;
    }

    //Put back pointer in tsunami
    tsunami->cchip = this;
}

Tick
TsunamiCChip::read(Packet &pkt)
{
    DPRINTF(Tsunami, "read  va=%#x size=%d\n", pkt.addr, pkt.size);

    assert(pkt.result == Unknown);
    assert(pkt.addr > pioAddr && pkt.addr < pioAddr + pioSize);

    pkt.time = curTick + pioDelay;
    Addr regnum = (pkt.addr - pioAddr) >> 6;
    Addr daddr = (pkt.addr - pioAddr);

    uint64_t *data64;

    switch (pkt.size) {

      case sizeof(uint64_t):
          if (!pkt.data) {
              data64 = new uint64_t;
              pkt.data = (uint8_t*)data64;
          } else
              data64 = (uint64_t*)pkt.data;

          if (daddr & TSDEV_CC_BDIMS)
          {
              *data64 = dim[(daddr >> 4) & 0x3F];
              break;
          }

          if (daddr & TSDEV_CC_BDIRS)
          {
              *data64 = dir[(daddr >> 4) & 0x3F];
              break;
          }

          switch(regnum) {
              case TSDEV_CC_CSR:
                  *data64 = 0x0;
                  break;
              case TSDEV_CC_MTR:
                  panic("TSDEV_CC_MTR not implemeted\n");
                   break;
              case TSDEV_CC_MISC:
                  *data64 = (ipint << 8) & 0xF | (itint << 4) & 0xF |
                                     (pkt.req->getCpuNum() & 0x3);
                  break;
              case TSDEV_CC_AAR0:
              case TSDEV_CC_AAR1:
              case TSDEV_CC_AAR2:
              case TSDEV_CC_AAR3:
                  *data64 = 0;
                  break;
              case TSDEV_CC_DIM0:
                  *data64 = dim[0];
                  break;
              case TSDEV_CC_DIM1:
                  *data64 = dim[1];
                  break;
              case TSDEV_CC_DIM2:
                  *data64 = dim[2];
                  break;
              case TSDEV_CC_DIM3:
                  *data64 = dim[3];
                  break;
              case TSDEV_CC_DIR0:
                  *data64 = dir[0];
                  break;
              case TSDEV_CC_DIR1:
                  *data64 = dir[1];
                  break;
              case TSDEV_CC_DIR2:
                  *data64 = dir[2];
                  break;
              case TSDEV_CC_DIR3:
                  *data64 = dir[3];
                  break;
              case TSDEV_CC_DRIR:
                  *data64 = drir;
                  break;
              case TSDEV_CC_PRBEN:
                  panic("TSDEV_CC_PRBEN not implemented\n");
                  break;
              case TSDEV_CC_IIC0:
              case TSDEV_CC_IIC1:
              case TSDEV_CC_IIC2:
              case TSDEV_CC_IIC3:
                  panic("TSDEV_CC_IICx not implemented\n");
                  break;
              case TSDEV_CC_MPR0:
              case TSDEV_CC_MPR1:
              case TSDEV_CC_MPR2:
              case TSDEV_CC_MPR3:
                  panic("TSDEV_CC_MPRx not implemented\n");
                  break;
              case TSDEV_CC_IPIR:
                  *data64 = ipint;
                  break;
              case TSDEV_CC_ITIR:
                  *data64 = itint;
                  break;
              default:
                  panic("default in cchip read reached, accessing 0x%x\n");
           } // uint64_t

      break;
      case sizeof(uint32_t):
      case sizeof(uint16_t):
      case sizeof(uint8_t):
      default:
        panic("invalid access size(?) for tsunami register!\n");
    }
    DPRINTFN("Tsunami CChip: read  regnum=%#x size=%d data=%lld\n", regnum,
            pkt.size, *data64);

    pkt.result = Success;
    return pioDelay;
}

Tick
TsunamiCChip::write(Packet &pkt)
{
    pkt.time = curTick + pioDelay;


    assert(pkt.addr >= pioAddr && pkt.addr < pioAddr + pioSize);
    Addr daddr = pkt.addr - pioAddr;
    Addr regnum = (pkt.addr - pioAddr) >> 6 ;


    uint64_t val = *(uint64_t *)pkt.data;
    assert(pkt.size == sizeof(uint64_t));

    DPRINTF(Tsunami, "write - addr=%#x value=%#x\n", pkt.addr, val);

    bool supportedWrite = false;


    if (daddr & TSDEV_CC_BDIMS)
    {
        int number = (daddr >> 4) & 0x3F;

        uint64_t bitvector;
        uint64_t olddim;
        uint64_t olddir;

        olddim = dim[number];
        olddir = dir[number];
        dim[number] = val;
        dir[number] = dim[number] & drir;
        for(int x = 0; x < Tsunami::Max_CPUs; x++)
        {
            bitvector = ULL(1) << x;
            // Figure out which bits have changed
            if ((dim[number] & bitvector) != (olddim & bitvector))
            {
                // The bit is now set and it wasn't before (set)
                if((dim[number] & bitvector) && (dir[number] & bitvector))
                {
                    tsunami->intrctrl->post(number, TheISA::INTLEVEL_IRQ1, x);
                    DPRINTF(Tsunami, "dim write resulting in posting dir"
                            " interrupt to cpu %d\n", number);
                }
                else if ((olddir & bitvector) &&
                        !(dir[number] & bitvector))
                {
                    // The bit was set and now its now clear and
                    // we were interrupting on that bit before
                    tsunami->intrctrl->clear(number, TheISA::INTLEVEL_IRQ1, x);
                    DPRINTF(Tsunami, "dim write resulting in clear"
                            " dir interrupt to cpu %d\n", number);

                }


            }
        }
    } else {
        switch(regnum) {
          case TSDEV_CC_CSR:
              panic("TSDEV_CC_CSR write\n");
          case TSDEV_CC_MTR:
              panic("TSDEV_CC_MTR write not implemented\n");
          case TSDEV_CC_MISC:
            uint64_t ipreq;
            ipreq = (val >> 12) & 0xF;
            //If it is bit 12-15, this is an IPI post
            if (ipreq) {
                reqIPI(ipreq);
                supportedWrite = true;
            }

            //If it is bit 8-11, this is an IPI clear
            uint64_t ipintr;
            ipintr = (val >> 8) & 0xF;
            if (ipintr) {
                clearIPI(ipintr);
                supportedWrite = true;
            }

            //If it is the 4-7th bit, clear the RTC interrupt
            uint64_t itintr;
              itintr = (val >> 4) & 0xF;
            if (itintr) {
                  clearITI(itintr);
                supportedWrite = true;
            }

              // ignore NXMs
              if (val & 0x10000000)
                  supportedWrite = true;

            if(!supportedWrite)
                  panic("TSDEV_CC_MISC write not implemented\n");


            case TSDEV_CC_AAR0:
            case TSDEV_CC_AAR1:
            case TSDEV_CC_AAR2:
            case TSDEV_CC_AAR3:
                panic("TSDEV_CC_AARx write not implemeted\n");
            case TSDEV_CC_DIM0:
            case TSDEV_CC_DIM1:
            case TSDEV_CC_DIM2:
            case TSDEV_CC_DIM3:
                int number;
                if(regnum == TSDEV_CC_DIM0)
                    number = 0;
                else if(regnum == TSDEV_CC_DIM1)
                    number = 1;
                else if(regnum == TSDEV_CC_DIM2)
                    number = 2;
                else
                    number = 3;

                uint64_t bitvector;
                uint64_t olddim;
                uint64_t olddir;

                olddim = dim[number];
                olddir = dir[number];
                dim[number] = val;
                dir[number] = dim[number] & drir;
                for(int x = 0; x < 64; x++)
                {
                    bitvector = ULL(1) << x;
                    // Figure out which bits have changed
                    if ((dim[number] & bitvector) != (olddim & bitvector))
                    {
                        // The bit is now set and it wasn't before (set)
                        if((dim[number] & bitvector) && (dir[number] & bitvector))
                        {
                          tsunami->intrctrl->post(number, TheISA::INTLEVEL_IRQ1, x);
                          DPRINTF(Tsunami, "posting dir interrupt to cpu 0\n");
                        }
                        else if ((olddir & bitvector) &&
                                !(dir[number] & bitvector))
                        {
                            // The bit was set and now its now clear and
                            // we were interrupting on that bit before
                            tsunami->intrctrl->clear(number, TheISA::INTLEVEL_IRQ1, x);
                          DPRINTF(Tsunami, "dim write resulting in clear"
                                    " dir interrupt to cpu %d\n",
                                    x);

                        }


                    }
                }
                break;
            case TSDEV_CC_DIR0:
            case TSDEV_CC_DIR1:
            case TSDEV_CC_DIR2:
            case TSDEV_CC_DIR3:
                panic("TSDEV_CC_DIR write not implemented\n");
            case TSDEV_CC_DRIR:
                panic("TSDEV_CC_DRIR write not implemented\n");
            case TSDEV_CC_PRBEN:
                panic("TSDEV_CC_PRBEN write not implemented\n");
            case TSDEV_CC_IIC0:
            case TSDEV_CC_IIC1:
            case TSDEV_CC_IIC2:
            case TSDEV_CC_IIC3:
                panic("TSDEV_CC_IICx write not implemented\n");
            case TSDEV_CC_MPR0:
            case TSDEV_CC_MPR1:
            case TSDEV_CC_MPR2:
            case TSDEV_CC_MPR3:
                panic("TSDEV_CC_MPRx write not implemented\n");
            case TSDEV_CC_IPIR:
                clearIPI(val);
                break;
            case TSDEV_CC_ITIR:
                clearITI(val);
                break;
            case TSDEV_CC_IPIQ:
                reqIPI(val);
                break;
            default:
              panic("default in cchip read reached, accessing 0x%x\n");
        }  // swtich(regnum)
    } // not BIG_TSUNAMI write
    pkt.result = Success;
    return pioDelay;
}

void
TsunamiCChip::clearIPI(uint64_t ipintr)
{
    int numcpus = tsunami->intrctrl->cpu->system->execContexts.size();
    assert(numcpus <= Tsunami::Max_CPUs);

    if (ipintr) {
        for (int cpunum=0; cpunum < numcpus; cpunum++) {
            // Check each cpu bit
            uint64_t cpumask = ULL(1) << cpunum;
            if (ipintr & cpumask) {
                // Check if there is a pending ipi
                if (ipint & cpumask) {
                    ipint &= ~cpumask;
                    tsunami->intrctrl->clear(cpunum, TheISA::INTLEVEL_IRQ3, 0);
                    DPRINTF(IPI, "clear IPI IPI cpu=%d\n", cpunum);
                }
                else
                    warn("clear IPI for CPU=%d, but NO IPI\n", cpunum);
            }
        }
    }
    else
        panic("Big IPI Clear, but not processors indicated\n");
}

void
TsunamiCChip::clearITI(uint64_t itintr)
{
    int numcpus = tsunami->intrctrl->cpu->system->execContexts.size();
    assert(numcpus <= Tsunami::Max_CPUs);

    if (itintr) {
        for (int i=0; i < numcpus; i++) {
            uint64_t cpumask = ULL(1) << i;
            if (itintr & cpumask & itint) {
                tsunami->intrctrl->clear(i, TheISA::INTLEVEL_IRQ2, 0);
                itint &= ~cpumask;
                DPRINTF(Tsunami, "clearing rtc interrupt to cpu=%d\n", i);
            }
        }
    }
    else
        panic("Big ITI Clear, but not processors indicated\n");
}

void
TsunamiCChip::reqIPI(uint64_t ipreq)
{
    int numcpus = tsunami->intrctrl->cpu->system->execContexts.size();
    assert(numcpus <= Tsunami::Max_CPUs);

    if (ipreq) {
        for (int cpunum=0; cpunum < numcpus; cpunum++) {
            // Check each cpu bit
            uint64_t cpumask = ULL(1) << cpunum;
            if (ipreq & cpumask) {
                // Check if there is already an ipi (bits 8:11)
                if (!(ipint & cpumask)) {
                    ipint  |= cpumask;
                    tsunami->intrctrl->post(cpunum, TheISA::INTLEVEL_IRQ3, 0);
                    DPRINTF(IPI, "send IPI cpu=%d\n", cpunum);
                }
                else
                    warn("post IPI for CPU=%d, but IPI already\n", cpunum);
            }
        }
    }
    else
        panic("Big IPI Request, but not processors indicated\n");
}


void
TsunamiCChip::postRTC()
{
    int size = tsunami->intrctrl->cpu->system->execContexts.size();
    assert(size <= Tsunami::Max_CPUs);

    for (int i = 0; i < size; i++) {
        uint64_t cpumask = ULL(1) << i;
       if (!(cpumask & itint)) {
           itint |= cpumask;
           tsunami->intrctrl->post(i, TheISA::INTLEVEL_IRQ2, 0);
           DPRINTF(Tsunami, "Posting RTC interrupt to cpu=%d", i);
       }
    }

}

void
TsunamiCChip::postDRIR(uint32_t interrupt)
{
    uint64_t bitvector = ULL(1) << interrupt;
    uint64_t size = tsunami->intrctrl->cpu->system->execContexts.size();
    assert(size <= Tsunami::Max_CPUs);
    drir |= bitvector;

    for(int i=0; i < size; i++) {
        dir[i] = dim[i] & drir;
       if (dim[i] & bitvector) {
              tsunami->intrctrl->post(i, TheISA::INTLEVEL_IRQ1, interrupt);
              DPRINTF(Tsunami, "posting dir interrupt to cpu %d,"
                        "interrupt %d\n",i, interrupt);
       }
    }
}

void
TsunamiCChip::clearDRIR(uint32_t interrupt)
{
    uint64_t bitvector = ULL(1) << interrupt;
    uint64_t size = tsunami->intrctrl->cpu->system->execContexts.size();
    assert(size <= Tsunami::Max_CPUs);

    if (drir & bitvector)
    {
        drir &= ~bitvector;
        for(int i=0; i < size; i++) {
           if (dir[i] & bitvector) {
               tsunami->intrctrl->clear(i, TheISA::INTLEVEL_IRQ1, interrupt);
               DPRINTF(Tsunami, "clearing dir interrupt to cpu %d,"
                    "interrupt %d\n",i, interrupt);

           }
           dir[i] = dim[i] & drir;
        }
    }
    else
        DPRINTF(Tsunami, "Spurrious clear? interrupt %d\n", interrupt);
}


void
TsunamiCChip::serialize(std::ostream &os)
{
    SERIALIZE_ARRAY(dim, Tsunami::Max_CPUs);
    SERIALIZE_ARRAY(dir, Tsunami::Max_CPUs);
    SERIALIZE_SCALAR(ipint);
    SERIALIZE_SCALAR(itint);
    SERIALIZE_SCALAR(drir);
}

void
TsunamiCChip::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_ARRAY(dim, Tsunami::Max_CPUs);
    UNSERIALIZE_ARRAY(dir, Tsunami::Max_CPUs);
    UNSERIALIZE_SCALAR(ipint);
    UNSERIALIZE_SCALAR(itint);
    UNSERIALIZE_SCALAR(drir);
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(TsunamiCChip)

    Param<Addr> pio_addr;
    Param<Tick> pio_latency;
    SimObjectParam<Platform *> platform;
    SimObjectParam<System *> system;
    SimObjectParam<Tsunami *> tsunami;

END_DECLARE_SIM_OBJECT_PARAMS(TsunamiCChip)

BEGIN_INIT_SIM_OBJECT_PARAMS(TsunamiCChip)

    INIT_PARAM(pio_addr, "Device Address"),
    INIT_PARAM(pio_latency, "Programmed IO latency"),
    INIT_PARAM(platform, "platform"),
    INIT_PARAM(system, "system object"),
    INIT_PARAM(tsunami, "Tsunami")

END_INIT_SIM_OBJECT_PARAMS(TsunamiCChip)

CREATE_SIM_OBJECT(TsunamiCChip)
{
    TsunamiCChip::Params *p = new TsunamiCChip::Params;
    p->name = getInstanceName();
    p->pio_addr = pio_addr;
    p->pio_delay = pio_latency;
    p->platform = platform;
    p->system = system;
    p->tsunami = tsunami;
    return new TsunamiCChip(p);
}

REGISTER_SIM_OBJECT("TsunamiCChip", TsunamiCChip)
