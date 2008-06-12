# Copyright (c) 2008 The Regents of The University of Michigan
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

# Copyright (c) 2007-2008 The Hewlett-Packard Development Company
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
def macroop BSR_R_R {
    # Determine if the input was zero, and also move it to a temp reg.
    movi t1, t1, t0, dataSize=8
    and t1, regm, regm, flags=(ZF,)
    bri t0, label("end"), flags=(CZF,)

    # Zero out the result register
    movi reg, reg, 0x0

    # Bit 6
    srli t3, t1, 32, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x20
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 5
    srli t3, t1, 16, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x10
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 4
    srli t3, t1, 8, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x8
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 3
    srli t3, t1, 4, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x4
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 2
    srli t3, t1, 2, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x2
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 1
    srli t3, t1, 1, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x1
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

end:
    fault "NoFault"
};

def macroop BSR_R_M {

    movi t1, t1, t0, dataSize=8
    ld t1, seg, sib, disp

    # Determine if the input was zero, and also move it to a temp reg.
    and t1, t1, t1, flags=(ZF,)
    bri t0, label("end"), flags=(CZF,)

    # Zero out the result register
    movi reg, reg, 0x0

    # Bit 6
    srli t3, t1, 32, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x20
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 5
    srli t3, t1, 16, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x10
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 4
    srli t3, t1, 8, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x8
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 3
    srli t3, t1, 4, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x4
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 2
    srli t3, t1, 2, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x2
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 1
    srli t3, t1, 1, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x1
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

end:
    fault "NoFault"
};

def macroop BSR_R_P {

    rdip t7
    movi t1, t1, t0, dataSize=8
    ld t1, seg, riprel, disp

    # Determine if the input was zero, and also move it to a temp reg.
    and t1, t1, t1, flags=(ZF,)
    bri t0, label("end"), flags=(CZF,)

    # Zero out the result register
    movi reg, reg, 0x0

    # Bit 6
    srli t3, t1, 32, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x20
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 5
    srli t3, t1, 16, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x10
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 4
    srli t3, t1, 8, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x8
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 3
    srli t3, t1, 4, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x4
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 2
    srli t3, t1, 2, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x2
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 1
    srli t3, t1, 1, dataSize=8, flags=(EZF,)
    ori t4, reg, 0x1
    mov reg, reg, t4, flags=(nCEZF,)
    mov t1, t1, t3, flags=(nCEZF,)

end:
    fault "NoFault"
};

def macroop BSF_R_R {
    # Determine if the input was zero, and also move it to a temp reg.
    mov t1, t1, t0, dataSize=8
    and t1, regm, regm, flags=(ZF,)
    bri t0, label("end"), flags=(CZF,)

    # Zero out the result register
    movi reg, reg, 0

    subi t2, t1, 1
    xor t1, t2, t1

    # Bit 6
    srli t3, t1, 32, dataSize=8
    andi t4, t3, 32, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 5
    srli t3, t1, 16, dataSize=8
    andi t4, t3, 16, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 4
    srli t3, t1, 8, dataSize=8
    andi t4, t3, 8, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 3
    srli t3, t1, 4, dataSize=8
    andi t4, t3, 4, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 2
    srli t3, t1, 2, dataSize=8
    andi t4, t3, 2, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 1
    srli t3, t1, 1, dataSize=8
    andi t4, t3, 1, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

end:
    fault "NoFault"
};

def macroop BSF_R_M {

    mov t1, t1, t0, dataSize=8
    ld t1, seg, sib, disp

    # Determine if the input was zero, and also move it to a temp reg.
    and t1, t1, t1, flags=(ZF,)
    bri t0, label("end"), flags=(CZF,)

    # Zero out the result register
    mov reg, reg, t0

    subi t2, t1, 1
    xor t1, t2, t1

    # Bit 6
    srli t3, t1, 32, dataSize=8
    andi t4, t3, 32, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 5
    srli t3, t1, 16, dataSize=8
    andi t4, t3, 16, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 4
    srli t3, t1, 8, dataSize=8
    andi t4, t3, 8, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 3
    srli t3, t1, 4, dataSize=8
    andi t4, t3, 4, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 2
    srli t3, t1, 2, dataSize=8
    andi t4, t3, 2, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 1
    srli t3, t1, 1, dataSize=8
    andi t4, t3, 1, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

end:
    fault "NoFault"
};

def macroop BSF_R_P {

    rdip t7
    mov t1, t1, t0, dataSize=8
    ld t1, seg, riprel, disp

    # Determine if the input was zero, and also move it to a temp reg.
    and t1, t1, t1, flags=(ZF,)
    bri t0, label("end"), flags=(CZF,)

    # Zero out the result register
    mov reg, reg, t0

    subi t2, t1, 1
    xor t1, t2, t1

    # Bit 6
    srli t3, t1, 32, dataSize=8
    andi t4, t3, 32, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 5
    srli t3, t1, 16, dataSize=8
    andi t4, t3, 16, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 4
    srli t3, t1, 8, dataSize=8
    andi t4, t3, 8, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 3
    srli t3, t1, 4, dataSize=8
    andi t4, t3, 4, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 2
    srli t3, t1, 2, dataSize=8
    andi t4, t3, 2, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

    # Bit 1
    srli t3, t1, 1, dataSize=8
    andi t4, t3, 1, flags=(EZF,)
    or reg, reg, t4
    mov t1, t1, t3, flags=(nCEZF,)

end:
    fault "NoFault"
};
'''
