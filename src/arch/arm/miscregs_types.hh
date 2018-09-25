/*
 * Copyright (c) 2010-2018 ARM Limited
 * All rights reserved
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
 * Copyright (c) 2009 The Regents of The University of Michigan
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
 *          Giacomo Gabrielli
 */

#ifndef __ARCH_ARM_MISCREGS_TYPES_HH__
#define __ARCH_ARM_MISCREGS_TYPES_HH__

#include "base/bitunion.hh"

namespace ArmISA
{
    BitUnion32(CPSR)
        Bitfield<31, 30> nz;
        Bitfield<29> c;
        Bitfield<28> v;
        Bitfield<27> q;
        Bitfield<26, 25> it1;
        Bitfield<24> j;
        Bitfield<23, 22> res0_23_22;
        Bitfield<21> ss;        // AArch64
        Bitfield<20> il;        // AArch64
        Bitfield<19, 16> ge;
        Bitfield<15, 10> it2;
        Bitfield<9> d;          // AArch64
        Bitfield<9> e;
        Bitfield<8> a;
        Bitfield<7> i;
        Bitfield<6> f;
        Bitfield<8, 6> aif;
        Bitfield<9, 6> daif;    // AArch64
        Bitfield<5> t;
        Bitfield<4> width;      // AArch64
        Bitfield<3, 2> el;      // AArch64
        Bitfield<4, 0> mode;
        Bitfield<0> sp;         // AArch64
    EndBitUnion(CPSR)

    BitUnion32(HDCR)
        Bitfield<11>   tdra;
        Bitfield<10>   tdosa;
        Bitfield<9>    tda;
        Bitfield<8>    tde;
        Bitfield<7>    hpme;
        Bitfield<6>    tpm;
        Bitfield<5>    tpmcr;
        Bitfield<4, 0> hpmn;
    EndBitUnion(HDCR)

    BitUnion32(HCPTR)
        Bitfield<31> tcpac;
        Bitfield<20> tta;
        Bitfield<15> tase;
        Bitfield<13> tcp13;
        Bitfield<12> tcp12;
        Bitfield<11> tcp11;
        Bitfield<10> tcp10;
        Bitfield<10> tfp;  // AArch64
        Bitfield<9>  tcp9;
        Bitfield<8>  tcp8;
        Bitfield<7>  tcp7;
        Bitfield<6>  tcp6;
        Bitfield<5>  tcp5;
        Bitfield<4>  tcp4;
        Bitfield<3>  tcp3;
        Bitfield<2>  tcp2;
        Bitfield<1>  tcp1;
        Bitfield<0>  tcp0;
    EndBitUnion(HCPTR)

    BitUnion32(HSTR)
        Bitfield<17> tjdbx;
        Bitfield<16> ttee;
        Bitfield<15> t15;
        Bitfield<13> t13;
        Bitfield<12> t12;
        Bitfield<11> t11;
        Bitfield<10> t10;
        Bitfield<9>  t9;
        Bitfield<8>  t8;
        Bitfield<7>  t7;
        Bitfield<6>  t6;
        Bitfield<5>  t5;
        Bitfield<4>  t4;
        Bitfield<3>  t3;
        Bitfield<2>  t2;
        Bitfield<1>  t1;
        Bitfield<0>  t0;
    EndBitUnion(HSTR)

    BitUnion64(HCR)
        Bitfield<34>     e2h;   // AArch64
        Bitfield<33>     id;    // AArch64
        Bitfield<32>     cd;    // AArch64
        Bitfield<31>     rw;    // AArch64
        Bitfield<30>     trvm;  // AArch64
        Bitfield<29>     hcd;   // AArch64
        Bitfield<28>     tdz;   // AArch64

