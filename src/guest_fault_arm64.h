/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BB_GUEST_FAULT_ARM64_H
#define BB_GUEST_FAULT_ARM64_H
#include "guest_cpu.h"
/* Pinned FEX f2b679f, ArchHelpers/Arm64Emitter.cpp x64::SRA (Linux ABI).
 * CPUState contains the last syscall synchronization, not the live JIT GPRs. */
static inline void guest_fault_arm64_gprs(GuestContext *out,const uint64_t host[31],uint64_t rip) {
    static const unsigned char sra[16]={4,7,5,6,8,9,10,11,12,13,14,15,16,17,19,29};
    for(unsigned i=0;i<16;++i)out->gpr[i]=host[sra[i]];
    out->rip=rip;
}
#endif
