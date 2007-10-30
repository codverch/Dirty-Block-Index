/*
 * Copyright (c) 2003-2006 The Regents of The University of Michigan
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
 * Authors: Gabe Black
 *          Ali Saidi
 */

/*
 * Copyright (c) 2007 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * Redistribution and use of this software in source and binary forms,
 * with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * The software must be used only for Non-Commercial Use which means any
 * use which is NOT directed to receiving any direct monetary
 * compensation for, or commercial advantage from such use.  Illustrative
 * examples of non-commercial use are academic research, personal study,
 * teaching, education and corporate research & development.
 * Illustrative examples of commercial use are distributing products for
 * commercial advantage and providing services using the software for
 * commercial advantage.
 *
 * If you wish to use this software or functionality therein that may be
 * covered by patents for commercial use, please contact:
 *     Director of Intellectual Property Licensing
 *     Office of Strategy and Technology
 *     Hewlett-Packard Company
 *     1501 Page Mill Road
 *     Palo Alto, California  94304
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.  Redistributions
 * in binary form must reproduce the above copyright notice, this list of
 * conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.  Neither the name of
 * the COPYRIGHT HOLDER(s), HEWLETT-PACKARD COMPANY, nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.  No right of
 * sublicense is granted herewith.  Derivatives of the software and
 * output created using the software may be prepared, but only for
 * Non-Commercial Uses.  Derivatives of the software may be shared with
 * others provided: (i) the others agree to abide by the list of
 * conditions herein which includes the Non-Commercial Use restrictions;
 * and (ii) such Derivatives of the software include the above copyright
 * notice to acknowledge the contribution from this software where
 * applicable, this list of conditions and the disclaimer below.
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
 * Authors: Gabe Black
 */

#include "arch/x86/isa_traits.hh"
#include "arch/x86/process.hh"
#include "arch/x86/segmentregs.hh"
#include "arch/x86/types.hh"
#include "base/loader/object_file.hh"
#include "base/loader/elf_object.hh"
#include "base/misc.hh"
#include "base/trace.hh"
#include "cpu/thread_context.hh"
#include "mem/page_table.hh"
#include "mem/translating_port.hh"
#include "sim/process_impl.hh"
#include "sim/system.hh"

using namespace std;
using namespace X86ISA;

M5_64_auxv_t::M5_64_auxv_t(int64_t type, int64_t val)
{
    a_type = TheISA::htog(type);
    a_val = TheISA::htog(val);
}

X86LiveProcess::X86LiveProcess(LiveProcessParams * params,
        ObjectFile *objFile)
    : LiveProcess(params, objFile)
{
    brk_point = objFile->dataBase() + objFile->dataSize() + objFile->bssSize();
    brk_point = roundUp(brk_point, VMPageSize);

    // Set pointer for next thread stack.  Reserve 8M for main stack.
    next_thread_stack_base = stack_base - (8 * 1024 * 1024);

    // Set up stack. On X86_64 Linux, stack goes from the top of memory
    // downward, less the hole for the kernel address space plus one page
    // for undertermined purposes.
    stack_base = (Addr)0x7FFFFFFFF000ULL;

    // Set up region for mmaps. This was determined empirically and may not
    // always be correct.
    mmap_start = mmap_end = (Addr)0x2aaaaaaab000ULL;
}

void X86LiveProcess::handleTrap(int trapNum, ThreadContext *tc)
{
    switch(trapNum)
    {
      default:
        panic("Unimplemented trap to operating system: trap number %#x.\n", trapNum);
    }
}

