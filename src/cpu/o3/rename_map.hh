/*
 * Copyright (c) 2015 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
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
 * Authors: Kevin Lim
 *          Steve Reinhardt
 */

#ifndef __CPU_O3_RENAME_MAP_HH__
#define __CPU_O3_RENAME_MAP_HH__

#include <iostream>
#include <utility>
#include <vector>

#include "arch/types.hh"
#include "config/the_isa.hh"
#include "cpu/o3/free_list.hh"
#include "cpu/o3/regfile.hh"
#include "cpu/reg_class.hh"

/**
 * Register rename map for a single class of registers (e.g., integer
 * or floating point).  Because the register class is implicitly
 * determined by the rename map instance being accessed, all
 * architectural register index parameters and values in this class
 * are relative (e.g., %fp2 is just index 2).
 */
class SimpleRenameMap
{
  private:

    /** The acutal arch-to-phys register map */
    std::vector<PhysRegIdPtr> map;

    /**
     * Pointer to the free list from which new physical registers
     * should be allocated in rename()
     */
    SimpleFreeList *freeList;

    /**
     * The architectural index of the zero register. This register is
     * mapped but read-only, so we ignore attempts to rename it via
     * the rename() method.  If there is no such register for this map
     * table, it should be set to an invalid index so that it never
     * matches.
     */
    RegIndex zeroReg;

  public:

    SimpleRenameMap();

    ~SimpleRenameMap() {};

    /**
     * Because we have an array of rename maps (one per thread) in the CPU,
     * it's awkward to initialize this object via the constructor.
     * Instead, this method is used for initialization.
     */
    void init(unsigned size, SimpleFreeList *_freeList, RegIndex _zeroReg);

    /**
     * Pair of a physical register and a physical register.  Used to
     * return the physical register that a logical register has been
     * renamed to, and the previous physical register that the same
     * logical register was previously mapped to.
     */
    typedef std::pair<PhysRegIdPtr, PhysRegIdPtr> RenameInfo;

    /**
     * Tell rename map to get a new free physical register to remap
     * the specified architectural register.
     * @param arch_reg The architectural register to remap.
     * @return A RenameInfo pair indicating both the new and previous
     * physical registers.
     */
    RenameInfo rename(RegIndex arch_reg);

    /**
     * Look up the physical register mapped to an architectural register.
     * @param arch_reg The architectural register to look up.
     * @return The physical register it is currently mapped to.
     */
    PhysRegIdPtr lookup(RegIndex arch_reg) const
    {
        assert(arch_reg < map.size());
        return map[arch_reg];
    }

    /**
     * Update rename map with a specific mapping.  Generally used to
     * roll back to old mappings on a squash.
     * @param arch_reg The architectural register to remap.
     * @param phys_reg The physical register to remap it to.
     */
    void setEntry(RegIndex arch_reg, PhysRegIdPtr phys_reg)
    {
        map[arch_reg] = phys_reg;
    }

    /** Return the number of free entries on the associated free list. */
    unsigned numFreeEntries() const { return freeList->numFreeRegs(); }
};


/**
 * Unified register rename map for all classes of registers.  Wraps a
 * set of class-specific rename maps.  Methods that do not specify a
 * register class (e.g., rename()) take register ids,
 * while methods that do specify a register class (e.g., renameInt())
 * take register indices.
 */
class UnifiedRenameMap
{
  private:

    /** The integer register rename map */
    SimpleRenameMap intMap;

    /** The floating-point register rename map */
    SimpleRenameMap floatMap;

    /** The condition-code register rename map */
    SimpleRenameMap ccMap;

    /**
     * The register file object is used only to get PhysRegIdPtr
     * on MiscRegs, as they are stored in it.
     */
    PhysRegFile *regFile;

  public:

    typedef SimpleRenameMap::RenameInfo RenameInfo;

    /** Default constructor.  init() must be called prior to use. */
    UnifiedRenameMap() : regFile(nullptr) {};

    /** Destructor. */
    ~UnifiedRenameMap() {};

    /** Initializes rename map with given parameters. */
    void init(PhysRegFile *_regFile,
              RegIndex _intZeroReg,
              RegIndex _floatZeroReg,
              UnifiedFreeList *freeList);

    /**
     * Tell rename map to get a new free physical register to remap
     * the specified architectural register. This version takes a
     * flattened architectural register id and calls the
     * appropriate class-specific rename table.
     * @param arch_reg The architectural register index to remap.
     * @return A RenameInfo pair indicating both the new and previous
     * physical registers.
     */
    RenameInfo rename(RegId arch_reg);

