#ifndef _ASM_IA64_SYSTEM_H
#define _ASM_IA64_SYSTEM_H

/*
 * System defines. Note that this is included both from .c and .S
 * files, so it does only defines, not any C code.  This is based
 * on information published in the Processor Abstraction Layer
 * and the System Abstraction Layer manual.
 *
 * Copyright (C) 1998-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 1999 Asit Mallick <asit.k.mallick@intel.com>
 * Copyright (C) 1999 Don Dugger <don.dugger@intel.com>
 */
#include <linux/config.h>

#include <asm/kregs.h>
#include <asm/page.h>
#include <asm/pal.h>
#include <asm/percpu.h>

#define KERNEL_START		(PAGE_OFFSET + 68*1024*1024)

/* 0xa000000000000000 - 0xa000000000000000+PERCPU_MAX_SIZE remain unmapped */
#define PERCPU_ADDR		(0xa000000000000000 + PERCPU_PAGE_SIZE)
#define GATE_ADDR		(0xa000000000000000 + 2*PERCPU_PAGE_SIZE)

#ifndef __ASSEMBLY__

#include <linux/kernel.h>
#include <linux/types.h>

struct pci_vector_struct {
	__u16 bus;	/* PCI Bus number */
	__u32 pci_id;	/* ACPI split 16 bits device, 16 bits function (see section 6.1.1) */
	__u8 pin;	/* PCI PIN (0 = A, 1 = B, 2 = C, 3 = D) */
	__u32 irq;	/* IRQ assigned */
};

extern struct ia64_boot_param {
	__u64 command_line;		/* physical address of command line arguments */
	__u64 efi_systab;		/* physical address of EFI system table */
	__u64 efi_memmap;		/* physical address of EFI memory map */
	__u64 efi_memmap_size;		/* size of EFI memory map */
	__u64 efi_memdesc_size;		/* size of an EFI memory map descriptor */
	__u32 efi_memdesc_version;	/* memory descriptor version */
	struct {
		__u16 num_cols;	/* number of columns on console output device */
		__u16 num_rows;	/* number of rows on console output device */
		__u16 orig_x;	/* cursor's x position */
		__u16 orig_y;	/* cursor's y position */
	} console_info;
	__u64 fpswa;		/* physical address of the fpswa interface */
	__u64 initrd_start;
	__u64 initrd_size;
} *ia64_boot_param;

static inline void
ia64_insn_group_barrier (void)
{
	__asm__ __volatile__ (";;" ::: "memory");
}

/*
 * Macros to force memory ordering.  In these descriptions, "previous"
 * and "subsequent" refer to program order; "visible" means that all
 * architecturally visible effects of a memory access have occurred
 * (at a minimum, this means the memory has been read or written).
 *
 *   wmb():	Guarantees that all preceding stores to memory-
 *		like regions are visible before any subsequent
 *		stores and that all following stores will be
 *		visible only after all previous stores.
 *   rmb():	Like wmb(), but for reads.
 *   mb():	wmb()/rmb() combo, i.e., all previous memory
 *		accesses are visible before all subsequent
 *		accesses and vice versa.  This is also known as
 *		a "fence."
 *
 * Note: "mb()" and its variants cannot be used as a fence to order
 * accesses to memory mapped I/O registers.  For that, mf.a needs to
 * be used.  However, we don't want to always use mf.a because (a)
 * it's (presumably) much slower than mf and (b) mf.a is supported for
 * sequential memory pages only.
 */
#define mb()	__asm__ __volatile__ ("mf" ::: "memory")
#define rmb()	mb()
#define wmb()	mb()
#define read_barrier_depends()	do { } while(0)

#ifdef CONFIG_SMP
# define smp_mb()	mb()
# define smp_rmb()	rmb()
# define smp_wmb()	wmb()
# define smp_read_barrier_depends()	read_barrier_depends()
#else
# define smp_mb()	barrier()
# define smp_rmb()	barrier()
# define smp_wmb()	barrier()
# define smp_read_barrier_depends()	do { } while(0)
#endif