void
X86LiveProcess::startup()
{
    if (checkpointRestored)
        return;

    argsInit(sizeof(IntReg), VMPageSize);

    for (int i = 0; i < threadContexts.size(); i++) {
        ThreadContext * tc = threadContexts[i];

        SegAttr dataAttr = 0;
        dataAttr.writable = 1;
        dataAttr.readable = 1;
        dataAttr.expandDown = 0;
        dataAttr.dpl = 3;
        dataAttr.defaultSize = 0;
        dataAttr.longMode = 1;

        //Initialize the segment registers.
        for(int seg = 0; seg < NUM_SEGMENTREGS; seg++) {
            tc->setMiscRegNoEffect(MISCREG_SEG_BASE(seg), 0);
            tc->setMiscRegNoEffect(MISCREG_SEG_ATTR(seg), dataAttr);
        }

        SegAttr csAttr = 0;
        csAttr.writable = 0;
        csAttr.readable = 1;
        csAttr.expandDown = 0;
        csAttr.dpl = 3;
        csAttr.defaultSize = 0;
        csAttr.longMode = 1;

        tc->setMiscRegNoEffect(MISCREG_CS_ATTR, csAttr);

        //Set up the registers that describe the operating mode.
        CR0 cr0 = 0;
        cr0.pg = 1; // Turn on paging.
        cr0.cd = 0; // Don't disable caching.
        cr0.nw = 0; // This is bit is defined to be ignored.
        cr0.am = 0; // No alignment checking
        cr0.wp = 0; // Supervisor mode can write read only pages
        cr0.ne = 1;
        cr0.et = 1; // This should always be 1
        cr0.ts = 0; // We don't do task switching, so causing fp exceptions
                    // would be pointless.
        cr0.em = 0; // Allow x87 instructions to execute natively.
        cr0.mp = 1; // This doesn't really matter, but the manual suggests
                    // setting it to one.
        cr0.pe = 1; // We're definitely in protected mode.
        tc->setMiscReg(MISCREG_CR0, cr0);

        Efer efer = 0;
        efer.sce = 1; // Enable system call extensions.
        efer.lme = 1; // Enable long mode.
        efer.lma = 1; // Activate long mode.
        efer.nxe = 1; // Enable nx support.
        efer.svme = 0; // Disable svm support for now. It isn't implemented.
        efer.ffxsr = 1; // Turn on fast fxsave and fxrstor.
        tc->setMiscReg(MISCREG_EFER, efer);
    }
}

