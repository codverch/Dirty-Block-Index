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

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <iostream>

#include "sim/syscall_emul.hh"
#include "base/chunk_generator.hh"
#include "base/trace.hh"
#include "cpu/thread_context.hh"
#include "cpu/base.hh"
#include "mem/page_table.hh"
#include "sim/process.hh"

#include "sim/sim_exit.hh"

using namespace std;
using namespace TheISA;

void
SyscallDesc::doSyscall(int callnum, LiveProcess *process, ThreadContext *tc)
{
    DPRINTFR(SyscallVerbose, "%d: %s: syscall %s called w/arguments %d,%d,%d,%d\n",
             curTick,tc->getCpuPtr()->name(), name,
             tc->getSyscallArg(0),tc->getSyscallArg(1),
             tc->getSyscallArg(2),tc->getSyscallArg(3));

    SyscallReturn retval = (*funcPtr)(this, callnum, process, tc);

    DPRINTFR(SyscallVerbose, "%d: %s: syscall %s returns %d\n",
             curTick,tc->getCpuPtr()->name(), name, retval.value());

    if (!(flags & SyscallDesc::SuppressReturnValue))
        tc->setSyscallReturn(retval);
}


SyscallReturn
unimplementedFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
                  ThreadContext *tc)
{
    fatal("syscall %s (#%d) unimplemented.", desc->name, callnum);

    return 1;
}


SyscallReturn
ignoreFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    warn("ignoring syscall %s(%d, %d, ...)", desc->name,
         tc->getSyscallArg(0), tc->getSyscallArg(1));

    return 0;
}


SyscallReturn
exitFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
         ThreadContext *tc)
{
    if (tc->exit()) {
        exitSimLoop("target called exit()", tc->getSyscallArg(0) & 0xff);
    }

    return 1;
}


SyscallReturn
getpagesizeFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    return (int)VMPageSize;
}


SyscallReturn
obreakFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    Addr junk;

    // change brk addr to first arg
    Addr new_brk = tc->getSyscallArg(0);
    if (new_brk != 0) {
        for (ChunkGenerator gen(p->brk_point, new_brk - p->brk_point,
                                VMPageSize); !gen.done(); gen.next()) {
            if (!p->pTable->translate(gen.addr(), junk))
                p->pTable->allocate(roundDown(gen.addr(), VMPageSize),
                                    VMPageSize);
        }
        p->brk_point = new_brk;
    }
    DPRINTF(SyscallVerbose, "Break Point changed to: %#X\n", p->brk_point);
    return p->brk_point;
}


SyscallReturn
closeFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    int target_fd = tc->getSyscallArg(0);
    int status = close(p->sim_fd(target_fd));
    if (status >= 0)
        p->free_fd(target_fd);
    return status;
}


SyscallReturn
readFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    int fd = p->sim_fd(tc->getSyscallArg(0));
    int nbytes = tc->getSyscallArg(2);
    BufferArg bufArg(tc->getSyscallArg(1), nbytes);

    int bytes_read = read(fd, bufArg.bufferPtr(), nbytes);

    if (bytes_read != -1)
        bufArg.copyOut(tc->getMemPort());

    return bytes_read;
}

SyscallReturn
writeFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    int fd = p->sim_fd(tc->getSyscallArg(0));
    int nbytes = tc->getSyscallArg(2);
    BufferArg bufArg(tc->getSyscallArg(1), nbytes);

    bufArg.copyIn(tc->getMemPort());

    int bytes_written = write(fd, bufArg.bufferPtr(), nbytes);

    fsync(fd);

    return bytes_written;
}


SyscallReturn
lseekFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    int fd = p->sim_fd(tc->getSyscallArg(0));
    uint64_t offs = tc->getSyscallArg(1);
    int whence = tc->getSyscallArg(2);

    off_t result = lseek(fd, offs, whence);

    return (result == (off_t)-1) ? -errno : result;
}


