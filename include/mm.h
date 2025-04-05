#ifndef MM_H
#define MM_H

#include "bitops.h"       // Required for BIT(), GENMASK(), NBITS(), etc.
#include "common.h"

/*===========================================================================
 * CPU Bus and Paging Definitions
 *===========================================================================*/
#define PAGING_CPU_BUS_WIDTH 22        /* 22-bit bus; maximum space = 4MB */
#define PAGING_PAGESZ        256       /* Page size: 256 bytes */
#define PAGING_MEMRAMSZ      BIT(21)    /* Physical RAM size (bit mask) */
#define PAGING_PAGE_ALIGNSZ(sz) (DIV_ROUND_UP((sz), PAGING_PAGESZ) * PAGING_PAGESZ)

#define PAGING_MEMSWPSZ      BIT(29)    /* Swap memory size (bit mask) */
#define PAGING_SWPFPN_OFFSET 5          /* Swap frame number offset */
#define PAGING_MAX_PGN       (DIV_ROUND_UP(BIT(PAGING_CPU_BUS_WIDTH), PAGING_PAGESZ))

#define PAGING_SBRK_INIT_SZ  PAGING_PAGESZ

/*===========================================================================
 * PTE Bit Definitions and Macros
 *===========================================================================*/
#define PAGING_PTE_PRESENT_MASK   BIT(31)
#define PAGING_PTE_SWAPPED_MASK   BIT(30)
#define PAGING_PTE_RESERVE_MASK   BIT(29)
#define PAGING_PTE_DIRTY_MASK     BIT(28)
#define PAGING_PTE_EMPTY01_MASK   BIT(14)
#define PAGING_PTE_EMPTY02_MASK   BIT(13)

/* PTE utility macros */
#define PAGING_PTE_SET_PRESENT(pte)  ((pte) |= PAGING_PTE_PRESENT_MASK)
#define PAGING_PAGE_PRESENT(pte)     ((pte) & PAGING_PTE_PRESENT_MASK)

/* User number (not used in this example) */
#define PAGING_PTE_USRNUM_LOBIT 15
#define PAGING_PTE_USRNUM_HIBIT 27

/* Frame Page Number (FPN) */
#define PAGING_PTE_FPN_LOBIT 0
#define PAGING_PTE_FPN_HIBIT 12

/* Swap Type and Swap Offset */
#define PAGING_PTE_SWPTYP_LOBIT 0
#define PAGING_PTE_SWPTYP_HIBIT 4
#define PAGING_PTE_SWPOFF_LOBIT 5
#define PAGING_PTE_SWPOFF_HIBIT 25

/* PTE Masks */
#define PAGING_PTE_USRNUM_MASK GENMASK(PAGING_PTE_USRNUM_HIBIT, PAGING_PTE_USRNUM_LOBIT)
#define PAGING_PTE_FPN_MASK    GENMASK(PAGING_PTE_FPN_HIBIT, PAGING_PTE_FPN_LOBIT)
#define PAGING_PTE_SWPTYP_MASK GENMASK(PAGING_PTE_SWPTYP_HIBIT, PAGING_PTE_SWPTYP_LOBIT)
#define PAGING_PTE_SWPOFF_MASK GENMASK(PAGING_PTE_SWPOFF_HIBIT, PAGING_PTE_SWPOFF_LOBIT)

/*===========================================================================
 * Address Bit Ranges for Paging
 *===========================================================================*/
/* OFFSET Bits */
#define PAGING_ADDR_OFFST_LOBIT 0
#define PAGING_ADDR_OFFST_HIBIT (NBITS(PAGING_PAGESZ) - 1)

/* Page Number Bits */
#define PAGING_ADDR_PGN_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_ADDR_PGN_HIBIT (PAGING_CPU_BUS_WIDTH - 1)

/* Frame PHY Number Bits */
#define PAGING_ADDR_FPN_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_ADDR_FPN_HIBIT (NBITS(PAGING_MEMRAMSZ) - 1)

/* Swap Frame Number Bits */
#define PAGING_SWP_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_SWP_HIBIT (NBITS(PAGING_MEMSWPSZ) - 1)
#define PAGING_SWP(pte) (((pte) & PAGING_PTE_SWPOFF_MASK) >> PAGING_SWPFPN_OFFSET)

