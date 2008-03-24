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
 * Authors: Ron Dreslinski
 *          Steve Reinhardt
 *          Ali Saidi
 */

/**
 * @file
 * Declaration of the Packet class.
 */

#ifndef __MEM_PACKET_HH__
#define __MEM_PACKET_HH__

#include <cassert>
#include <list>
#include <bitset>

#include "base/compiler.hh"
#include "base/fast_alloc.hh"
#include "base/misc.hh"
#include "base/printable.hh"
#include "mem/request.hh"
#include "sim/host.hh"
#include "sim/core.hh"


struct Packet;
typedef Packet *PacketPtr;
typedef uint8_t* PacketDataPtr;
typedef std::list<PacketPtr> PacketList;

class MemCmd
{
  public:

    /** List of all commands associated with a packet. */
    enum Command
    {
        InvalidCmd,
        ReadReq,
        ReadResp,
        ReadRespWithInvalidate,
        WriteReq,
        WriteResp,
        Writeback,
        SoftPFReq,
        HardPFReq,
        SoftPFResp,
        HardPFResp,
        WriteInvalidateReq,
        WriteInvalidateResp,
        UpgradeReq,
        UpgradeResp,
        ReadExReq,
        ReadExResp,
        LoadLockedReq,
        LoadLockedResp,
        StoreCondReq,
        StoreCondResp,
        SwapReq,
        SwapResp,
        // Error responses
        // @TODO these should be classified as responses rather than
        // requests; coding them as requests initially for backwards
        // compatibility
        NetworkNackError,  // nacked at network layer (not by protocol)
        InvalidDestError,  // packet dest field invalid
        BadAddressError,   // memory address invalid
        // Fake simulator-only commands
        PrintReq,       // Print state matching address
        NUM_MEM_CMDS
    };

  private:
    /** List of command attributes. */
    enum Attribute
    {
        IsRead,         //!< Data flows from responder to requester
        IsWrite,        //!< Data flows from requester to responder
        IsPrefetch,     //!< Not a demand access
        IsInvalidate,
        NeedsExclusive, //!< Requires exclusive copy to complete in-cache
        IsRequest,      //!< Issued by requester
        IsResponse,     //!< Issue by responder
        NeedsResponse,  //!< Requester needs response from target
        IsSWPrefetch,
        IsHWPrefetch,
        IsLocked,       //!< Alpha/MIPS LL or SC access
        HasData,        //!< There is an associated payload
        IsError,        //!< Error response
        IsPrint,        //!< Print state matching address (for debugging)
        NUM_COMMAND_ATTRIBUTES
    };

    /** Structure that defines attributes and other data associated
     * with a Command. */
    struct CommandInfo {
        /** Set of attribute flags. */
        const std::bitset<NUM_COMMAND_ATTRIBUTES> attributes;
        /** Corresponding response for requests; InvalidCmd if no
         * response is applicable. */
        const Command response;
        /** String representation (for printing) */
        const std::string str;
    };

    /** Array to map Command enum to associated info. */
    static const CommandInfo commandInfo[];

  private:

    Command cmd;

    bool testCmdAttrib(MemCmd::Attribute attrib) const {
        return commandInfo[cmd].attributes[attrib] != 0;
    }

  public:

    bool isRead() const         { return testCmdAttrib(IsRead); }
    bool isWrite()  const       { return testCmdAttrib(IsWrite); }
    bool isRequest() const      { return testCmdAttrib(IsRequest); }
    bool isResponse() const     { return testCmdAttrib(IsResponse); }
    bool needsExclusive() const { return testCmdAttrib(NeedsExclusive); }
    bool needsResponse() const  { return testCmdAttrib(NeedsResponse); }
    bool isInvalidate() const   { return testCmdAttrib(IsInvalidate); }
    bool hasData() const        { return testCmdAttrib(HasData); }
    bool isReadWrite() const    { return isRead() && isWrite(); }
    bool isLocked() const       { return testCmdAttrib(IsLocked); }
    bool isError() const        { return testCmdAttrib(IsError); }
    bool isPrint() const        { return testCmdAttrib(IsPrint); }

