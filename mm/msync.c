/*
 *	linux/mm/msync.c
 *
 * Copyright (C) 1994-1999  Linus Torvalds
 */

/*
 * The msync() system call.
 */
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/mman.h>

#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>

/*
 * Called with mm->page_table_lock held to protect against other
 * threads/the swapper from ripping pte's out from under us.
 */
static int filemap_sync_pte(pte_t *ptep, struct vm_area_struct *vma,
	unsigned long address, unsigned int flags)
{
	pte_t pte = *ptep;

	if (pte_present(pte) && pte_dirty(pte)) {
		struct page *page = pte_page(pte);
		if (VALID_PAGE(page) && !PageReserved(page) && ptep_test_and_clear_dirty(ptep)) {
			flush_tlb_page(vma, address);
			set_page_dirty(page);
		}
	}
	return 0;
}

static inline int filemap_sync_pte_range(pmd_t * pmd,
	unsigned long address, unsigned long end, 
	struct vm_area_struct *vma, unsigned int flags)
{
	pte_t *pte;
	int error;

	if (pmd_none(*pmd))
		return 0;
	if (pmd_bad(*pmd)) {
		pmd_ERROR(*pmd);
		pmd_clear(pmd);
		return 0;
	}
	pte = pte_offset_map(pmd, address);
	if ((address & PMD_MASK) != (end & PMD_MASK))
		end = (address & PMD_MASK) + PMD_SIZE;
	error = 0;
	do {
		error |= filemap_sync_pte(pte, vma, address, flags);
		address += PAGE_SIZE;
		pte++;
	} while (address && (address < end));

	pte_unmap(pte - 1);

	return error;
}

static inline int filemap_sync_pmd_range(pgd_t * pgd,
	unsigned long address, unsigned long end, 
	struct vm_area_struct *vma, unsigned int flags)
{
	pmd_t * pmd;
	int error;

	if (pgd_none(*pgd))
		return 0;
	if (pgd_bad(*pgd)) {
		pgd_ERROR(*pgd);
		pgd_clear(pgd);
		return 0;
	}
	pmd = pmd_offset(pgd, address);
	if ((address & PGDIR_MASK) != (end & PGDIR_MASK))
		end = (address & PGDIR_MASK) + PGDIR_SIZE;
	error = 0;
	do {
		error |= filemap_sync_pte_range(pmd, address, end, vma, flags);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
	return error;
}

int filemap_sync(struct vm_area_struct * vma, unsigned long address,
	size_t size, unsigned int flags)
{
	pgd_t * dir;
	unsigned long end = address + size;
	int error = 0;

	/* Aquire the lock early; it may be possible to avoid dropping
	 * and reaquiring it repeatedly.
	 */
	spin_lock(&vma->vm_mm->page_table_lock);

	dir = pgd_offset(vma->vm_mm, address);
	flush_cache_range(vma, address, end);
	if (address >= end)
		BUG();
	do {
		error |= filemap_sync_pmd_range(dir, address, end, vma, flags);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (address && (address < end));
	flush_tlb_range(vma, end - size, end);

	spin_unlock(&vma->vm_mm->page_table_lock);

	return error;
}

/*
 * MS_SYNC syncs the entire file - including mappings.
 *
 * MS_ASYNC initiates writeout of just the dirty mapped data.
 * This provides no guarantee of file integrity - things like indirect
 * blocks may not have started writeout.  MS_ASYNC is primarily useful
 * where the application knows that it has finished with the data and
 * wishes to intelligently schedule its own I/O traffic.
 */
static int msync_interval(struct vm_area_struct * vma,
	unsigned long start, unsigned long end, int flags)
{
	int ret = 0;
	struct file * file = vma->vm_file;

	if (file && (vma->vm_flags & VM_SHARED)) {
		ret = filemap_sync(vma, start, end-start, flags);

		if (!ret && (flags & (MS_SYNC|MS_ASYNC))) {
			struct inode * inode = file->f_dentry->d_inode;

			down(&inode->i_sem);
			ret = filemap_fdatasync(inode->i_mapping);
			if (flags & MS_SYNC) {
				int err;

				if (file->f_op && file->f_op->fsync) {
					err = file->f_op->fsync(file, file->f_dentry, 1);
					if (err && !ret)
						ret = err;
				}
				err = filemap_fdatawait(inode->i_mapping);
				if (err && !ret)
					ret = err;
			}
			up(&inode->i_sem);
		}
	}
	return ret;
}

asmlinkage long sys_msync(unsigned long start, size_t len, int flags)
{
	unsigned long end;
	struct vm_area_struct * vma;
	int unmapped_error, error = -EINVAL;

	down_read(&current->mm->mmap_sem);
	if (start & ~PAGE_MASK)
		goto out;
	len = (len + ~PAGE_MASK) & PAGE_MASK;
	end = start + len;
	if (end < start)
		goto out;
	if (flags & ~(MS_ASYNC | MS_INVALIDATE | MS_SYNC))
		goto out;
	error = 0;
	if (end == start)
		goto out;
	/*
	 * If the interval [start,end) covers some unmapped address ranges,
	 * just ignore them, but return -EFAULT at the end.
	 */
	vma = find_vma(current->mm, start);
	unmapped_error = 0;
	for (;;) {
		/* Still start < end. */
		error = -EFAULT;
		if (!vma)
			goto out;
		/* Here start < vma->vm_end. */
		if (start < vma->vm_start) {
			unmapped_error = -EFAULT;
			start = vma->vm_start;
		}
		/* Here vma->vm_start <= start < vma->vm_end. */
		if (end <= vma->vm_end) {
			if (start < end) {
				error = msync_interval(vma, start, end, flags);
				if (error)
					goto out;
			}
			error = unmapped_error;
			goto out;
		}
		/* Here vma->vm_start <= start < vma->vm_end < end. */
		error = msync_interval(vma, start, vma->vm_end, flags);
		if (error)
			goto out;
		start = vma->vm_end;
		vma = vma->vm_next;
	}
out:
	up_read(&current->mm->mmap_sem);
	return error;
}

