#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include <thd.h>
#include "isr.h"
#include "tss.h"

typedef enum {
	CR4_TSD    = 1 << 2,  /* time stamp (rdtsc) access at user-level disabled */
	CR4_PSE    = 1 << 4,  /* page size extensions (superpages) */
	CR4_PGE    = 1 << 7,  /* page global bit enabled */
	CR4_PCE    = 1 << 8,  /* user-level access to performance counters enabled (rdpmc) */
	CR4_OSFXSR = 1 << 9,  /* floating point enabled */
	CR4_SMEP   = 1 << 20, /* Supervisor Mode Execution Protection Enable */
	CR4_SMAP   = 1 << 21  /* Supervisor Mode Access Protection Enable */
} cr4_flags_t;

enum
{
	CR0_PG    = 1 << 31, /* enable paging */
	CR0_FPEMU = 1 << 2,  /* disable floating point, enable emulation */
	CR0_PRMOD = 1 << 0   /* in protected-mode (vs real-mode) */
};

static inline u64_t
chal_cpu_cr4_get(void)
{
	u64_t config;
	asm("movq %%cr4, %0" : "=r"(config));
	return config;
}

static inline void
chal_cpu_cr4_set(cr4_flags_t flags)
{
	u64_t config = chal_cpu_cr4_get();
	config |= (u64_t)flags;
	asm("movq %0, %%cr4" : : "r"(config));
}

static inline void
chal_cpu_eflags_init(void)
{
	u64_t val;

	asm volatile("pushfq ; popq %0" : "=r"(val));
	val |= 3 << 12; /* iopl */
	asm volatile("pushq %0 ; popfq" : : "r"(val));
}

static void
chal_cpu_pgtbl_activate(pgtbl_t pgtbl)
{
	asm volatile("movq %0, %%cr3" : : "r"(pgtbl));
}

#define IA32_SYSENTER_CS  0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176
#define MSR_PLATFORM_INFO 0x000000ce
#define MSR_APIC_BASE     0x1b
#define MSR_TSC_AUX       0xc0000103

extern void sysenter_entry(void);

static inline void
writemsr(u32_t reg, u32_t low, u32_t high)
{
	__asm__("wrmsr" : : "c"(reg), "a"(low), "d"(high));
}

static inline void
readmsr(u32_t reg, u32_t *low, u32_t *high)
{
	__asm__("rdmsr" : "=a"(*low), "=d"(*high) : "c"(reg));
}

static inline void
chal_cpuid(int code, u32_t *a, u32_t *b, u32_t *c, u32_t *d)
{
	asm volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(code));
}

static void
chal_cpu_init(void)
{
	u64_t cr4 = chal_cpu_cr4_get();
	cpuid_t cpu_id = get_cpuid();

	chal_cpu_cr4_set(cr4 | CR4_PSE | CR4_PGE);
	writemsr(IA32_SYSENTER_CS, SEL_KCSEG, 0);
	/* X86_64-FIXME: tss[cpu_id].esp -> rsp */
	writemsr(IA32_SYSENTER_ESP, (u32_t)tss[cpu_id].rsp0, (u32_t)(tss[cpu_id].rsp0 >> 32));
	writemsr(IA32_SYSENTER_EIP, (u32_t)sysenter_entry, (u32_t)((u64_t)sysenter_entry >> 32));
	writemsr(0xC0000080,0x501, 0 );
	//writemsr(0xc0000081,0x1000, 0x00100008 );
	//writemsr(0xC0000082,0x00000000,0xffff8000);

	chal_cpu_eflags_init();
}

static inline vaddr_t
chal_cpu_fault_vaddr(struct pt_regs *r)
{
	vaddr_t fault_addr;
	asm volatile("movq %%cr2, %0" : "=r"(fault_addr));
	return fault_addr;
}

/* FIXME: I doubt these flags are really the same as the PGTBL_* macros */
static inline u64_t
chal_cpu_fault_errcode(struct pt_regs *r)
{
	return r->orig_ax;
}

static inline u64_t
chal_cpu_fault_ip(struct pt_regs *r)
{
	return r->ip;
}

static inline void
chal_user_upcall(void *ip, u16_t tid, u16_t cpuid)
{
	__asm__("movq $0, %r11;movq $0x1000, %rcx;mov $0x00, %rax; mov %rax, %ds;sysretq" );
}

void chal_timer_thd_init(struct thread *t);

#endif /* CHAL_CPU_H */