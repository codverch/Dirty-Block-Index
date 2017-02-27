/*
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
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
 *          Ali Saidi
 */

#include "sim/syscall_emul.hh"

#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "arch/utility.hh"
#include "base/chunk_generator.hh"
#include "base/trace.hh"
#include "config/the_isa.hh"
#include "cpu/thread_context.hh"
#include "mem/page_table.hh"
#include "sim/process.hh"
#include "sim/sim_exit.hh"
#include "sim/syscall_debug_macros.hh"
#include "sim/syscall_desc.hh"
#include "sim/system.hh"

using namespace std;
using namespace TheISA;

SyscallReturn
unimplementedFunc(SyscallDesc *desc, int callnum, Process *process,
                  ThreadContext *tc)
{
    fatal("syscall %s (#%d) unimplemented.", desc->name(), callnum);

    return 1;
}


SyscallReturn
ignoreFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    if (desc->needWarning()) {
        warn("ignoring syscall %s(...)%s", desc->name(), desc->warnOnce() ?
             "\n      (further warnings will be suppressed)" : "");
    }

    return 0;
}

static void
exitFutexWake(ThreadContext *tc, uint64_t uaddr)
{
    std::map<uint64_t, std::list<ThreadContext *> * >
        &futex_map = tc->getSystemPtr()->futexMap;

    int wokenUp = 0;
    std::list<ThreadContext *> * tcWaitList;
    if (futex_map.count(uaddr)) {
        tcWaitList = futex_map.find(uaddr)->second;
        if (tcWaitList->size() > 0) {
            tcWaitList->front()->activate();
            tcWaitList->pop_front();
            wokenUp++;
        }
        if (tcWaitList->empty()) {
            futex_map.erase(uaddr);
            delete tcWaitList;
        }
    }
    DPRINTF(SyscallVerbose, "exit: FUTEX_WAKE, activated %d waiting "
                            "thread contexts\n", wokenUp);
}

SyscallReturn
exitFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    if (p->system->numRunningContexts() == 1 && !p->childClearTID) {
        // Last running free-parent context; exit simulator.
        int index = 0;
        exitSimLoop("target called exit()",
                    p->getSyscallArg(tc, index) & 0xff);
    } else {
        if (p->childClearTID)
            exitFutexWake(tc, p->childClearTID);
        tc->halt();
    }

    return 1;
}


SyscallReturn
exitGroupFunc(SyscallDesc *desc, int callnum, Process *process,
              ThreadContext *tc)
{
    // halt all threads belonging to this process
    for (auto i: process->contextIds) {
        process->system->getThreadContext(i)->halt();
    }

    if (!process->system->numRunningContexts()) {
        // all threads belonged to this process... exit simulator
        int index = 0;
        exitSimLoop("target called exit()",
                    process->getSyscallArg(tc, index) & 0xff);
    }

    return 1;
}


SyscallReturn
getpagesizeFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    return (int)PageBytes;
}


SyscallReturn
brkFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    // change brk addr to first arg
    int index = 0;
    Addr new_brk = p->getSyscallArg(tc, index);

    // in Linux at least, brk(0) returns the current break value
    // (note that the syscall and the glibc function have different behavior)
    if (new_brk == 0)
        return p->memState->brkPoint;

    if (new_brk > p->memState->brkPoint) {
        // might need to allocate some new pages
        for (ChunkGenerator gen(p->memState->brkPoint,
                                new_brk - p->memState->brkPoint,
                                PageBytes); !gen.done(); gen.next()) {
            if (!p->pTable->translate(gen.addr()))
                p->allocateMem(roundDown(gen.addr(), PageBytes), PageBytes);

            // if the address is already there, zero it out
            else {
                uint8_t zero  = 0;
                SETranslatingPortProxy &tp = tc->getMemProxy();

                // split non-page aligned accesses
                Addr next_page = roundUp(gen.addr(), PageBytes);
                uint32_t size_needed = next_page - gen.addr();
                tp.memsetBlob(gen.addr(), zero, size_needed);
                if (gen.addr() + PageBytes > next_page &&
                    next_page < new_brk &&
                    p->pTable->translate(next_page))
                {
                    size_needed = PageBytes - size_needed;
                    tp.memsetBlob(next_page, zero, size_needed);
                }
            }
        }
    }

    p->memState->brkPoint = new_brk;
    DPRINTF_SYSCALL(Verbose, "brk: break point changed to: %#X\n",
                    p->memState->brkPoint);
    return p->memState->brkPoint;
}