SyscallReturn
_llseekFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    int fd = p->sim_fd(tc->getSyscallArg(0));
    uint64_t offset_high = tc->getSyscallArg(1);
    uint32_t offset_low = tc->getSyscallArg(2);
    Addr result_ptr = tc->getSyscallArg(3);
    int whence = tc->getSyscallArg(4);

    uint64_t offset = (offset_high << 32) | offset_low;

    uint64_t result = lseek(fd, offset, whence);
    result = TheISA::htog(result);

    if (result == (off_t)-1) {
        //The seek failed.
        return -errno;
    } else {
        //The seek succeeded.
        //Copy "result" to "result_ptr"
        //XXX We'll assume that the size of loff_t is 64 bits on the
        //target platform
        BufferArg result_buf(result_ptr, sizeof(result));
        memcpy(result_buf.bufferPtr(), &result, sizeof(result));
        result_buf.copyOut(tc->getMemPort());
        return 0;
    }


    return (result == (off_t)-1) ? -errno : result;
}


SyscallReturn
munmapFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    // given that we don't really implement mmap, munmap is really easy
    return 0;
}


const char *hostname = "m5.eecs.umich.edu";

SyscallReturn
gethostnameFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    int name_len = tc->getSyscallArg(1);
    BufferArg name(tc->getSyscallArg(0), name_len);

    strncpy((char *)name.bufferPtr(), hostname, name_len);

    name.copyOut(tc->getMemPort());

    return 0;
}

SyscallReturn
getcwdFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    int result = 0;
    unsigned long size = tc->getSyscallArg(1);
    BufferArg buf(tc->getSyscallArg(0), size);

    // Is current working directory defined?
    string cwd = p->getcwd();
    if (!cwd.empty()) {
        if (cwd.length() >= size) {
            // Buffer too small
            return -ERANGE;
        }
        strncpy((char *)buf.bufferPtr(), cwd.c_str(), size);
        result = cwd.length();
    }
    else {
        if (getcwd((char *)buf.bufferPtr(), size) != NULL) {
            result = strlen((char *)buf.bufferPtr());
        }
        else {
            result = -1;
        }
    }

    buf.copyOut(tc->getMemPort());

    return (result == -1) ? -errno : result;
}


SyscallReturn
readlinkFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    string path;

    if (!tc->getMemPort()->tryReadString(path, tc->getSyscallArg(0)))
        return (TheISA::IntReg)-EFAULT;

    // Adjust path for current working directory
    path = p->fullPath(path);

    size_t bufsiz = tc->getSyscallArg(2);
    BufferArg buf(tc->getSyscallArg(1), bufsiz);

    int result = readlink(path.c_str(), (char *)buf.bufferPtr(), bufsiz);

    buf.copyOut(tc->getMemPort());

    return (result == -1) ? -errno : result;
}

SyscallReturn
unlinkFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    string path;

    if (!tc->getMemPort()->tryReadString(path, tc->getSyscallArg(0)))
        return (TheISA::IntReg)-EFAULT;

    // Adjust path for current working directory
    path = p->fullPath(path);

    int result = unlink(path.c_str());
    return (result == -1) ? -errno : result;
}


SyscallReturn
mkdirFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    string path;

    if (!tc->getMemPort()->tryReadString(path, tc->getSyscallArg(0)))
        return (TheISA::IntReg)-EFAULT;

    // Adjust path for current working directory
    path = p->fullPath(path);

    mode_t mode = tc->getSyscallArg(1);

    int result = mkdir(path.c_str(), mode);
    return (result == -1) ? -errno : result;
}

SyscallReturn
renameFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    string old_name;

    if (!tc->getMemPort()->tryReadString(old_name, tc->getSyscallArg(0)))
        return -EFAULT;

    string new_name;

    if (!tc->getMemPort()->tryReadString(new_name, tc->getSyscallArg(1)))
        return -EFAULT;

    // Adjust path for current working directory
    old_name = p->fullPath(old_name);
    new_name = p->fullPath(new_name);

    int64_t result = rename(old_name.c_str(), new_name.c_str());
    return (result == -1) ? -errno : result;
}