    const Command responseCommand() const {
        return commandInfo[cmd].response;
    }

    /** Return the string to a cmd given by idx. */
    const std::string &toString() const {
        return commandInfo[cmd].str;
    }

    int toInt() const { return (int)cmd; }

    MemCmd(Command _cmd)
        : cmd(_cmd)
    { }

    MemCmd(int _cmd)
        : cmd((Command)_cmd)
    { }

    MemCmd()
        : cmd(InvalidCmd)
    { }

    bool operator==(MemCmd c2) { return (cmd == c2.cmd); }
    bool operator!=(MemCmd c2) { return (cmd != c2.cmd); }

    friend class Packet;
};

/**
 * A Packet is used to encapsulate a transfer between two objects in
 * the memory system (e.g., the L1 and L2 cache).  (In contrast, a
 * single Request travels all the way from the requester to the
 * ultimate destination and back, possibly being conveyed by several
 * different Packets along the way.)
 */
class Packet : public FastAlloc, public Printable
{
  public:

    typedef MemCmd::Command Command;

    /** The command field of the packet. */
    MemCmd cmd;

    /** A pointer to the original request. */
    RequestPtr req;

  private:
   /** A pointer to the data being transfered.  It can be differnt
    *    sizes at each level of the heirarchy so it belongs in the
    *    packet, not request. This may or may not be populated when a
    *    responder recieves the packet. If not populated it memory
    *    should be allocated.
    */
    PacketDataPtr data;

    /** Is the data pointer set to a value that shouldn't be freed
     *   when the packet is destroyed? */
    bool staticData;
    /** The data pointer points to a value that should be freed when
     *   the packet is destroyed. */
    bool dynamicData;
    /** the data pointer points to an array (thus delete [] ) needs to
     *   be called on it rather than simply delete.*/
    bool arrayData;

    /** The address of the request.  This address could be virtual or
     *   physical, depending on the system configuration. */
    Addr addr;

     /** The size of the request or transfer. */
    int size;

    /** Device address (e.g., bus ID) of the source of the
     *   transaction. The source is not responsible for setting this
     *   field; it is set implicitly by the interconnect when the
     *   packet is first sent.  */
    short src;

    /** Device address (e.g., bus ID) of the destination of the
     *   transaction. The special value Broadcast indicates that the
     *   packet should be routed based on its address. This field is
     *   initialized in the constructor and is thus always valid
     *   (unlike * addr, size, and src). */
    short dest;

    /** The original value of the command field.  Only valid when the
     * current command field is an error condition; in that case, the
     * previous contents of the command field are copied here.  This
     * field is *not* set on non-error responses.
     */
    MemCmd origCmd;

    /** Are the 'addr' and 'size' fields valid? */
    bool addrSizeValid;
    /** Is the 'src' field valid? */
    bool srcValid;
    bool destValid;

    enum Flag {
        // Snoop response flags
        MemInhibit,
        Shared,
        // Special control flags
        /// Special timing-mode atomic snoop for multi-level coherence.
        ExpressSnoop,
        /// Does supplier have exclusive copy?
        /// Useful for multi-level coherence.
        SupplyExclusive,
        NUM_PACKET_FLAGS
    };

    /** Status flags */
    std::bitset<NUM_PACKET_FLAGS> flags;

  public:

    /** Used to calculate latencies for each packet.*/
    Tick time;

    /** The time at which the packet will be fully transmitted */
    Tick finishTime;

    /** The time at which the first chunk of the packet will be transmitted */
    Tick firstWordTime;

    /** The special destination address indicating that the packet
     *   should be routed based on its address. */
    static const short Broadcast = -1;

