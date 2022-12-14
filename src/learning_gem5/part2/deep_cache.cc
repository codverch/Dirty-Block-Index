#include "learning_gem5/part2/deep_cache.hh"

#include "base/compiler.hh"
#include "base/random.hh"
#include "debug/DeepCache.hh"
#include "sim/system.hh"

namespace gem5
{

    DeepCache::DeepCache(const DeepCacheParams &params) : ClockedObject(params),
                                                          latency(params.latency),
                                                          blockSize(params.system->cacheLineSize()),
                                                          capacity(params.size / blockSize),
                                                          memPort(params.name + ".mem_side", this),
                                                          blocked(false), originalPacket(nullptr), waitingPortId(-1), stats(this)
    {
        // Since the CPU side ports are a vector of ports, create an instance of
        // the CPUSidePort for each connection. This member of params is
        // automatically created depending on the name of the vector port and
        // holds the number of connections to this port name
        for (int i = 0; i < params.port_cpu_side_connection_count; ++i)
        {
            cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
        }
    }

    Port &
    DeepCache::getPort(const std::string &if_name, PortID idx)
    {
        // This is the name from the Python SimObject declaration in DeepCache.py
        if (if_name == "mem_side")
        {
            panic_if(idx != InvalidPortID,
                     "Mem side of simple cache not a vector port");
            return memPort;
        }
        else if (if_name == "cpu_side" && idx < cpuPorts.size())
        {
            // We should have already created all of the ports in the constructor
            return cpuPorts[idx];
        }
        else
        {
            // pass it along to our super class
            return ClockedObject::getPort(if_name, idx);
        }
    }

    void
    DeepCache::CPUSidePort::sendPacket(PacketPtr pkt)
    {
        // Note: This flow control is very simple since the cache is blocking.

        panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

        // If we can't send the packet across the port, store it for later.
        DPRINTF(DeepCache, "Sending %s to CPU\n", pkt->print());
        if (!sendTimingResp(pkt))
        {
            DPRINTF(DeepCache, "failed!\n");
            blockedPacket = pkt;
        }
    }

    AddrRangeList
    DeepCache::CPUSidePort::getAddrRanges() const
    {
        return owner->getAddrRanges();
    }

    void
    DeepCache::CPUSidePort::trySendRetry()
    {
        if (needRetry && blockedPacket == nullptr)
        {
            // Only send a retry if the port is now completely free
            needRetry = false;
            DPRINTF(DeepCache, "Sending retry req.\n");
            sendRetryReq();
        }
    }

    void
    DeepCache::CPUSidePort::recvFunctional(PacketPtr pkt)
    {
        // Just forward to the cache.
        return owner->handleFunctional(pkt);
    }

    bool
    DeepCache::CPUSidePort::recvTimingReq(PacketPtr pkt)
    {
        DPRINTF(DeepCache, "Got request %s\n", pkt->print());

        if (blockedPacket || needRetry)
        {
            // The cache may not be able to send a reply if this is blocked
            DPRINTF(DeepCache, "Request blocked\n");
            needRetry = true;
            return false;
        }
        // Just forward to the cache.
        if (!owner->handleRequest(pkt, id))
        {
            DPRINTF(DeepCache, "Request failed\n");
            // stalling
            needRetry = true;
            return false;
        }
        else
        {
            DPRINTF(DeepCache, "Request succeeded\n");
            return true;
        }
    }

    void
    DeepCache::CPUSidePort::recvRespRetry()
    {
        // We should have a blocked packet if this function is called.
        assert(blockedPacket != nullptr);

        // Grab the blocked packet.
        PacketPtr pkt = blockedPacket;
        blockedPacket = nullptr;

        DPRINTF(DeepCache, "Retrying response pkt %s\n", pkt->print());
        // Try to resend it. It's possible that it fails again.
        sendPacket(pkt);

        // We may now be able to accept new packets
        trySendRetry();
    }

    void
    DeepCache::MemSidePort::sendPacket(PacketPtr pkt)
    {
        // Note: This flow control is very simple since the cache is blocking.

        panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

        // If we can't send the packet across the port, store it for later.
        if (!sendTimingReq(pkt))
        {
            blockedPacket = pkt;
        }
    }

    bool
    DeepCache::MemSidePort::recvTimingResp(PacketPtr pkt)
    {
        // Just forward to the cache.
        return owner->handleResponse(pkt);
    }