        Bitfield<27>     tge;
        Bitfield<26>     tvm;
        Bitfield<25>     ttlb;
        Bitfield<24>     tpu;
        Bitfield<23>     tpc;
        Bitfield<22>     tsw;
        Bitfield<21>     tac;
        Bitfield<21>     tacr;  // AArch64
        Bitfield<20>     tidcp;
        Bitfield<19>     tsc;
        Bitfield<18>     tid3;
        Bitfield<17>     tid2;
        Bitfield<16>     tid1;
        Bitfield<15>     tid0;
        Bitfield<14>     twe;
        Bitfield<13>     twi;
        Bitfield<12>     dc;
        Bitfield<11, 10> bsu;
        Bitfield<9>      fb;
        Bitfield<8>      va;
        Bitfield<8>      vse;   // AArch64
        Bitfield<7>      vi;
        Bitfield<6>      vf;
        Bitfield<5>      amo;
        Bitfield<4>      imo;
        Bitfield<3>      fmo;
        Bitfield<2>      ptw;
        Bitfield<1>      swio;
        Bitfield<0>      vm;
    EndBitUnion(HCR)

    BitUnion32(NSACR)
        Bitfield<20> nstrcdis;
        Bitfield<19> rfr;
        Bitfield<15> nsasedis;
        Bitfield<14> nsd32dis;
        Bitfield<13> cp13;
        Bitfield<12> cp12;
        Bitfield<11> cp11;
        Bitfield<10> cp10;
        Bitfield<9>  cp9;
        Bitfield<8>  cp8;
        Bitfield<7>  cp7;
        Bitfield<6>  cp6;
        Bitfield<5>  cp5;
        Bitfield<4>  cp4;
        Bitfield<3>  cp3;
        Bitfield<2>  cp2;
        Bitfield<1>  cp1;
        Bitfield<0>  cp0;
    EndBitUnion(NSACR)

    BitUnion32(SCR)
        Bitfield<13> twe;
        Bitfield<12> twi;
        Bitfield<11> st;  // AArch64
        Bitfield<10> rw;  // AArch64
        Bitfield<9> sif;
        Bitfield<8> hce;
        Bitfield<7> scd;
        Bitfield<7> smd;  // AArch64
        Bitfield<6> nEt;
        Bitfield<5> aw;
        Bitfield<4> fw;
        Bitfield<3> ea;
        Bitfield<2> fiq;
        Bitfield<1> irq;
        Bitfield<0> ns;
    EndBitUnion(SCR)

    BitUnion32(SCTLR)
        Bitfield<30>   te;      // Thumb Exception Enable (AArch32 only)
        Bitfield<29>   afe;     // Access flag enable (AArch32 only)
        Bitfield<28>   tre;     // TEX remap enable (AArch32 only)
        Bitfield<27>   nmfi;    // Non-maskable FIQ support (ARMv7 only)
        Bitfield<26>   uci;     // Enable EL0 access to DC CVAU, DC CIVAC,
                                // DC CVAC and IC IVAU instructions
                                // (AArch64 SCTLR_EL1 only)
        Bitfield<25>   ee;      // Exception Endianness
        Bitfield<24>   ve;      // Interrupt Vectors Enable (ARMv7 only)
        Bitfield<24>   e0e;     // Endianness of explicit data accesses at EL0
                                // (AArch64 SCTLR_EL1 only)
        Bitfield<23>   xp;      // Extended page table enable (dropped in ARMv7)
        Bitfield<22>   u;       // Alignment (dropped in ARMv7)
        Bitfield<21>   fi;      // Fast interrupts configuration enable
                                // (ARMv7 only)
        Bitfield<20>   uwxn;    // Unprivileged write permission implies EL1 XN
                                // (AArch32 only)
        Bitfield<19>   dz;      // Divide by Zero fault enable
                                // (dropped in ARMv7)
        Bitfield<19>   wxn;     // Write permission implies XN
        Bitfield<18>   ntwe;    // Not trap WFE
                                // (ARMv8 AArch32 and AArch64 SCTLR_EL1 only)
        Bitfield<18>   rao2;    // Read as one
        Bitfield<16>   ntwi;    // Not trap WFI
                                // (ARMv8 AArch32 and AArch64 SCTLR_EL1 only)
        Bitfield<16>   rao3;    // Read as one
        Bitfield<15>   uct;     // Enable EL0 access to CTR_EL0
                                // (AArch64 SCTLR_EL1 only)
        Bitfield<14>   rr;      // Round Robin select (ARMv7 only)
        Bitfield<14>   dze;     // Enable EL0 access to DC ZVA
                                // (AArch64 SCTLR_EL1 only)
        Bitfield<13>   v;       // Vectors bit (AArch32 only)
        Bitfield<12>   i;       // Instruction cache enable
        Bitfield<11>   z;       // Branch prediction enable (ARMv7 only)
        Bitfield<10>   sw;      // SWP/SWPB enable (ARMv7 only)
        Bitfield<9, 8> rs;      // Deprecated protection bits (dropped in ARMv7)
        Bitfield<9>    uma;     // User mask access (AArch64 SCTLR_EL1 only)
        Bitfield<8>    sed;     // SETEND disable
                                // (ARMv8 AArch32 and AArch64 SCTLR_EL1 only)
        Bitfield<7>    b;       // Endianness support (dropped in ARMv7)
        Bitfield<7>    itd;     // IT disable
                                // (ARMv8 AArch32 and AArch64 SCTLR_EL1 only)
        Bitfield<6, 3> rao4;    // Read as one
        Bitfield<6>    thee;    // ThumbEE enable
                                // (ARMv8 AArch32 and AArch64 SCTLR_EL1 only)
        Bitfield<5>    cp15ben; // CP15 barrier enable
                                // (AArch32 and AArch64 SCTLR_EL1 only)
        Bitfield<4>    sa0;     // Stack Alignment Check Enable for EL0
                                // (AArch64 SCTLR_EL1 only)
        Bitfield<3>    sa;      // Stack Alignment Check Enable (AArch64 only)
        Bitfield<2>    c;       // Cache enable
        Bitfield<1>    a;       // Alignment check enable
        Bitfield<0>    m;       // MMU enable
    EndBitUnion(SCTLR)

