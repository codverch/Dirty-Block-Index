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

#
# Byte version of one operand unsigned multiply.
#

def macroop MUL_B_R
{
    mul1u rax, reg
    mulel rax
    # Really ah
    muleh rsi, flags=(OF,CF)
};

def macroop MUL_B_M
{
    ld t1, seg, sib, disp
    mul1u rax, t1
    mulel rax
    # Really ah
    muleh rsi, flags=(OF,CF)
};

def macroop MUL_B_P
{
    rdip t7
    ld t1, seg, riprel, disp
    mul1u rax, t1
    mulel rax
    # Really ah
    muleh rsi, flags=(OF,CF)
};

#
# One operand unsigned multiply.
#

def macroop MUL_R
{
    mul1u rax, reg
    mulel rax
    muleh rdx, flags=(OF,CF)
};

def macroop MUL_M
{
    ld t1, seg, sib, disp
    mul1u rax, t1
    mulel rax
    muleh rdx, flags=(OF,CF)
};

def macroop MUL_P
{
    rdip t7
    ld t1, seg, riprel, disp
    mul1u rax, t1
    mulel rax
    muleh rdx, flags=(OF,CF)
};

#
# Byte version of one operand signed multiply.
#

def macroop IMUL_B_R
{
    mul1s rax, reg
    mulel rax
    # Really ah
    muleh rsi, flags=(OF,CF)
};

def macroop IMUL_B_M
{
    ld t1, seg, sib, disp
    mul1s rax, t1
    mulel rax
    # Really ah
    muleh rsi, flags=(OF,CF)
};

def macroop IMUL_B_P
{
    rdip t7
    ld t1, seg, riprel, disp
    mul1s rax, t1
    mulel rax
    # Really ah
    muleh rsi, flags=(OF,CF)
};

#
# One operand signed multiply.
#

def macroop IMUL_R
{
    mul1s rax, reg
    mulel rax
    muleh rdx, flags=(OF,CF)
};

def macroop IMUL_M
{
    ld t1, seg, sib, disp
    mul1s rax, t1
    mulel rax
    muleh rdx, flags=(OF,CF)
};

def macroop IMUL_P
{
    rdip t7
    ld t1, seg, riprel, disp
    mul1s rax, t1
    mulel rax
    muleh rdx, flags=(OF,CF)
};

def macroop IMUL_R_R
{
    mul1s reg, regm
    mulel reg
    muleh t0, flags=(CF,OF)
};

def macroop IMUL_R_M
{
    ld t1, seg, sib, disp
    mul1s reg, t1
    mulel reg
    muleh t0, flags=(CF,OF)
};

def macroop IMUL_R_P
{
    rdip t7
    ld t1, seg, riprel, disp
    mul1s reg, t1
    mulel reg
    muleh t0, flags=(CF,OF)
};

#
# Three operand signed multiply.
#

def macroop IMUL_R_R_I
{
    limm t1, imm
    mul1s regm, t1
    mulel reg
    muleh t0, flags=(OF,CF)
};

def macroop IMUL_R_M_I
{
    limm t1, imm
    ld t2, seg, sib, disp
    mul1s t2, t1
    mulel reg
    muleh t0, flags=(OF,CF)
};

def macroop IMUL_R_P_I
{
    rdip t7
    limm t1, imm
    ld t2, seg, riprel
    mul1s t2, t1
    mulel reg
    muleh t0, flags=(OF,CF)
};

#
# One byte version of unsigned division
#

def macroop DIV_B_R
{
    div1 rax, rax, reg
};

def macroop DIV_B_M
{
    ld t1, seg, sib, disp
    div1 rax, rax, t1
};

def macroop DIV_B_P
{
    rdip t7
    ld t1, seg, riprel, disp
    div1 rax, rax, t1
};

#
# Unsigned division
#

def macroop DIV_R
{
    divr t1, rax, reg
    divq rax, rax, reg
    mov rdx, rdx, t1
};

def macroop DIV_M
{
    ld t1, seg, sib, disp
    divr rdx, rax, t1
    divq rax, rax, t1
};

def macroop DIV_P
{
    rdip t7
    ld t1, seg, riprel, disp
    divr rdx, rax, t1
    divq rax, rax, t1
};

#
# Signed division
#

def macroop IDIV_R
{
    divr t1, rax, reg
    divq rax, rax, reg
    mov rdx, rdx, t1
};

def macroop IDIV_M
{
    ld t1, seg, sib, disp
    divr rdx, rax, t1
    divq rax, rax, t1
};

def macroop IDIV_P
{
    rdip t7
    ld t1, seg, riprel, disp
    divr rdx, rax, t1
    divq rax, rax, t1
};
'''
#let {{
#    class IDIV(Inst):
#	"GenFault ${new UnimpInstFault}"
#}};
