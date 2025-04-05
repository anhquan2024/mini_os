// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/* 
Assumed structure definitions based on usage:
struct vm_area_struct {
    int vm_id;
    int vm_start;
    int vm_end;
    struct vm_area_struct *vm_next;
    struct vm_rg_struct *vm_freerg_list;
};

struct vm_rg_struct {
    int rg_start;
    int rg_end;
    struct vm_rg_struct *rg_next;
};
*/

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = pvma->vm_id;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
    if (pvma == NULL)
      return NULL;
    vmait = pvma->vm_id;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, int vicfpn, int swpfpn)
{
    __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node_at_brk - get vm area node for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: requested size in bytes
 *@alignedsz: aligned size (in bytes) for mapping (obtained via PAGING_PAGE_ALIGNSZ)
 *
 * This function creates a new vm region node whose boundaries start at the current
 * break pointer (bp) of the pcb and extend by the aligned size. It also bumps the bp.
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct *newrg;
  /* Optionally retrieve current vma if needed:
     struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  */
  newrg = malloc(sizeof(struct vm_rg_struct));
  if (!newrg)
      return NULL;

  /* Update the new region boundaries:
     rg_start is set to the current break pointer,
     rg_end is set to rg_start + aligned size.
     Also update the break pointer (bp) in caller.
  */
  newrg->rg_start = caller->bp;
  newrg->rg_end = caller->bp + alignedsz;
  newrg->rg_next = NULL;
  caller->bp += alignedsz;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: planned start address of the new region
 *@vmaend: planned end address of the new region
 *
 * This function validates that the new planned memory area does not overlap with
 * the current vm area. For simplicity, we assume that if the new rg_start is not
 * below the current vm_end then there is no overlap.
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct *vma = get_vma_by_num(caller->mm, vmaid);
  if (vma == NULL)
    return -1;
  /* If the new region starts before the current vm_end, it overlaps */
  if (vmastart < vma->vm_end)
    return -1;
  return 0;
}

/* Dummy implementation of vm_map_ram:
   Map the newly allocated vm region into the physical memory (MEMRAM).
   Here we simply assign the new region boundaries to ret_rg.
   In a real system, this would actually map the pages.
*/
// int vm_map_ram(struct pcb_t *caller, int astart, int send, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
// {
//     /* For our purposes assume mapping is successful. */
//     ret_rg->rg_start = astart;
//     ret_rg->rg_end = send;
//     return 0;
// }

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size in bytes
 *
 * This function increases the memory area limits and maps the additional region.
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  struct vm_rg_struct *newrg_tmp = malloc(sizeof(struct vm_rg_struct));
  if (!newrg_tmp)
      return -1;
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  if (!area)
      return -1;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (!cur_vma)
      return -1;

  int old_end = cur_vma->vm_end;

  /* Validate that the new area does not overlap */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
    return -1; /* Overlap detected and failed allocation */

  /* Extend the current vm area's limit to include the new region */
  cur_vma->vm_end = area->rg_end;

  /* Map the new memory region into MEMRAM */
  if (vm_map_ram(caller, area->rg_start, area->rg_end, old_end, incnumpage, newrg_tmp) < 0)
    return -1; /* Mapping failed */

  /* Successful expansion, free the temporary mapping structure if needed */
  free(newrg_tmp);
  return 0;
}

// #endif