    BitUnion32(CPACR)
        Bitfield<1, 0> cp0;
        Bitfield<3, 2> cp1;
        Bitfield<5, 4> cp2;
        Bitfield<7, 6> cp3;
        Bitfield<9, 8> cp4;
        Bitfield<11, 10> cp5;
        Bitfield<13, 12> cp6;
        Bitfield<15, 14> cp7;
        Bitfield<17, 16> cp8;
        Bitfield<19, 18> cp9;
        Bitfield<21, 20> cp10;
        Bitfield<21, 20> fpen;  // AArch64
        Bitfield<23, 22> cp11;
        Bitfield<25, 24> cp12;
        Bitfield<27, 26> cp13;
        Bitfield<29, 28> rsvd;
        Bitfield<28> tta;  // AArch64
        Bitfield<30> d32dis;
        Bitfield<31> asedis;
    EndBitUnion(CPACR)

    BitUnion32(FSR)
        Bitfield<3, 0> fsLow;
        Bitfield<5, 0> status;  // LPAE
        Bitfield<7, 4> domain;
        Bitfield<9> lpae;
        Bitfield<10> fsHigh;
        Bitfield<11> wnr;
        Bitfield<12> ext;
        Bitfield<13> cm;  // LPAE
    EndBitUnion(FSR)

    BitUnion32(FPSCR)
        Bitfield<0> ioc;
        Bitfield<1> dzc;
        Bitfield<2> ofc;
        Bitfield<3> ufc;
        Bitfield<4> ixc;
        Bitfield<7> idc;
        Bitfield<8> ioe;
        Bitfield<9> dze;
        Bitfield<10> ofe;
        Bitfield<11> ufe;
        Bitfield<12> ixe;
        Bitfield<15> ide;
        Bitfield<18, 16> len;
        Bitfield<21, 20> stride;
        Bitfield<23, 22> rMode;
        Bitfield<24> fz;
        Bitfield<25> dn;
        Bitfield<26> ahp;
        Bitfield<27> qc;
        Bitfield<28> v;
        Bitfield<29> c;
        Bitfield<30> z;
        Bitfield<31> n;
    EndBitUnion(FPSCR)