    /**
     * Perform rename() on an integer register, given a
     * integer register index.
     */
    RenameInfo renameInt(RegIndex rel_arch_reg)
    {
        return intMap.rename(rel_arch_reg);
    }

    /**
     * Perform rename() on a floating-point register, given a
     * floating-point register index.
     */
    RenameInfo renameFloat(RegIndex rel_arch_reg)
    {
        return floatMap.rename(rel_arch_reg);
    }

    /**
     * Perform rename() on a condition-code register, given a
     * condition-code register index.
     */
    RenameInfo renameCC(RegIndex rel_arch_reg)
    {
        return ccMap.rename(rel_arch_reg);
    }

    /**
     * Perform rename() on a misc register, given a
     * misc register index.
     */
    RenameInfo renameMisc(RegIndex rel_arch_reg)
    {
        // misc regs aren't really renamed, just remapped
        PhysRegIdPtr phys_reg = lookupMisc(rel_arch_reg);
        // Set the new register to the previous one to keep the same
        // mapping throughout the execution.
        return RenameInfo(phys_reg, phys_reg);
    }


    /**
     * Look up the physical register mapped to an architectural register.
     * This version takes a flattened architectural register id
     * and calls the appropriate class-specific rename table.
     * @param arch_reg The architectural register to look up.
     * @return The physical register it is currently mapped to.
     */
    PhysRegIdPtr lookup(RegId arch_reg) const;

    /**
     * Perform lookup() on an integer register, given a
     * integer register index.
     */
    PhysRegIdPtr lookupInt(RegIndex rel_arch_reg) const
    {
        return intMap.lookup(rel_arch_reg);
    }

    /**
     * Perform lookup() on a floating-point register, given a
     * floating-point register index.
     */
    PhysRegIdPtr lookupFloat(RegIndex rel_arch_reg) const
    {
        return  floatMap.lookup(rel_arch_reg);
    }

    /**
     * Perform lookup() on a condition-code register, given a
     * condition-code register index.
     */
    PhysRegIdPtr lookupCC(RegIndex rel_arch_reg) const
    {
        return ccMap.lookup(rel_arch_reg);
    }

    /**
     * Perform lookup() on a misc register, given a relative
     * misc register index.
     */
    PhysRegIdPtr lookupMisc(RegIndex rel_arch_reg) const
    {
        // misc regs aren't really renamed, they keep the same
        // mapping throughout the execution.
        return regFile->getMiscRegId(rel_arch_reg);
    }

    /**
     * Update rename map with a specific mapping.  Generally used to
     * roll back to old mappings on a squash.  This version takes a
     * flattened architectural register id and calls the
     * appropriate class-specific rename table.
     * @param arch_reg The architectural register to remap.
     * @param phys_reg The physical register to remap it to.
     */
    void setEntry(RegId arch_reg, PhysRegIdPtr phys_reg);

    /**
     * Perform setEntry() on an integer register, given a
     * integer register index.
     */
    void setIntEntry(RegIndex arch_reg, PhysRegIdPtr phys_reg)
    {
        assert(phys_reg->isIntPhysReg());
        intMap.setEntry(arch_reg, phys_reg);
    }

    /**
     * Perform setEntry() on a floating-point register, given a
     * floating-point register index.
     */
    void setFloatEntry(RegIndex arch_reg, PhysRegIdPtr phys_reg)
    {
        assert(phys_reg->isFloatPhysReg());
        floatMap.setEntry(arch_reg, phys_reg);
    }

    /**
     * Perform setEntry() on a condition-code register, given a
     * condition-code register index.
     */
    void setCCEntry(RegIndex arch_reg, PhysRegIdPtr phys_reg)
    {
        assert(phys_reg->isCCPhysReg());
        ccMap.setEntry(arch_reg, phys_reg);
    }

    /**
     * Return the minimum number of free entries across all of the
     * register classes.  The minimum is used so we guarantee that
     * this number of entries is available regardless of which class
     * of registers is requested.
     */
    unsigned numFreeEntries() const
    {
        return std::min(intMap.numFreeEntries(), floatMap.numFreeEntries());
    }

    /**
     * Return whether there are enough registers to serve the request.
     */
    bool canRename(uint32_t intRegs, uint32_t floatRegs, uint32_t ccRegs) const
    {
        return intRegs <= intMap.numFreeEntries() &&
            floatRegs <= floatMap.numFreeEntries() &&
            ccRegs <= ccMap.numFreeEntries();
    }

};

#endif //__CPU_O3_RENAME_MAP_HH__
