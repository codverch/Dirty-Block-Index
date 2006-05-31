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
 */

/**
 * @file
 * Declaration of the Packet class.
 */

#ifndef __MEM_PACKET_HH__
#define __MEM_PACKET_HH__

#include "mem/request.hh"
#include "arch/isa_traits.hh"
#include "sim/root.hh"

struct Packet;
typedef Packet* PacketPtr;
typedef uint8_t* PacketDataPtr;

/**
 * A Packet is used to encapsulate a transfer between two objects in
 * the memory system (e.g., the L1 and L2 cache).  (In contrast, a
 * single Request travels all the way from the requester to the
 * ultimate destination and back, possibly being conveyed by several
 * different Packets along the way.)
 */
class Packet
{
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
     *   packet * is first sent.  */
    short src;

    /** Device address (e.g., bus ID) of the destination of the
     *   transaction. The special value Broadcast indicates that the
     *   packet should be routed based on its address. This field is
     *   initialized in the constructor and is thus always valid
     *   (unlike * addr, size, and src). */
    short dest;

    /** Is the 'addr' field valid? */
    bool addrValid;
    /** Is the 'size' field valid? */
    bool sizeValid;
    /** Is the 'src' field valid? */
    bool srcValid;

  public:

    /** The special destination address indicating that the packet
     *   should be routed based on its address. */
    static const short Broadcast = -1;

    /** A pointer to the original request. */
    RequestPtr req;

    /** A virtual base opaque structure used to hold coherence-related
     *    state.  A specific subclass would be derived from this to
     *    carry state specific to a particular coherence protocol.  */
    class CoherenceState {
      public:
        virtual ~CoherenceState() {}
    };

    /** This packet's coherence state.  Caches should use
     *   dynamic_cast<> to cast to the state appropriate for the
     *   system's coherence protocol.  */
    CoherenceState *coherence;

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

    /** This packet's sender state.  Devices should use dynamic_cast<>
     *   to cast to the state appropriate to the sender. */
    SenderState *senderState;

  private:
    /** List of command attributes. */
    enum CommandAttribute
    {
        IsRead		= 1 << 0,
        IsWrite		= 1 << 1,
        IsPrefetch	= 1 << 2,
        IsInvalidate	= 1 << 3,
        IsRequest	= 1 << 4,
        IsResponse 	= 1 << 5,
        NeedsResponse	= 1 << 6,
    };

  public:
    /** List of all commands associated with a packet. */
    enum Command
    {
        ReadReq		= IsRead  | IsRequest | NeedsResponse,
        WriteReq	= IsWrite | IsRequest | NeedsResponse,
        WriteReqNoAck	= IsWrite | IsRequest,
        ReadResp	= IsRead  | IsResponse,
        WriteResp	= IsWrite | IsResponse
    };

    /** Return the string name of the cmd field (for debugging and
     *   tracing). */
    const std::string &cmdString() const;

    /** The command field of the packet. */
    Command cmd;

    bool isRead() 	 { return (cmd & IsRead)  != 0; }
    bool isRequest()	 { return (cmd & IsRequest)  != 0; }
    bool isResponse()	 { return (cmd & IsResponse) != 0; }
    bool needsResponse() { return (cmd & NeedsResponse) != 0; }

    /** Possible results of a packet's request. */
    enum Result
    {
        Success,
        BadAddress,
        Unknown
    };

    /** The result of this packet's request. */
    Result result;

    /** Accessor function that returns the source index of the packet. */
    short getSrc() const { assert(srcValid); return src; }
    void setSrc(short _src) { src = _src; srcValid = true; }

    /** Accessor function that returns the destination index of
        the packet. */
    short getDest() const { return dest; }
    void setDest(short _dest) { dest = _dest; }

    Addr getAddr() const { assert(addrValid); return addr; }
    void setAddr(Addr _addr) { addr = _addr; addrValid = true; }

    int getSize() const { assert(sizeValid); return size; }
    void setSize(int _size) { size = _size; sizeValid = true; }

    /** Constructor.  Note that a Request object must be constructed
     *   first, but the Requests's physical address and size fields
     *   need not be valid. The command and destination addresses
     *   must be supplied.  */
    Packet(Request *_req, Command _cmd, short _dest)
        :  data(NULL), staticData(false), dynamicData(false), arrayData(false),
           addr(_req->paddr), size(_req->size), dest(_dest),
           addrValid(_req->validPaddr), sizeValid(_req->validSize),
           srcValid(false),
           req(_req), coherence(NULL), senderState(NULL), cmd(_cmd),
           result(Unknown)
    {
    }

    /** Destructor. */
    ~Packet()
    { deleteData(); }

    /** Reinitialize packet address and size from the associated
     *   Request object, and reset other fields that may have been
     *   modified by a previous transaction.  Typically called when a
     *   statically allocated Request/Packet pair is reused for
     *   multiple transactions. */
    void reinitFromRequest() {
        assert(req->validPaddr);
        setAddr(req->paddr);
        assert(req->validSize);
        setSize(req->size);
        result = Unknown;
        if (dynamicData) {
            deleteData();
            dynamicData = false;
            arrayData = false;
        }
    }

    /** Take a request packet and modify it in place to be suitable
     *   for returning as a response to that request.  Used for timing
     *   accesses only.  For atomic and functional accesses, the
     *   request packet is always implicitly passed back *without*
     *   modifying the command or destination fields, so this function
     *   should not be called. */
    void makeTimingResponse() {
        assert(needsResponse());
        int icmd = (int)cmd;
        icmd &= ~(IsRequest | NeedsResponse);
        icmd |= IsResponse;
        cmd = (Command)icmd;
        dest = src;
        srcValid = false;
    }

    /** Set the data pointer to the following value that should not be freed. */
    template <typename T>
    void dataStatic(T *p);

    /** Set the data pointer to a value that should have delete [] called on it.
     */
    template <typename T>
    void dataDynamicArray(T *p);

    /** set the data pointer to a value that should have delete called on it. */
    template <typename T>
    void dataDynamic(T *p);

    /** return the value of what is pointed to in the packet. */
    template <typename T>
    T get();

    /** get a pointer to the data ptr. */
    template <typename T>
    T* getPtr();

    /** set the value in the data pointer to v. */
    template <typename T>
    void set(T v);

    /** delete the data pointed to in the data pointer. Ok to call to matter how
     * data was allocted. */
    void deleteData();

    /** If there isn't data in the packet, allocate some. */
    void allocate();

    /** Do the packet modify the same addresses. */
    bool intersect(Packet *p);
};

bool fixPacket(Packet *func, Packet *timing);
#endif //__MEM_PACKET_HH