/*===========================================================================
 * Value Operators
 *===========================================================================*/
#define SETBIT(v, mask)   ((v) |= (mask))
#define CLRBIT(v, mask)   ((v) &= ~(mask))
#define SETVAL(v, value, mask, offst) ((v) = (((v) & ~(mask)) | (((value) << (offst)) & (mask))))
#define GETVAL(v, mask, offst) (((v) & (mask)) >> (offst))

/*===========================================================================
 * Masks for Address Extraction
 *===========================================================================*/
#define PAGING_OFFST_MASK GENMASK(PAGING_ADDR_OFFST_HIBIT, PAGING_ADDR_OFFST_LOBIT)
#define PAGING_PGN_MASK   GENMASK(PAGING_ADDR_PGN_HIBIT, PAGING_ADDR_PGN_LOBIT)
#define PAGING_FPN_MASK   GENMASK(PAGING_ADDR_FPN_HIBIT, PAGING_ADDR_FPN_LOBIT)
#define PAGING_SWP_MASK   GENMASK(PAGING_SWP_HIBIT, PAGING_SWP_LOBIT)

/* Macros to extract values from an address/PTE */
#define PAGING_OFFST(x)  GETVAL((x), PAGING_OFFST_MASK, PAGING_ADDR_OFFST_LOBIT)
#define PAGING_PGN(x)    GETVAL((x), PAGING_PGN_MASK, PAGING_ADDR_PGN_LOBIT)
#define PAGING_FPN(x)    GETVAL((x), PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT)

/*===========================================================================
 * VM Region and Paging Function Prototypes
 *===========================================================================*/

/* VM Region: Initialization and list management */
struct vm_rg_struct * init_vm_rg(int rg_start, int rg_end);
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode);
int enlist_pgn_node(struct pgn_t **pgnlist, int pgn);

/* Paging functions */
int vmap_page_range(struct pcb_t *caller, int addr, int pgnum, 
                    struct framephy_struct *frames, struct vm_rg_struct *ret_rg);
int vm_map_ram(struct pcb_t *caller, int astart, int send, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg);
int alloc_pages_range(struct pcb_t *caller, int incpgnum, struct framephy_struct **frm_lst);
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                   struct memphy_struct *mpdst, int dstfpn);
int pte_set_fpn(uint32_t *pte, int fpn);
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff);
int init_pte(uint32_t *pte, int pre, int fpn, int drt, int swp, int swptyp, int swpoff);
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr);
int __free(struct pcb_t *caller, int vmaid, int rgid);
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data);
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value);
int init_mm(struct mm_struct *mm, struct pcb_t *caller);

/* VM Prototypes */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index);
int pgfree_data(struct pcb_t *proc, uint32_t reg_index);
int pgread(struct pcb_t *proc, uint32_t source, uint32_t offset, uint32_t destination);
int pgwrite(struct pcb_t *proc, BYTE data, uint32_t destination, uint32_t offset);

/* Local VM Prototypes */
struct vm_rg_struct * get_symrg_byid(struct mm_struct *mm, int rgid);
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend);
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg);
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz);
int find_victim_page(struct mm_struct *mm, int *pgn);
struct vm_area_struct * get_vma_by_num(struct mm_struct *mm, int vmaid);

/* Memory/Physical prototypes */
int MEMPHY_get_freefp(struct memphy_struct *mp, int *fpn);
int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn);
int MEMPHY_read(struct memphy_struct *mp, int addr, BYTE *value);
int MEMPHY_write(struct memphy_struct *mp, int addr, BYTE data);
int MEMPHY_dump(struct memphy_struct *mp);
int init_memphy(struct memphy_struct *mp, int max_size, int randomflg);

/* Print Functions */
int print_list_fp(struct framephy_struct *fp);
int print_list_rg(struct vm_rg_struct *rg);
int print_list_vma(struct vm_area_struct *vma);
int print_list_pgn(struct pgn_t *ip);
int print_pgtbl(struct pcb_t *ip, uint32_t start, uint32_t end);

#endif /* MM_H */