/*
 * XXX check on these---I suspect what Linus really wants here is
 * acquire vs release semantics but we can't discuss this stuff with
 * Linus just yet.  Grrr...
 */
#define set_mb(var, value)	do { (var) = (value); mb(); } while (0)
#define set_wmb(var, value)	do { (var) = (value); mb(); } while (0)

#define safe_halt()         ia64_pal_halt(1)                /* PAL_HALT */

/*
 * The group barrier in front of the rsm & ssm are necessary to ensure
 * that none of the previous instructions in the same group are
 * affected by the rsm/ssm.
 */
/* For spinlocks etc */

/* clearing psr.i is implicitly serialized (visible by next insn) */
/* setting psr.i requires data serialization */
#define __local_irq_save(x)	__asm__ __volatile__ ("mov %0=psr;;"			\
						      "rsm psr.i;;"			\
						      : "=r" (x) :: "memory")
#define __local_irq_disable()	__asm__ __volatile__ (";; rsm psr.i;;" ::: "memory")
#define __local_irq_restore(x)	__asm__ __volatile__ ("cmp.ne p6,p7=%0,r0;;"		\
						      "(p6) ssm psr.i;"			\
						      "(p7) rsm psr.i;;"		\
						      "(p6) srlz.d"			\
						      :: "r" ((x) & IA64_PSR_I)		\
						      : "p6", "p7", "memory")

#ifdef CONFIG_IA64_DEBUG_IRQ

  extern unsigned long last_cli_ip;

# define __save_ip()		__asm__ ("mov %0=ip" : "=r" (last_cli_ip))

# define local_irq_save(x)					\
do {								\
	unsigned long psr;					\
								\
	__local_irq_save(psr);					\
	if (psr & IA64_PSR_I)					\
		__save_ip();					\
	(x) = psr;						\
} while (0)

# define local_irq_disable()	do { unsigned long x; local_irq_save(x); } while (0)

# define local_irq_restore(x)					\
do {								\
	unsigned long old_psr, psr = (x);			\
								\
	local_save_flags(old_psr);				\
	__local_irq_restore(psr);				\
	if ((old_psr & IA64_PSR_I) && !(psr & IA64_PSR_I))	\
		__save_ip();					\
} while (0)

#else /* !CONFIG_IA64_DEBUG_IRQ */
# define local_irq_save(x)	__local_irq_save(x)
# define local_irq_disable()	__local_irq_disable()
# define local_irq_restore(x)	__local_irq_restore(x)
#endif /* !CONFIG_IA64_DEBUG_IRQ */

#define local_irq_enable()	__asm__ __volatile__ (";; ssm psr.i;; srlz.d" ::: "memory")
#define local_save_flags(flags)	__asm__ __volatile__ ("mov %0=psr" : "=r" (flags) :: "memory")

#define irqs_disabled()				\
({						\
	unsigned long flags;			\
	local_save_flags(flags);		\
	(flags & IA64_PSR_I) == 0;		\
})

#ifdef __KERNEL__

#define prepare_to_switch()    do { } while(0)

#ifdef CONFIG_IA32_SUPPORT
# define IS_IA32_PROCESS(regs)	(ia64_psr(regs)->is != 0)
#else
# define IS_IA32_PROCESS(regs)		0
struct task_struct;
static inline void ia32_save_state(struct task_struct *t __attribute__((unused))){}
static inline void ia32_load_state(struct task_struct *t __attribute__((unused))){}
#endif

/*
 * Context switch from one thread to another.  If the two threads have
 * different address spaces, schedule() has already taken care of
 * switching to the new address space by calling switch_mm().
 *
 * Disabling access to the fph partition and the debug-register
 * context switch MUST be done before calling ia64_switch_to() since a
 * newly created thread returns directly to
 * ia64_ret_from_syscall_clear_r8.
 */
extern struct task_struct *ia64_switch_to (void *next_task);

struct task_struct;

extern void ia64_save_extra (struct task_struct *task);
extern void ia64_load_extra (struct task_struct *task);

