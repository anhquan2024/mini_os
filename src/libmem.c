/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  /* TODO: commit the vmaid */
  // rgnode.vmaid

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }
  
  // get_free_vmrg_area FAILED: handle region management (Fig.6)
  // Retrieve the current vm area:
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // Align the increment size to a page boundary:
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);

  // Save old sbrk (the current break):
  int old_sbrk = cur_vma->sbrk;

  // Increase the limit invoking system call with SYSMEM_INC_OP.
  // Here we use the wrapper inc_vma_limit to perform the system call.
  int inc_limit_ret = inc_vma_limit(caller, vmaid, inc_sz);
  if (inc_limit_ret < 0)
  {
      pthread_mutex_unlock(&mmvm_lock);
      return -1;  // Failed to increase the limit.
  }

  // Commit the new limit in the vma structure:
  cur_vma->sbrk = old_sbrk + inc_sz;

  // Commit the allocation address as the old sbrk value:
  *alloc_addr = old_sbrk;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  //struct vm_rg_struct rgnode;

  // Dummy initialization for avoding compiler dummay warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later

  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  // Retrieve the memory region corresponding to rgid.
  struct vm_rg_struct *region = get_symrg_byid(caller->mm, rgid);
  if (region == NULL)
      return -1;

  // Check if the region is valid (allocated).
  if (region->rg_start == -1 || region->rg_end == -1 || region->rg_start >= region->rg_end)
      return -1;

  // Enlist the freed memory region into the free region list.
  if (enlist_vm_freerg_list(caller->mm, region) != 0)
      return -1;

  // Reset the region in the symbol table.
  region->rg_start = -1;
  region->rg_end = -1;
  region->rg_next = NULL;

  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  /* TODO Implement allocation on vm area 0 */
  int addr;
  int ret = __alloc(proc, 0, reg_index, size, &addr);

#ifdef IODUMP
  if(ret == 0)
    printf("Allocated region %d with size %d at address %d\n", reg_index, size, addr);
#endif
  return ret; 
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  /* TODO Implement free region */
  int ret = __free(proc, 0, reg_index);
#ifdef IODUMP
    if(ret == 0)
      printf("Freed region %d\n", reg_index);
#endif
    return ret;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte))
  { /* Page is not online, bring it into MEMRAM */
      int vicpgn, swpfpn;

      /* Find a victim page in MEMRAM to swap out */
      if (find_victim_page(caller->mm, &vicpgn) != 0)
          return -1;

      /* Get a free frame number in swap area */
      if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0)
          return -1;

      /* Swap the victim page from MEMRAM to MEMSWP.
        * This call will exchange the contents of the victim frame (vicpgn)
        * with the free swap frame (swpfpn). */
      if (__mm_swap_page(caller, vicpgn, swpfpn) != 0)
          return -1;

      /* Retrieve the target frame number stored in swap from the page table
        * (assume the swap frame is encoded in the page table entry). */
      int tgtfpn = PAGING_SWP(pte); // Macro: extract swap frame number

      /* Now swap the target page from MEMSWP back into MEMRAM,
        * using the frame freed by the victim swap (vicpgn). */
      if (__mm_swap_page(caller, tgtfpn, vicpgn) != 0)
          return -1;

    /* Update the page table entry to mark the page as present in MEMRAM.
    * Clear the old frame bits and set the new frame (vicpgn) along with the present flag. */
    mm->pgd[pgn] = (mm->pgd[pgn] & ~PAGING_PTE_FPN_MASK) | vicpgn | PAGING_PTE_PRESENT_MASK;
}

*fpn = PAGING_FPN(mm->pgd[pgn]);
return 0;

}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* Calculate physical address using the frame number and offset */
  int phyaddr = fpn * PAGING_PAGESZ + off;

  /* Read from physical memory.
   * This simulates a SYSCALL 17 sys_memmap with SYSMEM_IO_READ.
   * MEMPHY_read is expected to return 0 on success.
   */
  if (MEMPHY_read(caller->mram, phyaddr, data) != 0)
      return -1;

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);  // assumes macro extracts offset within page
  int fpn;

  /* Bring the page into MEMRAM, swapping from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
      return -1;  /* invalid page access */

  /* Calculate physical address using the frame number and offset */
  int phyaddr = fpn * PAGING_PAGESZ + off;

  /* Write to physical memory.
   * This simulates a SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE.
   * MEMPHY_write is expected to return 0 on success.
   */
  if (MEMPHY_write(caller->mram, phyaddr, value) != 0)
      return -1;

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  /* TODO update result of reading action*/
  //destination 
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, 0, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if(pg == NULL)
    return -1;
  
  *retpgn = pg->pgn;
  mm->fifo_pgn = pg->pg_next; // Move to the next page in the FIFO list  

  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* TODO Traverse on list of free vm region to find a fit space */
  struct vm_rg_struct *prev = NULL;
  while (rgit != NULL) {
    int avail = rgit->rg_end - rgit->rg_start;
    if (avail >= size) {
        newrg->rg_start = rgit->rg_start;
        newrg->rg_end = newrg->rg_start + size;
        // Adjust the free region in the list
        rgit->rg_start = newrg->rg_end;
        if (rgit->rg_start >= rgit->rg_end) {
            if (prev == NULL)
                cur_vma->vm_freerg_list = rgit->rg_next;
            else
                prev->rg_next = rgit->rg_next;
            free(rgit);
        }
        return 0;
    }
    prev = rgit;
    rgit = rgit->rg_next;
  }
  
  return 0;
}

//#endif
