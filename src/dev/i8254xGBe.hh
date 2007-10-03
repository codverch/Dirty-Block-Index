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
 * Authors: Ali Saidi
 */

/* @file
 * Device model for Intel's 8254x line of gigabit ethernet controllers.
 */

#ifndef __DEV_I8254XGBE_HH__
#define __DEV_I8254XGBE_HH__

#include <deque>
#include <string>

#include "base/inet.hh"
#include "base/statistics.hh"
#include "dev/etherdevice.hh"
#include "dev/etherint.hh"
#include "dev/etherpkt.hh"
#include "dev/i8254xGBe_defs.hh"
#include "dev/pcidev.hh"
#include "dev/pktfifo.hh"
#include "params/IGbE.hh"
#include "sim/eventq.hh"

class IGbEInt;

class IGbE : public EtherDevice
{
  private:
    IGbEInt *etherInt;

    // device registers
    iGbReg::Regs regs;

    // eeprom data, status and control bits
    int eeOpBits, eeAddrBits, eeDataBits;
    uint8_t eeOpcode, eeAddr;
    uint16_t flash[iGbReg::EEPROM_SIZE];

    // The drain event if we have one
    Event *drainEvent;

    // cached parameters from params struct
    bool useFlowControl;

    // packet fifos
    PacketFifo rxFifo;
    PacketFifo txFifo;

    // Packet that we are currently putting into the txFifo
    EthPacketPtr txPacket;

    // Should to Rx/Tx State machine tick?
    bool rxTick;
    bool txTick;
    bool txFifoTick;

    bool rxDmaPacket;

    // Event and function to deal with RDTR timer expiring
    void rdtrProcess() {
        rxDescCache.writeback(0);
        DPRINTF(EthernetIntr, "Posting RXT interrupt because RDTR timer expired\n");
        postInterrupt(iGbReg::IT_RXT, true);
    }

    //friend class EventWrapper<IGbE, &IGbE::rdtrProcess>;
    EventWrapper<IGbE, &IGbE::rdtrProcess> rdtrEvent;

    // Event and function to deal with RADV timer expiring
    void radvProcess() {
        rxDescCache.writeback(0);
        DPRINTF(EthernetIntr, "Posting RXT interrupt because RADV timer expired\n");
        postInterrupt(iGbReg::IT_RXT, true);
    }

    //friend class EventWrapper<IGbE, &IGbE::radvProcess>;
    EventWrapper<IGbE, &IGbE::radvProcess> radvEvent;

    // Event and function to deal with TADV timer expiring
    void tadvProcess() {
        txDescCache.writeback(0);
        DPRINTF(EthernetIntr, "Posting TXDW interrupt because TADV timer expired\n");
        postInterrupt(iGbReg::IT_TXDW, true);
    }

    //friend class EventWrapper<IGbE, &IGbE::tadvProcess>;
    EventWrapper<IGbE, &IGbE::tadvProcess> tadvEvent;

    // Event and function to deal with TIDV timer expiring
    void tidvProcess() {
        txDescCache.writeback(0);
        DPRINTF(EthernetIntr, "Posting TXDW interrupt because TIDV timer expired\n");
        postInterrupt(iGbReg::IT_TXDW, true);
    }
    //friend class EventWrapper<IGbE, &IGbE::tidvProcess>;
    EventWrapper<IGbE, &IGbE::tidvProcess> tidvEvent;

    // Main event to tick the device
    void tick();
    //friend class EventWrapper<IGbE, &IGbE::tick>;
    EventWrapper<IGbE, &IGbE::tick> tickEvent;


    void rxStateMachine();
    void txStateMachine();
    void txWire();

    /** Write an interrupt into the interrupt pending register and check mask
     * and interrupt limit timer before sending interrupt to CPU
     * @param t the type of interrupt we are posting
     * @param now should we ignore the interrupt limiting timer
     */
    void postInterrupt(iGbReg::IntTypes t, bool now = false);

    /** Check and see if changes to the mask register have caused an interrupt
     * to need to be sent or perhaps removed an interrupt cause.
     */
    void chkInterrupt();

    /** Send an interrupt to the cpu
     */
    void delayIntEvent();
    void cpuPostInt();
    // Event to moderate interrupts
    EventWrapper<IGbE, &IGbE::delayIntEvent> interEvent;

    /** Clear the interupt line to the cpu
     */
    void cpuClearInt();

    Tick intClock() { return Clock::Int::ns * 1024; }

    /** This function is used to restart the clock so it can handle things like
     * draining and resume in one place. */
    void restartClock();

    /** Check if all the draining things that need to occur have occured and
     * handle the drain event if so.
     */
    void checkDrain();