SyscallReturn
truncateFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    string path;

    if (!tc->getMemPort()->tryReadString(path, tc->getSyscallArg(0)))
        return -EFAULT;

    off_t length = tc->getSyscallArg(1);

    // Adjust path for current working directory
    path = p->fullPath(path);

    int result = truncate(path.c_str(), length);
    return (result == -1) ? -errno : result;
}

SyscallReturn
ftruncateFunc(SyscallDesc *desc, int num, LiveProcess *process, ThreadContext *tc)
{
    int fd = process->sim_fd(tc->getSyscallArg(0));

    if (fd < 0)
        return -EBADF;

    off_t length = tc->getSyscallArg(1);

    int result = ftruncate(fd, length);
    return (result == -1) ? -errno : result;
}

SyscallReturn
umaskFunc(SyscallDesc *desc, int num, LiveProcess *process, ThreadContext *tc)
{
    // Letting the simulated program change the simulator's umask seems like
    // a bad idea.  Compromise by just returning the current umask but not
    // changing anything.
    mode_t oldMask = umask(0);
    umask(oldMask);
    return oldMask;
}

SyscallReturn
chownFunc(SyscallDesc *desc, int num, LiveProcess *p, ThreadContext *tc)
{
    string path;

    if (!tc->getMemPort()->tryReadString(path, tc->getSyscallArg(0)))
        return -EFAULT;

    /* XXX endianess */
    uint32_t owner = tc->getSyscallArg(1);
    uid_t hostOwner = owner;
    uint32_t group = tc->getSyscallArg(2);
    gid_t hostGroup = group;

    // Adjust path for current working directory
    path = p->fullPath(path);

    int result = chown(path.c_str(), hostOwner, hostGroup);
    return (result == -1) ? -errno : result;
}

SyscallReturn
fchownFunc(SyscallDesc *desc, int num, LiveProcess *process, ThreadContext *tc)
{
    int fd = process->sim_fd(tc->getSyscallArg(0));

    if (fd < 0)
        return -EBADF;

    /* XXX endianess */
    uint32_t owner = tc->getSyscallArg(1);
    uid_t hostOwner = owner;
    uint32_t group = tc->getSyscallArg(2);
    gid_t hostGroup = group;

    int result = fchown(fd, hostOwner, hostGroup);
    return (result == -1) ? -errno : result;
}


SyscallReturn
dupFunc(SyscallDesc *desc, int num, LiveProcess *process, ThreadContext *tc)
{
    int fd = process->sim_fd(tc->getSyscallArg(0));
    if (fd < 0)
        return -EBADF;

    Process::FdMap *fdo = process->sim_fd_obj(tc->getSyscallArg(0));

    int result = dup(fd);
    return (result == -1) ? -errno : process->alloc_fd(result, fdo->filename, fdo->flags, fdo->mode, false);
}


SyscallReturn
fcntlFunc(SyscallDesc *desc, int num, LiveProcess *process,
          ThreadContext *tc)
{
    int fd = tc->getSyscallArg(0);

    if (fd < 0 || process->sim_fd(fd) < 0)
        return -EBADF;

    int cmd = tc->getSyscallArg(1);
    switch (cmd) {
      case 0: // F_DUPFD
        // if we really wanted to support this, we'd need to do it
        // in the target fd space.
        warn("fcntl(%d, F_DUPFD) not supported, error returned\n", fd);
        return -EMFILE;

      case 1: // F_GETFD (get close-on-exec flag)
      case 2: // F_SETFD (set close-on-exec flag)
        return 0;

      case 3: // F_GETFL (get file flags)
      case 4: // F_SETFL (set file flags)
        // not sure if this is totally valid, but we'll pass it through
        // to the underlying OS
        warn("fcntl(%d, %d) passed through to host\n", fd, cmd);
        return fcntl(process->sim_fd(fd), cmd);
        // return 0;

      case 7: // F_GETLK  (get lock)
      case 8: // F_SETLK  (set lock)
      case 9: // F_SETLKW (set lock and wait)
        // don't mess with file locking... just act like it's OK
        warn("File lock call (fcntl(%d, %d)) ignored.\n", fd, cmd);
        return 0;

      default:
        warn("Unknown fcntl command %d\n", cmd);
        return 0;
    }
}