    /** A virtual base opaque structure used to hold state associated
     *    with the packet but specific to the sending device (e.g., an
     *    MSHR).  A pointer to this state is returned in the packet's
     *    response so that the sender can quickly look up the state
     *    needed to process it.  A specific subclass would be derived
     *    from this to carry state specific to a particular sending
     *    device.  */
    class SenderState {
      public:
        virtual ~SenderState() {}
    };

    /**
     * Object used to maintain state of a PrintReq.  The senderState
     * field of a PrintReq should always be of this type.
     */
    class PrintReqState : public SenderState, public FastAlloc {
        /** An entry in the label stack. */
        class LabelStackEntry {
          public:
            const std::string label;
            std::string *prefix;
            bool labelPrinted;
            LabelStackEntry(const std::string &_label,
                            std::string *_prefix);
        };

        typedef std::list<LabelStackEntry> LabelStack;
        LabelStack labelStack;

        std::string *curPrefixPtr;

      public:
        std::ostream &os;
        const int verbosity;

        PrintReqState(std::ostream &os, int verbosity = 0);
        ~PrintReqState();

        /** Returns the current line prefix. */
        const std::string &curPrefix() { return *curPrefixPtr; }

        /** Push a label onto the label stack, and prepend the given
         * prefix string onto the current prefix.  Labels will only be
         * printed if an object within the label's scope is
         * printed. */
        void pushLabel(const std::string &lbl,
                       const std::string &prefix = "  ");
        /** Pop a label off the label stack. */
        void popLabel();
        /** Print all of the pending unprinted labels on the
         * stack. Called by printObj(), so normally not called by
         * users unless bypassing printObj(). */
        void printLabels();
        /** Print a Printable object to os, because it matched the
         * address on a PrintReq. */
        void printObj(Printable *obj);
    };

    /** This packet's sender state.  Devices should use dynamic_cast<>
     *   to cast to the state appropriate to the sender. */
    SenderState *senderState;

    /** Return the string name of the cmd field (for debugging and
     *   tracing). */
    const std::string &cmdString() const { return cmd.toString(); }

    /** Return the index of this command. */
    inline int cmdToIndex() const { return cmd.toInt(); }

    bool isRead() const         { return cmd.isRead(); }
    bool isWrite()  const       { return cmd.isWrite(); }
    bool isRequest() const      { return cmd.isRequest(); }
    bool isResponse() const     { return cmd.isResponse(); }
    bool needsExclusive() const { return cmd.needsExclusive(); }
    bool needsResponse() const  { return cmd.needsResponse(); }
    bool isInvalidate() const   { return cmd.isInvalidate(); }
    bool hasData() const        { return cmd.hasData(); }
    bool isReadWrite() const    { return cmd.isReadWrite(); }
    bool isLocked() const       { return cmd.isLocked(); }
    bool isError() const        { return cmd.isError(); }
    bool isPrint() const        { return cmd.isPrint(); }

    // Snoop flags
    void assertMemInhibit()     { flags[MemInhibit] = true; }
    bool memInhibitAsserted()   { return flags[MemInhibit]; }
    void assertShared()         { flags[Shared] = true; }
    bool sharedAsserted()       { return flags[Shared]; }

    // Special control flags
    void setExpressSnoop()      { flags[ExpressSnoop] = true; }
    bool isExpressSnoop()       { return flags[ExpressSnoop]; }
    void setSupplyExclusive()   { flags[SupplyExclusive] = true; }
    bool isSupplyExclusive()    { return flags[SupplyExclusive]; }

    // Network error conditions... encapsulate them as methods since
    // their encoding keeps changing (from result field to command
    // field, etc.)
    void setNacked()     { assert(isResponse()); cmd = MemCmd::NetworkNackError; }
    void setBadAddress() { assert(isResponse()); cmd = MemCmd::BadAddressError; }
    bool wasNacked()     { return cmd == MemCmd::NetworkNackError; }
    bool hadBadAddress() { return cmd == MemCmd::BadAddressError; }
    void copyError(Packet *pkt) { assert(pkt->isError()); cmd = pkt->cmd; }

