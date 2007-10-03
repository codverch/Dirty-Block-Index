# Copyright (c) 2007 The Hewlett-Packard Development Company
# All rights reserved.
#
# Redistribution and use of this software in source and binary forms,
# with or without modification, are permitted provided that the
# following conditions are met:
#
# The software must be used only for Non-Commercial Use which means any
# use which is NOT directed to receiving any direct monetary
# compensation for, or commercial advantage from such use.  Illustrative
# examples of non-commercial use are academic research, personal study,
# teaching, education and corporate research & development.
# Illustrative examples of commercial use are distributing products for
# commercial advantage and providing services using the software for
# commercial advantage.
#
# If you wish to use this software or functionality therein that may be
# covered by patents for commercial use, please contact:
#     Director of Intellectual Property Licensing
#     Office of Strategy and Technology
#     Hewlett-Packard Company
#     1501 Page Mill Road
#     Palo Alto, California  94304
#
# Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.  Redistributions
# in binary form must reproduce the above copyright notice, this list of
# conditions and the following disclaimer in the documentation and/or
# other materials provided with the distribution.  Neither the name of
# the COPYRIGHT HOLDER(s), HEWLETT-PACKARD COMPANY, nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.  No right of
# sublicense is granted herewith.  Derivatives of the software and
# output created using the software may be prepared, but only for
# Non-Commercial Uses.  Derivatives of the software may be shared with
# others provided: (i) the others agree to abide by the list of
# conditions herein which includes the Non-Commercial Use restrictions;
# and (ii) such Derivatives of the software include the above copyright
# notice to acknowledge the contribution from this software where
# applicable, this list of conditions and the disclaimer below.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Gabe Black

microcode = '''
def macroop POP_R {
    # Make the default data size of pops 64 bits in 64 bit mode
    .adjust_env oszIn64Override

    ld reg, ss, [1, t0, rsp]
    addi rsp, rsp, dsz
};

def macroop POP_M {
    # Make the default data size of pops 64 bits in 64 bit mode
    .adjust_env oszIn64Override

    ld t1, ss, [1, t0, rsp]
    # Check stack address
    addi rsp, rsp, dsz
    st t1, seg, sib, disp
};

def macroop POP_P {
    # Make the default data size of pops 64 bits in 64 bit mode
    .adjust_env oszIn64Override

    rdip t7
    ld t1, ss, [1, t0, rsp]
    # Check stack address
    addi rsp, rsp, dsz
    st t1, seg, riprel, disp
};

def macroop PUSH_R {
    # Make the default data size of pops 64 bits in 64 bit mode
    .adjust_env oszIn64Override

    # This needs to work slightly differently from the other versions of push
    # because the -original- version of the stack pointer is what gets pushed
    st reg, ss, [1, t0, rsp], "-env.dataSize"
    subi rsp, rsp, dsz
};

def macroop PUSH_I {
    # Make the default data size of pops 64 bits in 64 bit mode
    .adjust_env oszIn64Override

    limm t1, imm
    st t1, ss, [1, t0, rsp], "-env.dataSize"
    subi rsp, rsp, dsz
};

def macroop PUSH_M {
    # Make the default data size of pops 64 bits in 64 bit mode
    .adjust_env oszIn64Override

    ld t1, seg, sib, disp
    st t1, ss, [1, t0, rsp], "-env.dataSize"
    subi rsp, rsp, dsz
};

def macroop PUSH_P {
    # Make the default data size of pops 64 bits in 64 bit mode
    .adjust_env oszIn64Override

    rdip t7
    ld t1, seg, riprel, disp
    # Check stack address
    subi rsp, rsp, dsz
    st t1, ss, [1, t0, rsp]
};

def macroop PUSHA {
    # Check all the stack addresses.
    st rax, ss, [1, t0, rsp], "-0 * env.dataSize"
    st rcx, ss, [1, t0, rsp], "-1 * env.dataSize"
    st rdx, ss, [1, t0, rsp], "-2 * env.dataSize"
    st rbx, ss, [1, t0, rsp], "-3 * env.dataSize"
    st rsp, ss, [1, t0, rsp], "-4 * env.dataSize"
    st rbp, ss, [1, t0, rsp], "-5 * env.dataSize"
    st rsi, ss, [1, t0, rsp], "-6 * env.dataSize"
    st rdi, ss, [1, t0, rsp], "-7 * env.dataSize"
    subi rsp, rsp, "8 * env.dataSize"
};

def macroop POPA {
    # Check all the stack addresses.
    ld rdi, ss, [1, t0, rsp], "0 * env.dataSize"
    ld rsi, ss, [1, t0, rsp], "1 * env.dataSize"
    ld rbp, ss, [1, t0, rsp], "2 * env.dataSize"
    ld rbx, ss, [1, t0, rsp], "4 * env.dataSize"
    ld rdx, ss, [1, t0, rsp], "5 * env.dataSize"
    ld rcx, ss, [1, t0, rsp], "6 * env.dataSize"
    ld rax, ss, [1, t0, rsp], "7 * env.dataSize"
    addi rsp, rsp, "8 * env.dataSize"
};

def macroop LEAVE {
    # Make the default data size of pops 64 bits in 64 bit mode
    .adjust_env oszIn64Override

    mov t1, t1, rbp
    ld rbp, ss, [1, t0, t1]
    mov rsp, rsp, t1
    addi rsp, rsp, dsz
};
'''
#let {{
#    class ENTER(Inst):
#	"GenFault ${new UnimpInstFault}"
#}};