SyscallReturn
fcntl64Func(SyscallDesc *desc, int num, LiveProcess *process,
            ThreadContext *tc)
{
    int fd = tc->getSyscallArg(0);

    if (fd < 0 || process->sim_fd(fd) < 0)
        return -EBADF;

    int cmd = tc->getSyscallArg(1);
    switch (cmd) {
      case 33: //F_GETLK64
        warn("fcntl64(%d, F_GETLK64) not supported, error returned\n", fd);
        return -EMFILE;

      case 34: // F_SETLK64
      case 35: // F_SETLKW64
        warn("fcntl64(%d, F_SETLK(W)64) not supported, error returned\n", fd);
        return -EMFILE;

      default:
        // not sure if this is totally valid, but we'll pass it through
        // to the underlying OS
        warn("fcntl64(%d, %d) passed through to host\n", fd, cmd);
        return fcntl(process->sim_fd(fd), cmd);
        // return 0;
    }
}

SyscallReturn
pipePseudoFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
         ThreadContext *tc)
{
    int fds[2], sim_fds[2];
    int pipe_retval = pipe(fds);

    if (pipe_retval < 0) {
        // error
        return pipe_retval;
    }

    sim_fds[0] = process->alloc_fd(fds[0], "PIPE-READ", O_WRONLY, -1, true);
    sim_fds[1] = process->alloc_fd(fds[1], "PIPE-WRITE", O_RDONLY, -1, true);

    process->setReadPipeSource(sim_fds[0], sim_fds[1]);
    // Alpha Linux convention for pipe() is that fd[0] is returned as
    // the return value of the function, and fd[1] is returned in r20.
    tc->setIntReg(SyscallPseudoReturnReg, sim_fds[1]);
    return sim_fds[0];
}


SyscallReturn
getpidPseudoFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    // Make up a PID.  There's no interprocess communication in
    // fake_syscall mode, so there's no way for a process to know it's
    // not getting a unique value.

    tc->setIntReg(SyscallPseudoReturnReg, process->ppid());
    return process->pid();
}


SyscallReturn
getuidPseudoFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    // Make up a UID and EUID... it shouldn't matter, and we want the
    // simulation to be deterministic.

    // EUID goes in r20.
    tc->setIntReg(SyscallPseudoReturnReg, process->euid()); //EUID
    return process->uid();		// UID
}


SyscallReturn
getgidPseudoFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    // Get current group ID.  EGID goes in r20.
    tc->setIntReg(SyscallPseudoReturnReg, process->egid()); //EGID
    return process->gid();
}


SyscallReturn
setuidFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    // can't fathom why a benchmark would call this.
    warn("Ignoring call to setuid(%d)\n", tc->getSyscallArg(0));
    return 0;
}

SyscallReturn
getpidFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    // Make up a PID.  There's no interprocess communication in
    // fake_syscall mode, so there's no way for a process to know it's
    // not getting a unique value.

    tc->setIntReg(SyscallPseudoReturnReg, process->ppid()); //PID
    return process->pid();
}

SyscallReturn
getppidFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    return process->ppid();
}

SyscallReturn
getuidFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    return process->uid();		// UID
}

SyscallReturn
geteuidFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    return process->euid();		// UID
}

SyscallReturn
getgidFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    return process->gid();
}

SyscallReturn
getegidFunc(SyscallDesc *desc, int callnum, LiveProcess *process,
           ThreadContext *tc)
{
    return process->egid();
}