    bool nic_pkt() { panic("Unimplemented"); M5_DUMMY_RETURN }

    /** Accessor function that returns the source index of the packet. */
    short getSrc() const    { assert(srcValid); return src; }
    void setSrc(short _src) { src = _src; srcValid = true; }
    /** Reset source field, e.g. to retransmit packet on different bus. */
    void clearSrc() { srcValid = false; }

    /** Accessor function that returns the destination index of
        the packet. */
    short getDest() const     { assert(destValid); return dest; }
    void setDest(short _dest) { dest = _dest; destValid = true; }

    Addr getAddr() const { assert(addrSizeValid); return addr; }
    int getSize() const  { assert(addrSizeValid); return size; }
    Addr getOffset(int blkSize) const { return addr & (Addr)(blkSize - 1); }

    /** Constructor.  Note that a Request object must be constructed
     *   first, but the Requests's physical address and size fields
     *   need not be valid. The command and destination addresses
     *   must be supplied.  */
    Packet(Request *_req, MemCmd _cmd, short _dest)
        :  cmd(_cmd), req(_req),
           data(NULL), staticData(false), dynamicData(false), arrayData(false),
           addr(_req->paddr), size(_req->size), dest(_dest),
           addrSizeValid(_req->validPaddr), srcValid(false), destValid(true),
           flags(0), time(curTick), senderState(NULL)
    {
    }

    /** Alternate constructor if you are trying to create a packet with
     *  a request that is for a whole block, not the address from the req.
     *  this allows for overriding the size/addr of the req.*/
    Packet(Request *_req, MemCmd _cmd, short _dest, int _blkSize)
        :  cmd(_cmd), req(_req),
           data(NULL), staticData(false), dynamicData(false), arrayData(false),
           addr(_req->paddr & ~(_blkSize - 1)), size(_blkSize), dest(_dest),
           addrSizeValid(_req->validPaddr), srcValid(false), destValid(true),
           flags(0), time(curTick), senderState(NULL)
    {
    }

    /** Alternate constructor for copying a packet.  Copy all fields
     * *except* if the original packet's data was dynamic, don't copy
     * that, as we can't guarantee that the new packet's lifetime is
     * less than that of the original packet.  In this case the new
     * packet should allocate its own data. */
    Packet(Packet *origPkt, bool clearFlags = false)
        :  cmd(origPkt->cmd), req(origPkt->req),
           data(origPkt->staticData ? origPkt->data : NULL),
           staticData(origPkt->staticData),
           dynamicData(false), arrayData(false),
           addr(origPkt->addr), size(origPkt->size),
           src(origPkt->src), dest(origPkt->dest),
           addrSizeValid(origPkt->addrSizeValid),
           srcValid(origPkt->srcValid), destValid(origPkt->destValid),
           flags(clearFlags ? 0 : origPkt->flags),
           time(curTick), senderState(origPkt->senderState)
    {
    }

    /** Destructor. */
    ~Packet()
    { if (staticData || dynamicData) deleteData(); }

    /** Reinitialize packet address and size from the associated
     *   Request object, and reset other fields that may have been
     *   modified by a previous transaction.  Typically called when a
     *   statically allocated Request/Packet pair is reused for
     *   multiple transactions. */
    void reinitFromRequest() {
        assert(req->validPaddr);
        flags = 0;
        addr = req->paddr;
        size = req->size;
        time = req->time;
        addrSizeValid = true;
        if (dynamicData) {
            deleteData();
            dynamicData = false;
            arrayData = false;
        }
    }

    /**
     * Take a request packet and modify it in place to be suitable for
     * returning as a response to that request.  The source and
     * destination fields are *not* modified, as is appropriate for
     * atomic accesses.
     */
    void makeResponse()
    {
        assert(needsResponse());
        assert(isRequest());
        origCmd = cmd;
        cmd = cmd.responseCommand();
        dest = src;
        destValid = srcValid;
        srcValid = false;
    }

    void makeAtomicResponse()
    {
        makeResponse();
    }