SyscallReturn
setTidAddressFunc(SyscallDesc *desc, int callnum, Process *process,
                  ThreadContext *tc)
{
    int index = 0;
    uint64_t tidPtr = process->getSyscallArg(tc, index);

    process->childClearTID = tidPtr;
    return process->pid();
}

SyscallReturn
closeFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);

    return p->fds->closeFDEntry(tgt_fd);
}


SyscallReturn
readFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr bufPtr = p->getSyscallArg(tc, index);
    int nbytes = p->getSyscallArg(tc, index);

    auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
    if (!hbfdp)
        return -EBADF;
    int sim_fd = hbfdp->getSimFD();

    BufferArg bufArg(bufPtr, nbytes);
    int bytes_read = read(sim_fd, bufArg.bufferPtr(), nbytes);

    if (bytes_read > 0)
        bufArg.copyOut(tc->getMemProxy());

    return bytes_read;
}

SyscallReturn
writeFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr bufPtr = p->getSyscallArg(tc, index);
    int nbytes = p->getSyscallArg(tc, index);

    auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
    if (!hbfdp)
        return -EBADF;
    int sim_fd = hbfdp->getSimFD();

    BufferArg bufArg(bufPtr, nbytes);
    bufArg.copyIn(tc->getMemProxy());

    int bytes_written = write(sim_fd, bufArg.bufferPtr(), nbytes);

    fsync(sim_fd);

    return bytes_written;
}


SyscallReturn
lseekFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    uint64_t offs = p->getSyscallArg(tc, index);
    int whence = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    off_t result = lseek(sim_fd, offs, whence);

    return (result == (off_t)-1) ? -errno : result;
}


SyscallReturn
_llseekFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    uint64_t offset_high = p->getSyscallArg(tc, index);
    uint32_t offset_low = p->getSyscallArg(tc, index);
    Addr result_ptr = p->getSyscallArg(tc, index);
    int whence = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    uint64_t offset = (offset_high << 32) | offset_low;

    uint64_t result = lseek(sim_fd, offset, whence);
    result = TheISA::htog(result);

    if (result == (off_t)-1)
        return -errno;
    // Assuming that the size of loff_t is 64 bits on the target platform
    BufferArg result_buf(result_ptr, sizeof(result));
    memcpy(result_buf.bufferPtr(), &result, sizeof(result));
    result_buf.copyOut(tc->getMemProxy());
    return 0;
}


SyscallReturn
munmapFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    // With mmap more fully implemented, it might be worthwhile to bite
    // the bullet and implement munmap. Should allow us to reuse simulated
    // memory.
    return 0;
}


const char *hostname = "m5.eecs.umich.edu";

SyscallReturn
gethostnameFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    Addr bufPtr = p->getSyscallArg(tc, index);
    int name_len = p->getSyscallArg(tc, index);
    BufferArg name(bufPtr, name_len);

    strncpy((char *)name.bufferPtr(), hostname, name_len);

    name.copyOut(tc->getMemProxy());

    return 0;
}

SyscallReturn
getcwdFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int result = 0;
    int index = 0;
    Addr bufPtr = p->getSyscallArg(tc, index);
    unsigned long size = p->getSyscallArg(tc, index);
    BufferArg buf(bufPtr, size);

    // Is current working directory defined?
    string cwd = p->getcwd();
    if (!cwd.empty()) {
        if (cwd.length() >= size) {
            // Buffer too small
            return -ERANGE;
        }
        strncpy((char *)buf.bufferPtr(), cwd.c_str(), size);
        result = cwd.length();
    } else {
        if (getcwd((char *)buf.bufferPtr(), size)) {
            result = strlen((char *)buf.bufferPtr());
        } else {
            result = -1;
        }
    }

    buf.copyOut(tc->getMemProxy());

    return (result == -1) ? -errno : result;
}

/// Target open() handler.
SyscallReturn
readlinkFunc(SyscallDesc *desc, int callnum, Process *process,
             ThreadContext *tc)
{
    return readlinkFunc(desc, callnum, process, tc, 0);
}