    template<class T>
    class DescCache
    {
      protected:
        virtual Addr descBase() const = 0;
        virtual long descHead() const = 0;
        virtual long descTail() const = 0;
        virtual long descLen() const = 0;
        virtual void updateHead(long h) = 0;
        virtual void enableSm() = 0;
        virtual void intAfterWb() const {}
        virtual void fetchAfterWb() = 0;

        std::deque<T*> usedCache;
        std::deque<T*> unusedCache;

        T *fetchBuf;
        T *wbBuf;

        // Pointer to the device we cache for
        IGbE *igbe;

        // Name of this  descriptor cache
        std::string _name;

        // How far we've cached
        int cachePnt;

        // The size of the descriptor cache
        int size;

        // How many descriptors we are currently fetching
        int curFetching;

        // How many descriptors we are currently writing back
        int wbOut;

        // if the we wrote back to the end of the descriptor ring and are going
        // to have to wrap and write more
        bool moreToWb;

        // What the alignment is of the next descriptor writeback
        Addr wbAlignment;

       /** The packet that is currently being dmad to memory if any
         */
        EthPacketPtr pktPtr;

      public:
        DescCache(IGbE *i, const std::string n, int s)
            : igbe(i), _name(n), cachePnt(0), size(s), curFetching(0), wbOut(0),
              pktPtr(NULL), fetchEvent(this), wbEvent(this)
        {
            fetchBuf = new T[size];
            wbBuf = new T[size];
        }

        virtual ~DescCache()
        {
            reset();
        }

        std::string name() { return _name; }

        /** If the address/len/head change when we've got descriptors that are
         * dirty that is very bad. This function checks that we don't and if we
         * do panics.
         */
        void areaChanged()
        {
            if (usedCache.size() > 0 || curFetching || wbOut)
                panic("Descriptor Address, Length or Head changed. Bad\n");
            reset();

        }

        void writeback(Addr aMask)
        {
            int curHead = descHead();
            int max_to_wb = usedCache.size();

            DPRINTF(EthernetDesc, "Writing back descriptors head: %d tail: "
                    "%d len: %d cachePnt: %d max_to_wb: %d descleft: %d\n",
                    curHead, descTail(), descLen(), cachePnt, max_to_wb,
                    descLeft());

            // Check if this writeback is less restrictive that the previous
            // and if so setup another one immediately following it
            if (wbOut && (aMask < wbAlignment)) {
                moreToWb = true;
                wbAlignment = aMask;
                DPRINTF(EthernetDesc, "Writing back already in process, returning\n");
                return;
            }


            moreToWb = false;
            wbAlignment = aMask;

            if (max_to_wb + curHead >= descLen()) {
                max_to_wb = descLen() - curHead;
                moreToWb = true;
                // this is by definition aligned correctly
            } else if (aMask != 0) {
                // align the wb point to the mask
                max_to_wb = max_to_wb & ~aMask;
            }

            DPRINTF(EthernetDesc, "Writing back %d descriptors\n", max_to_wb);

            if (max_to_wb <= 0 || wbOut)
                return;

            wbOut = max_to_wb;

            for (int x = 0; x < wbOut; x++)
                memcpy(&wbBuf[x], usedCache[x], sizeof(T));


            assert(wbOut);
            igbe->dmaWrite(igbe->platform->pciToDma(descBase() + curHead * sizeof(T)),
                    wbOut * sizeof(T), &wbEvent, (uint8_t*)wbBuf);
        }

        /** Fetch a chunk of descriptors into the descriptor cache.
         * Calls fetchComplete when the memory system returns the data
         */
        void fetchDescriptors()
        {
            size_t max_to_fetch;

            if (descTail() >= cachePnt)
                max_to_fetch = descTail() - cachePnt;
            else
                max_to_fetch = descLen() - cachePnt;

            max_to_fetch = std::min(max_to_fetch, (size - usedCache.size() -
                        unusedCache.size()));

            DPRINTF(EthernetDesc, "Fetching descriptors head: %d tail: "
                    "%d len: %d cachePnt: %d max_to_fetch: %d descleft: %d\n",
                    descHead(), descTail(), descLen(), cachePnt,
                    max_to_fetch, descLeft());

            // Nothing to do
            if (max_to_fetch == 0 || curFetching)
                return;

            // So we don't have two descriptor fetches going on at once
            curFetching = max_to_fetch;

            DPRINTF(EthernetDesc, "Fetching descriptors at %#x (%#x), size: %#x\n",
                    descBase() + cachePnt * sizeof(T),
                    igbe->platform->pciToDma(descBase() + cachePnt * sizeof(T)),
                    curFetching * sizeof(T));

            assert(curFetching);
            igbe->dmaRead(igbe->platform->pciToDma(descBase() + cachePnt * sizeof(T)),
                    curFetching * sizeof(T), &fetchEvent, (uint8_t*)fetchBuf);
        }