void
X86LiveProcess::argsInit(int intSize, int pageSize)
{
    typedef M5_64_auxv_t auxv_t;
    Process::startup();

    string filename;
    if(argv.size() < 1)
        filename = "";
    else
        filename = argv[0];

    //We want 16 byte alignment
    uint64_t align = 16;

    // load object file into target memory
    objFile->loadSections(initVirtMem);

    enum X86CpuFeature {
        X86_OnboardFPU = 1 << 0,
        X86_VirtualModeExtensions = 1 << 1,
        X86_DebuggingExtensions = 1 << 2,
        X86_PageSizeExtensions = 1 << 3,

        X86_TimeStampCounter = 1 << 4,
        X86_ModelSpecificRegisters = 1 << 5,
        X86_PhysicalAddressExtensions = 1 << 6,
        X86_MachineCheckExtensions = 1 << 7,

        X86_CMPXCHG8Instruction = 1 << 8,
        X86_OnboardAPIC = 1 << 9,
        X86_SYSENTER_SYSEXIT = 1 << 11,

        X86_MemoryTypeRangeRegisters = 1 << 12,
        X86_PageGlobalEnable = 1 << 13,
        X86_MachineCheckArchitecture = 1 << 14,
        X86_CMOVInstruction = 1 << 15,

        X86_PageAttributeTable = 1 << 16,
        X86_36BitPSEs = 1 << 17,
        X86_ProcessorSerialNumber = 1 << 18,
        X86_CLFLUSHInstruction = 1 << 19,

        X86_DebugTraceStore = 1 << 21,
        X86_ACPIViaMSR = 1 << 22,
        X86_MultimediaExtensions = 1 << 23,

        X86_FXSAVE_FXRSTOR = 1 << 24,
        X86_StreamingSIMDExtensions = 1 << 25,
        X86_StreamingSIMDExtensions2 = 1 << 26,
        X86_CPUSelfSnoop = 1 << 27,

        X86_HyperThreading = 1 << 28,
        X86_AutomaticClockControl = 1 << 29,
        X86_IA64Processor = 1 << 30
    };

    //Setup the auxilliary vectors. These will already have endian conversion.
    //Auxilliary vectors are loaded only for elf formatted executables.
    ElfObject * elfObject = dynamic_cast<ElfObject *>(objFile);
    if(elfObject)
    {
        uint64_t features =
            X86_OnboardFPU |
            X86_VirtualModeExtensions |
            X86_DebuggingExtensions |
            X86_PageSizeExtensions |
            X86_TimeStampCounter |
            X86_ModelSpecificRegisters |
            X86_PhysicalAddressExtensions |
            X86_MachineCheckExtensions |
            X86_CMPXCHG8Instruction |
            X86_OnboardAPIC |
            X86_SYSENTER_SYSEXIT |
            X86_MemoryTypeRangeRegisters |
            X86_PageGlobalEnable |
            X86_MachineCheckArchitecture |
            X86_CMOVInstruction |
            X86_PageAttributeTable |
            X86_36BitPSEs |
//            X86_ProcessorSerialNumber |
            X86_CLFLUSHInstruction |
//            X86_DebugTraceStore |
//            X86_ACPIViaMSR |
            X86_MultimediaExtensions |
            X86_FXSAVE_FXRSTOR |
            X86_StreamingSIMDExtensions |
            X86_StreamingSIMDExtensions2 |
//            X86_CPUSelfSnoop |
//            X86_HyperThreading |
//            X86_AutomaticClockControl |
//            X86_IA64Processor |
            0;

        //Bits which describe the system hardware capabilities
        //XXX Figure out what these should be
        auxv.push_back(auxv_t(M5_AT_HWCAP, features));
        //The system page size
        auxv.push_back(auxv_t(M5_AT_PAGESZ, X86ISA::VMPageSize));
        //Frequency at which times() increments
        auxv.push_back(auxv_t(M5_AT_CLKTCK, 100));
        // For statically linked executables, this is the virtual address of the
        // program header tables if they appear in the executable image
        auxv.push_back(auxv_t(M5_AT_PHDR, elfObject->programHeaderTable()));
        // This is the size of a program header entry from the elf file.
        auxv.push_back(auxv_t(M5_AT_PHENT, elfObject->programHeaderSize()));
        // This is the number of program headers from the original elf file.
        auxv.push_back(auxv_t(M5_AT_PHNUM, elfObject->programHeaderCount()));
        //Defined to be 100 in the kernel source.
        //This is the address of the elf "interpreter", It should be set
        //to 0 for regular executables. It should be something else
        //(not sure what) for dynamic libraries.
        auxv.push_back(auxv_t(M5_AT_BASE, 0));

        //XXX Figure out what this should be.
        auxv.push_back(auxv_t(M5_AT_FLAGS, 0));
        //The entry point to the program
        auxv.push_back(auxv_t(M5_AT_ENTRY, objFile->entryPoint()));
        //Different user and group IDs
        auxv.push_back(auxv_t(M5_AT_UID, uid()));
        auxv.push_back(auxv_t(M5_AT_EUID, euid()));
        auxv.push_back(auxv_t(M5_AT_GID, gid()));
        auxv.push_back(auxv_t(M5_AT_EGID, egid()));
        //Whether to enable "secure mode" in the executable
        auxv.push_back(auxv_t(M5_AT_SECURE, 0));
        //The string "x86_64" with unknown meaning
        auxv.push_back(auxv_t(M5_AT_PLATFORM, 0));
    }

    //Figure out how big the initial stack needs to be

    // A sentry NULL void pointer at the top of the stack.
    int sentry_size = intSize;

    //This is the name of the file which is present on the initial stack
    //It's purpose is to let the user space linker examine the original file.
    int file_name_size = filename.size() + 1;

    string platform = "x86_64";
    int aux_data_size = platform.size() + 1;

    int env_data_size = 0;
    for (int i = 0; i < envp.size(); ++i) {
        env_data_size += envp[i].size() + 1;
    }
    int arg_data_size = 0;
    for (int i = 0; i < argv.size(); ++i) {
        arg_data_size += argv[i].size() + 1;
    }

    //The info_block needs to be padded so it's size is a multiple of the
    //alignment mask. Also, it appears that there needs to be at least some
    //padding, so if the size is already a multiple, we need to increase it
    //anyway.
    int base_info_block_size =
        sentry_size + file_name_size + env_data_size + arg_data_size;

    int info_block_size = roundUp(base_info_block_size, align);

    int info_block_padding = info_block_size - base_info_block_size;

    //Each auxilliary vector is two 8 byte words
    int aux_array_size = intSize * 2 * (auxv.size() + 1);

    int envp_array_size = intSize * (envp.size() + 1);
    int argv_array_size = intSize * (argv.size() + 1);

    int argc_size = intSize;

    //Figure out the size of the contents of the actual initial frame
    int frame_size =
        aux_array_size +
        envp_array_size +
        argv_array_size +
        argc_size;

    //There needs to be padding after the auxiliary vector data so that the
    //very bottom of the stack is aligned properly.
    int partial_size = frame_size + aux_data_size;
    int aligned_partial_size = roundUp(partial_size, align);
    int aux_padding = aligned_partial_size - partial_size;

    int space_needed =
        info_block_size +
        aux_data_size +
        aux_padding +
        frame_size;

    stack_min = stack_base - space_needed;
    stack_min = roundDown(stack_min, align);
    stack_size = stack_base - stack_min;

    // map memory
    pTable->allocate(roundDown(stack_min, pageSize),
                     roundUp(stack_size, pageSize));

    // map out initial stack contents
    Addr sentry_base = stack_base - sentry_size;
    Addr file_name_base = sentry_base - file_name_size;
    Addr env_data_base = file_name_base - env_data_size;
    Addr arg_data_base = env_data_base - arg_data_size;
    Addr aux_data_base = arg_data_base - info_block_padding - aux_data_size;
    Addr auxv_array_base = aux_data_base - aux_array_size - aux_padding;
    Addr envp_array_base = auxv_array_base - envp_array_size;
    Addr argv_array_base = envp_array_base - argv_array_size;
    Addr argc_base = argv_array_base - argc_size;

    DPRINTF(X86, "The addresses of items on the initial stack:\n");
    DPRINTF(X86, "0x%x - file name\n", file_name_base);
    DPRINTF(X86, "0x%x - env data\n", env_data_base);
    DPRINTF(X86, "0x%x - arg data\n", arg_data_base);
    DPRINTF(X86, "0x%x - aux data\n", aux_data_base);
    DPRINTF(X86, "0x%x - auxv array\n", auxv_array_base);
    DPRINTF(X86, "0x%x - envp array\n", envp_array_base);
    DPRINTF(X86, "0x%x - argv array\n", argv_array_base);
    DPRINTF(X86, "0x%x - argc \n", argc_base);
    DPRINTF(X86, "0x%x - stack min\n", stack_min);

    // write contents to stack

    // figure out argc
    uint64_t argc = argv.size();
    uint64_t guestArgc = TheISA::htog(argc);

    //Write out the sentry void *
    uint64_t sentry_NULL = 0;
    initVirtMem->writeBlob(sentry_base,
            (uint8_t*)&sentry_NULL, sentry_size);

    //Write the file name
    initVirtMem->writeString(file_name_base, filename.c_str());

    //Fix up the aux vector which points to the "platform" string
    assert(auxv[auxv.size() - 1].a_type = M5_AT_PLATFORM);
    auxv[auxv.size() - 1].a_val = aux_data_base;

    //Copy the aux stuff
    for(int x = 0; x < auxv.size(); x++)
    {
        initVirtMem->writeBlob(auxv_array_base + x * 2 * intSize,
                (uint8_t*)&(auxv[x].a_type), intSize);
        initVirtMem->writeBlob(auxv_array_base + (x * 2 + 1) * intSize,
                (uint8_t*)&(auxv[x].a_val), intSize);
    }
    //Write out the terminating zeroed auxilliary vector
    const uint64_t zero = 0;
    initVirtMem->writeBlob(auxv_array_base + 2 * intSize * auxv.size(),
            (uint8_t*)&zero, 2 * intSize);

    initVirtMem->writeString(aux_data_base, platform.c_str());

    copyStringArray(envp, envp_array_base, env_data_base, initVirtMem);
    copyStringArray(argv, argv_array_base, arg_data_base, initVirtMem);

    initVirtMem->writeBlob(argc_base, (uint8_t*)&guestArgc, intSize);

    //Set the stack pointer register
    threadContexts[0]->setIntReg(StackPointerReg, stack_min);

    Addr prog_entry = objFile->entryPoint();
    threadContexts[0]->setPC(prog_entry);
    threadContexts[0]->setNextPC(prog_entry + sizeof(MachInst));

    //Align the "stack_min" to a page boundary.
    stack_min = roundDown(stack_min, pageSize);

//    num_processes++;
}