    BitUnion32(FPEXC)
        Bitfield<31> ex;
        Bitfield<30> en;
        Bitfield<29, 0> subArchDefined;
    EndBitUnion(FPEXC)

    BitUnion32(MVFR0)
        Bitfield<3, 0> advSimdRegisters;
        Bitfield<7, 4> singlePrecision;
        Bitfield<11, 8> doublePrecision;
        Bitfield<15, 12> vfpExceptionTrapping;
        Bitfield<19, 16> divide;
        Bitfield<23, 20> squareRoot;
        Bitfield<27, 24> shortVectors;
        Bitfield<31, 28> roundingModes;
    EndBitUnion(MVFR0)

    BitUnion32(MVFR1)
        Bitfield<3, 0> flushToZero;
        Bitfield<7, 4> defaultNaN;
        Bitfield<11, 8> advSimdLoadStore;
        Bitfield<15, 12> advSimdInteger;
        Bitfield<19, 16> advSimdSinglePrecision;
        Bitfield<23, 20> advSimdHalfPrecision;
        Bitfield<27, 24> vfpHalfPrecision;
        Bitfield<31, 28> raz;
    EndBitUnion(MVFR1)

    BitUnion64(TTBCR)
        // Short-descriptor translation table format
        Bitfield<2, 0> n;
        Bitfield<4> pd0;
        Bitfield<5> pd1;
        // Long-descriptor translation table format
        Bitfield<2, 0> t0sz;
        Bitfield<7> epd0;
        Bitfield<9, 8> irgn0;
        Bitfield<11, 10> orgn0;
        Bitfield<13, 12> sh0;
        Bitfield<14> tg0;
        Bitfield<18, 16> t1sz;
        Bitfield<22> a1;
        Bitfield<23> epd1;
        Bitfield<25, 24> irgn1;
        Bitfield<27, 26> orgn1;
        Bitfield<29, 28> sh1;
        Bitfield<30> tg1;
        Bitfield<34, 32> ips;
        Bitfield<36> as;
        Bitfield<37> tbi0;
        Bitfield<38> tbi1;
        // Common
        Bitfield<31> eae;
        // TCR_EL2/3 (AArch64)
        Bitfield<18, 16> ps;
        Bitfield<20> tbi;
    EndBitUnion(TTBCR)

    // Fields of TCR_EL{1,2,3} (mostly overlapping)
    // TCR_EL1 is natively 64 bits, the others are 32 bits
    BitUnion64(TCR)
        Bitfield<5, 0> t0sz;
        Bitfield<7> epd0; // EL1
        Bitfield<9, 8> irgn0;
        Bitfield<11, 10> orgn0;
        Bitfield<13, 12> sh0;
        Bitfield<15, 14> tg0;
        Bitfield<18, 16> ps;
        Bitfield<20> tbi; // EL2/EL3
        Bitfield<21, 16> t1sz; // EL1
        Bitfield<22> a1; // EL1
        Bitfield<23> epd1; // EL1
        Bitfield<25, 24> irgn1; // EL1
        Bitfield<27, 26> orgn1; // EL1
        Bitfield<29, 28> sh1; // EL1
        Bitfield<31, 30> tg1; // EL1
        Bitfield<34, 32> ips; // EL1
        Bitfield<36> as; // EL1
        Bitfield<37> tbi0; // EL1
        Bitfield<38> tbi1; // EL1
    EndBitUnion(TCR)

    BitUnion32(HTCR)
        Bitfield<2, 0> t0sz;
        Bitfield<9, 8> irgn0;
        Bitfield<11, 10> orgn0;
        Bitfield<13, 12> sh0;
    EndBitUnion(HTCR)

    BitUnion32(VTCR_t)
        Bitfield<3, 0> t0sz;
        Bitfield<4> s;
        Bitfield<5, 0> t0sz64;
        Bitfield<7, 6> sl0;
        Bitfield<9, 8> irgn0;
        Bitfield<11, 10> orgn0;
        Bitfield<13, 12> sh0;
        Bitfield<15, 14> tg0;
        Bitfield<18, 16> ps; // Only defined for VTCR_EL2
    EndBitUnion(VTCR_t)