        /** Called by event when dma to read descriptors is completed
         */
        void fetchComplete()
        {
            T *newDesc;
            for (int x = 0; x < curFetching; x++) {
                newDesc = new T;
                memcpy(newDesc, &fetchBuf[x], sizeof(T));
                unusedCache.push_back(newDesc);
            }

#ifndef NDEBUG
            int oldCp = cachePnt;
#endif

            cachePnt += curFetching;
            assert(cachePnt <= descLen());
            if (cachePnt == descLen())
                cachePnt = 0;

            curFetching = 0;

            DPRINTF(EthernetDesc, "Fetching complete cachePnt %d -> %d\n",
                    oldCp, cachePnt);

            enableSm();
            igbe->checkDrain();
        }

        EventWrapper<DescCache, &DescCache::fetchComplete> fetchEvent;

        /** Called by event when dma to writeback descriptors is completed
         */
        void wbComplete()
        {

            long  curHead = descHead();
#ifndef NDEBUG
            long oldHead = curHead;
#endif
            for (int x = 0; x < wbOut; x++) {
                assert(usedCache.size());
                delete usedCache[0];
                usedCache.pop_front();
            };

            curHead += wbOut;
            wbOut = 0;

            if (curHead >= descLen())
                curHead -= descLen();

            // Update the head
            updateHead(curHead);

            DPRINTF(EthernetDesc, "Writeback complete curHead %d -> %d\n",
                    oldHead, curHead);

            // If we still have more to wb, call wb now
            intAfterWb();
            if (moreToWb) {
                DPRINTF(EthernetDesc, "Writeback has more todo\n");
                writeback(wbAlignment);
            }

            if (!wbOut) {
                igbe->checkDrain();
            }
            fetchAfterWb();
        }


        EventWrapper<DescCache, &DescCache::wbComplete> wbEvent;

        /* Return the number of descriptors left in the ring, so the device has
         * a way to figure out if it needs to interrupt.
         */
        int descLeft() const
        {
            int left = unusedCache.size();
            if (cachePnt - descTail() >= 0)
                left += (cachePnt - descTail());
            else
                left += (descTail() - cachePnt);

            return left;
        }

        /* Return the number of descriptors used and not written back.
         */
        int descUsed() const { return usedCache.size(); }

        /* Return the number of cache unused descriptors we have. */
        int descUnused() const {return unusedCache.size(); }

        /* Get into a state where the descriptor address/head/etc colud be
         * changed */
        void reset()
        {
            DPRINTF(EthernetDesc, "Reseting descriptor cache\n");
            for (int x = 0; x < usedCache.size(); x++)
                delete usedCache[x];
            for (int x = 0; x < unusedCache.size(); x++)
                delete unusedCache[x];

            usedCache.clear();
            unusedCache.clear();

            cachePnt = 0;

        }

        virtual void serialize(std::ostream &os)
        {
            SERIALIZE_SCALAR(cachePnt);
            SERIALIZE_SCALAR(curFetching);
            SERIALIZE_SCALAR(wbOut);
            SERIALIZE_SCALAR(moreToWb);
            SERIALIZE_SCALAR(wbAlignment);

            int usedCacheSize = usedCache.size();
            SERIALIZE_SCALAR(usedCacheSize);
            for(int x = 0; x < usedCacheSize; x++) {
                arrayParamOut(os, csprintf("usedCache_%d", x),
                        (uint8_t*)usedCache[x],sizeof(T));
            }

            int unusedCacheSize = unusedCache.size();
            SERIALIZE_SCALAR(unusedCacheSize);
            for(int x = 0; x < unusedCacheSize; x++) {
                arrayParamOut(os, csprintf("unusedCache_%d", x),
                        (uint8_t*)unusedCache[x],sizeof(T));
            }
        }

        virtual void unserialize(Checkpoint *cp, const std::string &section)
        {
            UNSERIALIZE_SCALAR(cachePnt);
            UNSERIALIZE_SCALAR(curFetching);
            UNSERIALIZE_SCALAR(wbOut);
            UNSERIALIZE_SCALAR(moreToWb);
            UNSERIALIZE_SCALAR(wbAlignment);

            int usedCacheSize;
            UNSERIALIZE_SCALAR(usedCacheSize);
            T *temp;
            for(int x = 0; x < usedCacheSize; x++) {
                temp = new T;
                arrayParamIn(cp, section, csprintf("usedCache_%d", x),
                        (uint8_t*)temp,sizeof(T));
                usedCache.push_back(temp);
            }

            int unusedCacheSize;
            UNSERIALIZE_SCALAR(unusedCacheSize);
            for(int x = 0; x < unusedCacheSize; x++) {
                temp = new T;
                arrayParamIn(cp, section, csprintf("unusedCache_%d", x),
                        (uint8_t*)temp,sizeof(T));
                unusedCache.push_back(temp);
            }
        }
        virtual bool hasOutstandingEvents() {
            return wbEvent.scheduled() || fetchEvent.scheduled();
        }

     };