SyscallReturn
readlinkFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc,
             int index)
{
    string path;

    if (!tc->getMemProxy().tryReadString(path, p->getSyscallArg(tc, index)))
        return -EFAULT;

    // Adjust path for current working directory
    path = p->fullPath(path);

    Addr bufPtr = p->getSyscallArg(tc, index);
    size_t bufsiz = p->getSyscallArg(tc, index);

    BufferArg buf(bufPtr, bufsiz);

    int result = -1;
    if (path != "/proc/self/exe") {
        result = readlink(path.c_str(), (char *)buf.bufferPtr(), bufsiz);
    } else {
        // Emulate readlink() called on '/proc/self/exe' should return the
        // absolute path of the binary running in the simulated system (the
        // Process' executable). It is possible that using this path in
        // the simulated system will result in unexpected behavior if:
        //  1) One binary runs another (e.g., -c time -o "my_binary"), and
        //     called binary calls readlink().
        //  2) The host's full path to the running benchmark changes from one
        //     simulation to another. This can result in different simulated
        //     performance since the simulated system will process the binary
        //     path differently, even if the binary itself does not change.

        // Get the absolute canonical path to the running application
        char real_path[PATH_MAX];
        char *check_real_path = realpath(p->progName(), real_path);
        if (!check_real_path) {
            fatal("readlink('/proc/self/exe') unable to resolve path to "
                  "executable: %s", p->progName());
        }
        strncpy((char*)buf.bufferPtr(), real_path, bufsiz);
        size_t real_path_len = strlen(real_path);
        if (real_path_len > bufsiz) {
            // readlink will truncate the contents of the
            // path to ensure it is no more than bufsiz
            result = bufsiz;
        } else {
            result = real_path_len;
        }

        // Issue a warning about potential unexpected results
        warn_once("readlink() called on '/proc/self/exe' may yield unexpected "
                  "results in various settings.\n      Returning '%s'\n",
                  (char*)buf.bufferPtr());
    }

    buf.copyOut(tc->getMemProxy());

    return (result == -1) ? -errno : result;
}

SyscallReturn
unlinkFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    return unlinkHelper(desc, num, p, tc, 0);
}

SyscallReturn
unlinkHelper(SyscallDesc *desc, int num, Process *p, ThreadContext *tc,
             int index)
{
    string path;

    if (!tc->getMemProxy().tryReadString(path, p->getSyscallArg(tc, index)))
        return -EFAULT;

    // Adjust path for current working directory
    path = p->fullPath(path);

    int result = unlink(path.c_str());
    return (result == -1) ? -errno : result;
}


SyscallReturn
mkdirFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path, p->getSyscallArg(tc, index)))
        return -EFAULT;

    // Adjust path for current working directory
    path = p->fullPath(path);

    mode_t mode = p->getSyscallArg(tc, index);

    int result = mkdir(path.c_str(), mode);
    return (result == -1) ? -errno : result;
}

SyscallReturn
renameFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    string old_name;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(old_name, p->getSyscallArg(tc, index)))
        return -EFAULT;

    string new_name;

    if (!tc->getMemProxy().tryReadString(new_name, p->getSyscallArg(tc, index)))
        return -EFAULT;

    // Adjust path for current working directory
    old_name = p->fullPath(old_name);
    new_name = p->fullPath(new_name);

    int64_t result = rename(old_name.c_str(), new_name.c_str());
    return (result == -1) ? -errno : result;
}

SyscallReturn
truncateFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path, p->getSyscallArg(tc, index)))
        return -EFAULT;

    off_t length = p->getSyscallArg(tc, index);

    // Adjust path for current working directory
    path = p->fullPath(path);

    int result = truncate(path.c_str(), length);
    return (result == -1) ? -errno : result;
}

SyscallReturn
ftruncateFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    off_t length = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    int result = ftruncate(sim_fd, length);
    return (result == -1) ? -errno : result;
}

SyscallReturn
truncate64Func(SyscallDesc *desc, int num,
               Process *process, ThreadContext *tc)
{
    int index = 0;
    string path;

    if (!tc->getMemProxy().tryReadString(path, process->getSyscallArg(tc, index)))
       return -EFAULT;

    int64_t length = process->getSyscallArg(tc, index, 64);

    // Adjust path for current working directory
    path = process->fullPath(path);

#if NO_STAT64
    int result = truncate(path.c_str(), length);
#else
    int result = truncate64(path.c_str(), length);
#endif
    return (result == -1) ? -errno : result;
}

SyscallReturn
ftruncate64Func(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    int64_t length = p->getSyscallArg(tc, index, 64);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

#if NO_STAT64
    int result = ftruncate(sim_fd, length);
#else
    int result = ftruncate64(sim_fd, length);
#endif
    return (result == -1) ? -errno : result;
}

SyscallReturn
umaskFunc(SyscallDesc *desc, int num, Process *process, ThreadContext *tc)
{
    // Letting the simulated program change the simulator's umask seems like
    // a bad idea.  Compromise by just returning the current umask but not
    // changing anything.
    mode_t oldMask = umask(0);
    umask(oldMask);
    return (int)oldMask;
}