    BitUnion32(PRRR)
       Bitfield<1,0> tr0;
       Bitfield<3,2> tr1;
       Bitfield<5,4> tr2;
       Bitfield<7,6> tr3;
       Bitfield<9,8> tr4;
       Bitfield<11,10> tr5;
       Bitfield<13,12> tr6;
       Bitfield<15,14> tr7;
       Bitfield<16> ds0;
       Bitfield<17> ds1;
       Bitfield<18> ns0;
       Bitfield<19> ns1;
       Bitfield<24> nos0;
       Bitfield<25> nos1;
       Bitfield<26> nos2;
       Bitfield<27> nos3;
       Bitfield<28> nos4;
       Bitfield<29> nos5;
       Bitfield<30> nos6;
       Bitfield<31> nos7;
   EndBitUnion(PRRR)

   BitUnion32(NMRR)
       Bitfield<1,0> ir0;
       Bitfield<3,2> ir1;
       Bitfield<5,4> ir2;
       Bitfield<7,6> ir3;
       Bitfield<9,8> ir4;
       Bitfield<11,10> ir5;
       Bitfield<13,12> ir6;
       Bitfield<15,14> ir7;
       Bitfield<17,16> or0;
       Bitfield<19,18> or1;
       Bitfield<21,20> or2;
       Bitfield<23,22> or3;
       Bitfield<25,24> or4;
       Bitfield<27,26> or5;
       Bitfield<29,28> or6;
       Bitfield<31,30> or7;
   EndBitUnion(NMRR)

   BitUnion32(CONTEXTIDR)
      Bitfield<7,0>  asid;
      Bitfield<31,8> procid;
   EndBitUnion(CONTEXTIDR)

   BitUnion32(L2CTLR)
      Bitfield<2,0>   sataRAMLatency;
      Bitfield<4,3>   reserved_4_3;
      Bitfield<5>     dataRAMSetup;
      Bitfield<8,6>   tagRAMLatency;
      Bitfield<9>     tagRAMSetup;
      Bitfield<11,10> dataRAMSlice;
      Bitfield<12>    tagRAMSlice;
      Bitfield<20,13> reserved_20_13;
      Bitfield<21>    eccandParityEnable;
      Bitfield<22>    reserved_22;
      Bitfield<23>    interptCtrlPresent;
      Bitfield<25,24> numCPUs;
      Bitfield<30,26> reserved_30_26;
      Bitfield<31>    l2rstDISABLE_monitor;
   EndBitUnion(L2CTLR)

   BitUnion32(CTR)
      Bitfield<3,0>   iCacheLineSize;
      Bitfield<13,4>  raz_13_4;
      Bitfield<15,14> l1IndexPolicy;
      Bitfield<19,16> dCacheLineSize;
      Bitfield<23,20> erg;
      Bitfield<27,24> cwg;
      Bitfield<28>    raz_28;
      Bitfield<31,29> format;
   EndBitUnion(CTR)

   BitUnion32(PMSELR)
      Bitfield<4, 0> sel;
   EndBitUnion(PMSELR)

    BitUnion64(PAR)
        // 64-bit format
        Bitfield<63, 56> attr;
        Bitfield<39, 12> pa;
        Bitfield<11>     lpae;
        Bitfield<9>      ns;
        Bitfield<8, 7>   sh;
        Bitfield<0>      f;
   EndBitUnion(PAR)

   BitUnion32(ESR)
        Bitfield<31, 26> ec;
        Bitfield<25> il;
        Bitfield<15, 0> imm16;
   EndBitUnion(ESR)

   BitUnion32(CPTR)
        Bitfield<31> tcpac;
        Bitfield<20> tta;
        Bitfield<13, 12> res1_13_12_el2;
        Bitfield<10> tfp;
        Bitfield<9, 0> res1_9_0_el2;
   EndBitUnion(CPTR)

}

#endif // __ARCH_ARM_MISCREGS_TYPES_HH__