    class RxDescCache : public DescCache<iGbReg::RxDesc>
    {
      protected:
        virtual Addr descBase() const { return igbe->regs.rdba(); }
        virtual long descHead() const { return igbe->regs.rdh(); }
        virtual long descLen() const { return igbe->regs.rdlen() >> 4; }
        virtual long descTail() const { return igbe->regs.rdt(); }
        virtual void updateHead(long h) { igbe->regs.rdh(h); }
        virtual void enableSm();
        virtual void fetchAfterWb() {
            if (!igbe->rxTick && igbe->getState() == SimObject::Running)
                fetchDescriptors();
        }

        bool pktDone;

      public:
        RxDescCache(IGbE *i, std::string n, int s);

        /** Write the given packet into the buffer(s) pointed to by the
         * descriptor and update the book keeping. Should only be called when
         * there are no dma's pending.
         * @param packet ethernet packet to write
         * @return if the packet could be written (there was a free descriptor)
         */
        bool writePacket(EthPacketPtr packet);
        /** Called by event when dma to write packet is completed
         */
        void pktComplete();

        /** Check if the dma on the packet has completed.
         */

        bool packetDone();

        EventWrapper<RxDescCache, &RxDescCache::pktComplete> pktEvent;

        virtual bool hasOutstandingEvents();

        virtual void serialize(std::ostream &os);
        virtual void unserialize(Checkpoint *cp, const std::string &section);
    };
    friend class RxDescCache;

    RxDescCache rxDescCache;

    class TxDescCache  : public DescCache<iGbReg::TxDesc>
    {
      protected:
        virtual Addr descBase() const { return igbe->regs.tdba(); }
        virtual long descHead() const { return igbe->regs.tdh(); }
        virtual long descTail() const { return igbe->regs.tdt(); }
        virtual long descLen() const { return igbe->regs.tdlen() >> 4; }
        virtual void updateHead(long h) { igbe->regs.tdh(h); }
        virtual void enableSm();
        virtual void intAfterWb() const {
            igbe->postInterrupt(iGbReg::IT_TXDW);
        }
        virtual void fetchAfterWb() {
            if (!igbe->txTick && igbe->getState() == SimObject::Running)
                fetchDescriptors();
        }

        bool pktDone;
        bool isTcp;
        bool pktWaiting;

      public:
        TxDescCache(IGbE *i, std::string n, int s);

        /** Tell the cache to DMA a packet from main memory into its buffer and
         * return the size the of the packet to reserve space in tx fifo.
         * @return size of the packet
         */
        int getPacketSize();
        void getPacketData(EthPacketPtr p);

        /** Ask if the packet has been transfered so the state machine can give
         * it to the fifo.
         * @return packet available in descriptor cache
         */
        bool packetAvailable();

        /** Ask if we are still waiting for the packet to be transfered.
         * @return packet still in transit.
         */
        bool packetWaiting() { return pktWaiting; }

        /** Called by event when dma to write packet is completed
         */
        void pktComplete();
        EventWrapper<TxDescCache, &TxDescCache::pktComplete> pktEvent;

        virtual bool hasOutstandingEvents();

        virtual void serialize(std::ostream &os);
        virtual void unserialize(Checkpoint *cp, const std::string &section);

    };
    friend class TxDescCache;

    TxDescCache txDescCache;

  public:
    typedef IGbEParams Params;
    const Params *
    params() const
    {
        return dynamic_cast<const Params *>(_params);
    }
    IGbE(const Params *params);
    ~IGbE() {}

    virtual EtherInt *getEthPort(const std::string &if_name, int idx);

    Tick clock;
    inline Tick ticks(int numCycles) const { return numCycles * clock; }

    virtual Tick read(PacketPtr pkt);
    virtual Tick write(PacketPtr pkt);

    virtual Tick writeConfig(PacketPtr pkt);

    bool ethRxPkt(EthPacketPtr packet);
    void ethTxDone();

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);
    virtual unsigned int drain(Event *de);
    virtual void resume();

};

class IGbEInt : public EtherInt
{
  private:
    IGbE *dev;

  public:
    IGbEInt(const std::string &name, IGbE *d)
        : EtherInt(name), dev(d)
    { }

    virtual bool recvPacket(EthPacketPtr pkt) { return dev->ethRxPkt(pkt); }
    virtual void sendDone() { dev->ethTxDone(); }
};





#endif //__DEV_I8254XGBE_HH__

