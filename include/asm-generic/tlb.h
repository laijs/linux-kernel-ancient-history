/* asm-generic/tlb.h
 *
 *	Generic TLB shootdown code
 *
 * Copyright 2001 Red Hat, Inc.
 * Based on code from mm/memory.c Copyright Linus Torvalds and others.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_GENERIC__TLB_H
#define _ASM_GENERIC__TLB_H

#include <linux/config.h>
#include <asm/tlbflush.h>

/* aim for something that fits in the L1 cache */
#define FREE_PTE_NR	508

/* mmu_gather_t is an opaque type used by the mm code for passing around any
 * data needed by arch specific code for tlb_remove_page.  This structure can
 * be per-CPU or per-MM as the page table lock is held for the duration of TLB
 * shootdown.
 */
typedef struct free_pte_ctx {
	struct mm_struct	*mm;
	unsigned long		nr;	/* set to ~0UL means fast mode */
	unsigned long		freed;
	struct page *		pages[FREE_PTE_NR];
} mmu_gather_t;

/* Users of the generic TLB shootdown code must declare this storage space. */
extern mmu_gather_t	mmu_gathers[NR_CPUS];

/* Do me later */
#define tlb_start_vma(tlb, vma) do { } while (0)
#define tlb_end_vma(tlb, vma) do { } while (0)

/* tlb_gather_mmu
 *	Return a pointer to an initialized mmu_gather_t.
 */
static inline mmu_gather_t *tlb_gather_mmu(struct mm_struct *mm)
{
	mmu_gather_t *tlb = &mmu_gathers[smp_processor_id()];
	unsigned long nr;

	tlb->mm = mm;
	tlb->freed = 0;

	/* Use fast mode if this MM only exists on this CPU */
	nr = ~0UL;
#ifdef CONFIG_SMP
	if (mm->cpu_vm_mask != (1<<smp_processor_id()))
		nr = 0UL;
#endif
	tlb->nr = nr;
	return tlb;
}

static inline void tlb_flush_mmu(mmu_gather_t *tlb, unsigned long start, unsigned long end)
{
	unsigned long nr;

	flush_tlb_mm(tlb->mm);
	nr = tlb->nr;
	if (nr != ~0UL) {
		unsigned long i;
		tlb->nr = 0;
		for (i=0; i < nr; i++)
			free_page_and_swap_cache(tlb->pages[i]);
	}
}

/* tlb_finish_mmu
 *	Called at the end of the shootdown operation to free up any resources
 *	that were required.  The page table lock is still held at this point.
 */
static inline void tlb_finish_mmu(mmu_gather_t *tlb, unsigned long start, unsigned long end)
{
	int freed = tlb->freed;
	struct mm_struct *mm = tlb->mm;
	int rss = mm->rss;

	if (rss < freed)
		freed = rss;
	mm->rss = rss - freed;
	tlb_flush_mmu(tlb, start, end);
}


/* void tlb_remove_page(mmu_gather_t *tlb, pte_t *ptep, unsigned long addr)
 *	Must perform the equivalent to __free_pte(pte_get_and_clear(ptep)), while
 *	handling the additional races in SMP caused by other CPUs caching valid
 *	mappings in their TLBs.
 */
static inline void tlb_remove_page(mmu_gather_t *tlb, struct page *page)
{
	/* Handle the common case fast, first. */\
	if (tlb->nr == ~0UL) {
		free_page_and_swap_cache(page);
		return;
	}
	tlb->pages[tlb->nr++] = page;
	if (tlb->nr >= FREE_PTE_NR)
		tlb_flush_mmu(tlb, 0, 0);
}

#endif /* _ASM_GENERIC__TLB_H */