    void
    DeepCache::MemSidePort::recvReqRetry()
    {
        // We should have a blocked packet if this function is called.
        assert(blockedPacket != nullptr);

        // Grab the blocked packet.
        PacketPtr pkt = blockedPacket;
        blockedPacket = nullptr;

        // Try to resend it. It's possible that it fails again.
        sendPacket(pkt);
    }

    void
    DeepCache::MemSidePort::recvRangeChange()
    {
        owner->sendRangeChange();
    }

    bool
    DeepCache::handleRequest(PacketPtr pkt, int port_id)
    {
        if (blocked)
        {
            // There is currently an outstanding request so we can't respond. Stall
            return false;
        }

        DPRINTF(DeepCache, "Got request for addr %#x\n", pkt->getAddr());

        // This cache is now blocked waiting for the response to this packet.
        blocked = true;

        // Store the port for when we get the response
        assert(waitingPortId == -1);
        waitingPortId = port_id;

        // Schedule an event after cache access latency to actually access
        schedule(new EventFunctionWrapper([this, pkt]
                                          { accessTiming(pkt); },
                                          name() + ".accessEvent", true),
                 clockEdge(latency));

        return true;
    }

    bool
    DeepCache::handleResponse(PacketPtr pkt)
    {
        assert(blocked);
        DPRINTF(DeepCache, "Got response for addr %#x\n", pkt->getAddr());

        // For now assume that inserts are off of the critical path and don't count
        // for any added latency.
        insert(pkt);

        stats.missLatency.sample(curTick() - missTime);

        // If we had to upgrade the request packet to a full cache line, now we
        // can use that packet to construct the response.
        if (originalPacket != nullptr)
        {
            DPRINTF(DeepCache, "Copying data from new packet to old\n");
            // We had to upgrade a previous packet. We can functionally deal with
            // the cache access now. It better be a hit.
            [[maybe_unused]] bool hit = accessFunctional(originalPacket);
            panic_if(!hit, "Should always hit after inserting");
            originalPacket->makeResponse();
            delete pkt; // We may need to delay this, I'm not sure.
            pkt = originalPacket;
            originalPacket = nullptr;
        } // else, pkt contains the data it needs

        sendResponse(pkt);

        return true;
    }

    void DeepCache::sendResponse(PacketPtr pkt)
    {
        assert(blocked);
        DPRINTF(DeepCache, "Sending resp for addr %#x\n", pkt->getAddr());

        int port = waitingPortId;

        // The packet is now done. We're about to put it in the port, no need for
        // this object to continue to stall.
        // We need to free the resource before sending the packet in case the CPU
        // tries to send another request immediately (e.g., in the same callchain).
        blocked = false;
        waitingPortId = -1;

        // Simply forward to the memory port
        cpuPorts[port].sendPacket(pkt);

        // For each of the cpu ports, if it needs to send a retry, it should do it
        // now since this memory object may be unblocked now.
        for (auto &port : cpuPorts)
        {
            port.trySendRetry();
        }
    }

    void
    DeepCache::handleFunctional(PacketPtr pkt)
    {
        if (accessFunctional(pkt))
        {
            pkt->makeResponse();
        }
        else
        {
            memPort.sendFunctional(pkt);
        }
    }