#ifdef CONFIG_PERFMON
  DECLARE_PER_CPU(unsigned long, pfm_syst_info);
# define PERFMON_IS_SYSWIDE() (get_cpu_var(pfm_syst_info) & 0x1)
#else
# define PERFMON_IS_SYSWIDE() (0)
#endif

#define __switch_to(prev,next,last) do {						\
	if (((prev)->thread.flags & (IA64_THREAD_DBG_VALID|IA64_THREAD_PM_VALID))	\
	    || IS_IA32_PROCESS(ia64_task_regs(prev)) || PERFMON_IS_SYSWIDE())		\
		ia64_save_extra(prev);							\
	if (((next)->thread.flags & (IA64_THREAD_DBG_VALID|IA64_THREAD_PM_VALID))	\
	    || IS_IA32_PROCESS(ia64_task_regs(next)) || PERFMON_IS_SYSWIDE())		\
		ia64_load_extra(next);							\
	(last) = ia64_switch_to((next));						\
} while (0)

#ifdef CONFIG_SMP

/*
 * In the SMP case, we save the fph state when context-switching
 * away from a thread that modified fph.  This way, when the thread
 * gets scheduled on another CPU, the CPU can pick up the state from
 * task->thread.fph, avoiding the complication of having to fetch
 * the latest fph state from another CPU.
 */
# define switch_to(prev,next,last) do {					\
	if (ia64_psr(ia64_task_regs(prev))->mfh) {			\
		ia64_psr(ia64_task_regs(prev))->mfh = 0;		\
		(prev)->thread.flags |= IA64_THREAD_FPH_VALID;		\
		__ia64_save_fpu((prev)->thread.fph);			\
		(prev)->thread.last_fph_cpu = smp_processor_id();	\
	}								\
	if ((next)->thread.flags & IA64_THREAD_FPH_VALID) {		\
		if (((next)->thread.last_fph_cpu == smp_processor_id())	\
		    && (ia64_get_fpu_owner() == next))			\
		{							\
			ia64_psr(ia64_task_regs(next))->dfh = 0;	\
			ia64_psr(ia64_task_regs(next))->mfh = 0;	\
		} else							\
			ia64_psr(ia64_task_regs(next))->dfh = 1;	\
	}								\
	__switch_to(prev,next,last);					\
  } while (0)
#else
# define switch_to(prev,next,last) do {						\
	ia64_psr(ia64_task_regs(next))->dfh = (ia64_get_fpu_owner() != (next));	\
	__switch_to(prev,next,last);						\
} while (0)
#endif

/*
 * On IA-64, we don't want to hold the runqueue's lock during the low-level context-switch,
 * because that could cause a deadlock.  Here is an example by Erich Focht:
 *
 * Example:
 * CPU#0:
 * schedule()
 *    -> spin_lock_irq(&rq->lock)
 *    -> context_switch()
 *       -> wrap_mmu_context()
 *          -> read_lock(&tasklist_lock)
 *
 * CPU#1:
 * sys_wait4() or release_task() or forget_original_parent()
 *    -> write_lock(&tasklist_lock)
 *    -> do_notify_parent()
 *       -> wake_up_parent()
 *          -> try_to_wake_up()
 *             -> spin_lock_irq(&parent_rq->lock)
 *
 * If the parent's rq happens to be on CPU#0, we'll wait for the rq->lock
 * of that CPU which will not be released, because there we wait for the
 * tasklist_lock to become available.
 */
#define prepare_arch_switch(rq, next)		\
do {						\
	spin_lock(&(next)->switch_lock);	\
	spin_unlock(&(rq)->lock);		\
} while (0)
#define finish_arch_switch(rq, prev)	spin_unlock_irq(&(prev)->switch_lock)
#define task_running(rq, p) 		((rq)->curr == (p) || spin_is_locked(&(p)->switch_lock))

#endif /* __KERNEL__ */

#endif /* __ASSEMBLY__ */

#endif /* _ASM_IA64_SYSTEM_H */