    void makeTimingResponse()
    {
        makeResponse();
    }

    /**
     * Take a request packet that has been returned as NACKED and
     * modify it so that it can be sent out again. Only packets that
     * need a response can be NACKED, so verify that that is true.
     */
    void
    reinitNacked()
    {
        assert(wasNacked());
        cmd = origCmd;
        assert(needsResponse());
        setDest(Broadcast);
    }


    /**
     * Set the data pointer to the following value that should not be
     * freed.
     */
    template <typename T>
    void
    dataStatic(T *p)
    {
        if(dynamicData)
            dynamicData = false;
        data = (PacketDataPtr)p;
        staticData = true;
    }

    /**
     * Set the data pointer to a value that should have delete []
     * called on it.
     */
    template <typename T>
    void
    dataDynamicArray(T *p)
    {
        assert(!staticData && !dynamicData);
        data = (PacketDataPtr)p;
        dynamicData = true;
        arrayData = true;
    }

    /**
     * set the data pointer to a value that should have delete called
     * on it.
     */
    template <typename T>
    void
    dataDynamic(T *p)
    {
        assert(!staticData && !dynamicData);
        data = (PacketDataPtr)p;
        dynamicData = true;
        arrayData = false;
    }

    /** get a pointer to the data ptr. */
    template <typename T>
    T*
    getPtr()
    {
        assert(staticData || dynamicData);
        return (T*)data;
    }

    /** return the value of what is pointed to in the packet. */
    template <typename T>
    T get();

    /** set the value in the data pointer to v. */
    template <typename T>
    void set(T v);

    /**
     * Copy data into the packet from the provided pointer.
     */
    void setData(uint8_t *p)
    {
        std::memcpy(getPtr<uint8_t>(), p, getSize());
    }

    /**
     * Copy data into the packet from the provided block pointer,
     * which is aligned to the given block size.
     */
    void setDataFromBlock(uint8_t *blk_data, int blkSize)
    {
        setData(blk_data + getOffset(blkSize));
    }

    /**
     * Copy data from the packet to the provided block pointer, which
     * is aligned to the given block size.
     */
    void writeData(uint8_t *p)
    {
        std::memcpy(p, getPtr<uint8_t>(), getSize());
    }

    /**
     * Copy data from the packet to the memory at the provided pointer.
     */
    void writeDataToBlock(uint8_t *blk_data, int blkSize)
    {
        writeData(blk_data + getOffset(blkSize));
    }

    /**
     * delete the data pointed to in the data pointer. Ok to call to
     * matter how data was allocted.
     */
    void deleteData();

    /** If there isn't data in the packet, allocate some. */
    void allocate();

    /**
     * Check a functional request against a memory value represented
     * by a base/size pair and an associated data array.  If the
     * functional request is a read, it may be satisfied by the memory
     * value.  If the functional request is a write, it may update the
     * memory value.
     */
    bool checkFunctional(Printable *obj, Addr base, int size, uint8_t *data);

    /**
     * Check a functional request against a memory value stored in
     * another packet (i.e. an in-transit request or response).
     */
    bool checkFunctional(PacketPtr otherPkt) {
        return checkFunctional(otherPkt,
                               otherPkt->getAddr(), otherPkt->getSize(),
                               otherPkt->hasData() ?
                                   otherPkt->getPtr<uint8_t>() : NULL);
    }

    /**
     * Push label for PrintReq (safe to call unconditionally).
     */
    void pushLabel(const std::string &lbl) {
        if (isPrint()) {
            dynamic_cast<PrintReqState*>(senderState)->pushLabel(lbl);
        }
    }

    /**
     * Pop label for PrintReq (safe to call unconditionally).
     */
    void popLabel() {
        if (isPrint()) {
            dynamic_cast<PrintReqState*>(senderState)->popLabel();
        }
    }

    void print(std::ostream &o, int verbosity = 0,
               const std::string &prefix = "") const;
};

#endif //__MEM_PACKET_HH