    void
    DeepCache::accessTiming(PacketPtr pkt)
    {
        bool hit = accessFunctional(pkt);

        DPRINTF(DeepCache, "%s for packet: %s\n", hit ? "Hit" : "Miss",
                pkt->print());

        if (hit)
        {
            // Respond to the CPU side
            stats.hits++; // update stats
            DDUMP(DeepCache, pkt->getConstPtr<uint8_t>(), pkt->getSize());
            pkt->makeResponse();
            sendResponse(pkt);
        }
        else
        {
            stats.misses++; // update stats
            missTime = curTick();
            // Forward to the memory side.
            // We can't directly forward the packet unless it is exactly the size
            // of the cache line, and aligned. Check for that here.
            Addr addr = pkt->getAddr();
            Addr block_addr = pkt->getBlockAddr(blockSize);
            unsigned size = pkt->getSize();
            if (addr == block_addr && size == blockSize)
            {
                // Aligned and block size. We can just forward.
                DPRINTF(DeepCache, "forwarding packet\n");
                memPort.sendPacket(pkt);
            }
            else
            {
                DPRINTF(DeepCache, "Upgrading packet to block size\n");
                panic_if(addr - block_addr + size > blockSize,
                         "Cannot handle accesses that span multiple cache lines");
                // Unaligned access to one cache block
                assert(pkt->needsResponse());
                MemCmd cmd;
                if (pkt->isWrite() || pkt->isRead())
                {
                    // Read the data from memory to write into the block.
                    // We'll write the data in the cache (i.e., a writeback cache)
                    cmd = MemCmd::ReadReq;
                }
                else
                {
                    panic("Unknown packet type in upgrade size");
                }

                // Create a new packet that is blockSize
                PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
                new_pkt->allocate();

                // Should now be block aligned
                assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));

                // Save the old packet
                originalPacket = pkt;

                DPRINTF(DeepCache, "forwarding packet\n");
                memPort.sendPacket(new_pkt);
            }
        }
    }

    bool
    DeepCache::accessFunctional(PacketPtr pkt)
    {
        Addr block_addr = pkt->getBlockAddr(blockSize);
        auto it = cacheStore.find(block_addr);
        if (it != cacheStore.end())
        {
            if (pkt->isWrite())
            {
                // Write the data into the block in the cache
                pkt->writeDataToBlock(it->second, blockSize);
            }
            else if (pkt->isRead())
            {
                // Read the data out of the cache block into the packet
                pkt->setDataFromBlock(it->second, blockSize);
            }
            else
            {
                panic("Unknown packet type!");
            }
            return true;
        }
        return false;
    }

    void
    DeepCache::insert(PacketPtr pkt)
    {
        // The packet should be aligned.
        assert(pkt->getAddr() == pkt->getBlockAddr(blockSize));
        // The address should not be in the cache
        assert(cacheStore.find(pkt->getAddr()) == cacheStore.end());
        // The pkt should be a response
        assert(pkt->isResponse());

        if (cacheStore.size() >= capacity)
        {
            // Select random thing to evict. This is a little convoluted since we
            // are using a std::unordered_map. See http://bit.ly/2hrnLP2
            int bucket, bucket_size;
            do
            {
                bucket = random_mt.random(0, (int)cacheStore.bucket_count() - 1);
            } while ((bucket_size = cacheStore.bucket_size(bucket)) == 0);
            auto block = std::next(cacheStore.begin(bucket),
                                   random_mt.random(0, bucket_size - 1));

            DPRINTF(DeepCache, "Removing addr %#x\n", block->first);

            // Write back the data.
            // Create a new request-packet pair
            RequestPtr req = std::make_shared<Request>(
                block->first, blockSize, 0, 0);

            PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
            new_pkt->dataDynamic(block->second); // This will be deleted later

            DPRINTF(DeepCache, "Writing packet back %s\n", pkt->print());
            // Send the write to memory
            memPort.sendPacket(new_pkt);

            // Delete this entry
            cacheStore.erase(block->first);
        }

        DPRINTF(DeepCache, "Inserting %s\n", pkt->print());
        DDUMP(DeepCache, pkt->getConstPtr<uint8_t>(), blockSize);

        // Allocate space for the cache block data
        uint8_t *data = new uint8_t[blockSize];

        // Insert the data and address into the cache store
        cacheStore[pkt->getAddr()] = data;

        // Write the data into the cache
        pkt->writeDataToBlock(data, blockSize);
    }

    AddrRangeList
    DeepCache::getAddrRanges() const
    {
        DPRINTF(DeepCache, "Sending new ranges\n");
        // Just use the same ranges as whatever is on the memory side.
        return memPort.getAddrRanges();
    }

    void
    DeepCache::sendRangeChange() const
    {
        for (auto &port : cpuPorts)
        {
            port.sendRangeChange();
        }
    }

    DeepCache::DeepCacheStats::DeepCacheStats(statistics::Group *parent)
        : statistics::Group(parent),
          ADD_STAT(hits, statistics::units::Count::get(), "Number of hits"),
          ADD_STAT(misses, statistics::units::Count::get(), "Number of misses"),
          ADD_STAT(missLatency, statistics::units::Tick::get(),
                   "Ticks for misses to the cache"),
          ADD_STAT(hitRatio, statistics::units::Ratio::get(),
                   "The ratio of hits to the total accesses to the cache",
                   hits / (hits + misses))
    {
        missLatency.init(16); // number of buckets
    }

} // namespace gem5