SyscallReturn
chownFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path, p->getSyscallArg(tc, index)))
        return -EFAULT;

    /* XXX endianess */
    uint32_t owner = p->getSyscallArg(tc, index);
    uid_t hostOwner = owner;
    uint32_t group = p->getSyscallArg(tc, index);
    gid_t hostGroup = group;

    // Adjust path for current working directory
    path = p->fullPath(path);

    int result = chown(path.c_str(), hostOwner, hostGroup);
    return (result == -1) ? -errno : result;
}

SyscallReturn
fchownFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    /* XXX endianess */
    uint32_t owner = p->getSyscallArg(tc, index);
    uid_t hostOwner = owner;
    uint32_t group = p->getSyscallArg(tc, index);
    gid_t hostGroup = group;

    int result = fchown(sim_fd, hostOwner, hostGroup);
    return (result == -1) ? -errno : result;
}


/**
 * TODO: there's a bit more involved here since file descriptors created with
 * dup are supposed to share a file description. So, there is a problem with
 * maintaining fields like file offset or flags since an update to such a
 * field won't be reflected in the metadata for the fd entries that we
 * maintain to hold metadata for checkpoint restoration.
 */
SyscallReturn
dupFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);

    auto old_hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
    if (!old_hbfdp)
        return -EBADF;
    int sim_fd = old_hbfdp->getSimFD();

    int result = dup(sim_fd);
    int local_errno = errno;

    std::shared_ptr<FDEntry> new_fdep = old_hbfdp->clone();
    auto new_hbfdp = std::dynamic_pointer_cast<HBFDEntry>(new_fdep);
    new_hbfdp->setSimFD(result);

    return (result == -1) ? -local_errno : p->fds->allocFD(new_fdep);
}

SyscallReturn
fcntlFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int arg;
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    int cmd = p->getSyscallArg(tc, index);

    auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
    if (!hbfdp)
        return -EBADF;
    int sim_fd = hbfdp->getSimFD();

    int coe = hbfdp->getCOE();

    switch (cmd) {
      case F_GETFD:
        return coe & FD_CLOEXEC;

      case F_SETFD: {
        arg = p->getSyscallArg(tc, index);
        arg ? hbfdp->setCOE(true) : hbfdp->setCOE(false);
        return 0;
      }

      // Rely on the host to maintain the file status flags for this file
      // description rather than maintain it ourselves. Admittedly, this
      // is suboptimal (and possibly error prone), but it is difficult to
      // maintain the flags by tracking them across the different descriptors
      // (that refer to this file description) caused by clone, dup, and
      // subsequent fcntls.
      case F_GETFL:
      case F_SETFL: {
        arg = p->getSyscallArg(tc, index);
        int rv = fcntl(sim_fd, cmd, arg);
        return (rv == -1) ? -errno : rv;
      }

      default:
        warn("fcntl: unsupported command %d\n", cmd);
        return 0;
    }
}

SyscallReturn
fcntl64Func(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);

    auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
    if (!hbfdp)
        return -EBADF;
    int sim_fd = hbfdp->getSimFD();

    int cmd = p->getSyscallArg(tc, index);
    switch (cmd) {
      case 33: //F_GETLK64
        warn("fcntl64(%d, F_GETLK64) not supported, error returned\n", tgt_fd);
        return -EMFILE;

      case 34: // F_SETLK64
      case 35: // F_SETLKW64
        warn("fcntl64(%d, F_SETLK(W)64) not supported, error returned\n",
             tgt_fd);
        return -EMFILE;

      default:
        // not sure if this is totally valid, but we'll pass it through
        // to the underlying OS
        warn("fcntl64(%d, %d) passed through to host\n", tgt_fd, cmd);
        return fcntl(sim_fd, cmd);
        // return 0;
    }
}

SyscallReturn
pipePseudoFunc(SyscallDesc *desc, int callnum, Process *process,
               ThreadContext *tc)
{
    int sim_fds[2], tgt_fds[2];

    int pipe_retval = pipe(sim_fds);
    if (pipe_retval < 0)
        return pipe_retval;

    auto rend = PipeFDEntry::EndType::read;
    auto rpfd = std::make_shared<PipeFDEntry>(sim_fds[0], O_WRONLY, rend);

    auto wend = PipeFDEntry::EndType::write;
    auto wpfd = std::make_shared<PipeFDEntry>(sim_fds[1], O_RDONLY, wend);

    tgt_fds[0] = process->fds->allocFD(rpfd);
    tgt_fds[1] = process->fds->allocFD(wpfd);

    /**
     * Now patch the read object to record the target file descriptor chosen
     * as the write end of the pipe.
     */
    rpfd->setPipeReadSource(tgt_fds[1]);

    /**
     * Alpha Linux convention for pipe() is that fd[0] is returned as
     * the return value of the function, and fd[1] is returned in r20.
     */
    tc->setIntReg(SyscallPseudoReturnReg, tgt_fds[1]);
    return sim_fds[0];
}

SyscallReturn
setpgidFunc(SyscallDesc *desc, int callnum, Process *process,
            ThreadContext *tc)
{
    int index = 0;
    int pid = process->getSyscallArg(tc, index);
    int pgid = process->getSyscallArg(tc, index);

    if (pgid < 0)
        return -EINVAL;

    if (pid == 0) {
        process->setpgid(process->pid());
        return 0;
    }

    Process *matched_ph = NULL;
    System *sysh = tc->getSystemPtr();

    // Retrieves process pointer from active/suspended thread contexts.
    for (int i = 0; i < sysh->numContexts(); i++) {
        if (sysh->threadContexts[i]->status() != ThreadContext::Halted) {
            Process *temp_h = sysh->threadContexts[i]->getProcessPtr();
            Process *walk_ph = (Process*)temp_h;

            if (walk_ph && walk_ph->pid() == process->pid())
                matched_ph = walk_ph;
        }
    }

    assert(matched_ph != NULL);
    matched_ph->setpgid((pgid == 0) ? matched_ph->pid() : pgid);

    return 0;
}

SyscallReturn
getpidPseudoFunc(SyscallDesc *desc, int callnum, Process *process,
                 ThreadContext *tc)
{
    // Make up a PID.  There's no interprocess communication in
    // fake_syscall mode, so there's no way for a process to know it's
    // not getting a unique value.

    tc->setIntReg(SyscallPseudoReturnReg, process->ppid());
    return process->pid();
}


SyscallReturn
getuidPseudoFunc(SyscallDesc *desc, int callnum, Process *process,
                 ThreadContext *tc)
{
    // Make up a UID and EUID... it shouldn't matter, and we want the
    // simulation to be deterministic.

    // EUID goes in r20.
    tc->setIntReg(SyscallPseudoReturnReg, process->euid()); //EUID
    return process->uid();              // UID
}


SyscallReturn
getgidPseudoFunc(SyscallDesc *desc, int callnum, Process *process,
                 ThreadContext *tc)
{
    // Get current group ID.  EGID goes in r20.
    tc->setIntReg(SyscallPseudoReturnReg, process->egid()); //EGID
    return process->gid();
}


SyscallReturn
setuidFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    // can't fathom why a benchmark would call this.
    int index = 0;
    warn("Ignoring call to setuid(%d)\n", process->getSyscallArg(tc, index));
    return 0;
}

SyscallReturn
getpidFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    return process->tgid();
}

SyscallReturn
gettidFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    return process->pid();
}

SyscallReturn
getppidFunc(SyscallDesc *desc, int callnum, Process *process,
            ThreadContext *tc)
{
    return process->ppid();
}

SyscallReturn
getuidFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    return process->uid();              // UID
}

SyscallReturn
geteuidFunc(SyscallDesc *desc, int callnum, Process *process,
            ThreadContext *tc)
{
    return process->euid();             // UID
}

SyscallReturn
getgidFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    return process->gid();
}

SyscallReturn
getegidFunc(SyscallDesc *desc, int callnum, Process *process,
            ThreadContext *tc)
{
    return process->egid();
}

SyscallReturn
fallocateFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
#if NO_FALLOCATE
    warn("Host OS cannot support calls to fallocate. Ignoring syscall");
#else
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    int mode = p->getSyscallArg(tc, index);
    off_t offset = p->getSyscallArg(tc, index);
    off_t len = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    int result = fallocate(sim_fd, mode, offset, len);
    if (result < 0)
        return -errno;
#endif
    return 0;
}

SyscallReturn
accessFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc,
           int index)
{
    string path;
    if (!tc->getMemProxy().tryReadString(path, p->getSyscallArg(tc, index)))
        return -EFAULT;

    // Adjust path for current working directory
    path = p->fullPath(path);

    mode_t mode = p->getSyscallArg(tc, index);

    int result = access(path.c_str(), mode);
    return (result == -1) ? -errno : result;
}

SyscallReturn
accessFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    return accessFunc(desc, callnum, p, tc, 0);
}

