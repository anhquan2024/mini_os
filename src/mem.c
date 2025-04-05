#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>
#include "common.h"

static BYTE _ram[RAM_SIZE];

static struct {
    uint32_t proc;	// ID of process currently uses this page
    int index;	// Index of the page in the list of pages allocated to the process.
    int next;	// The next page in the list. -1 if it is the last page.
} _mem_stat [NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
    memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
    memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
    pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
    return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
    return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
    return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct trans_table_t * get_trans_table(
        addr_t index, 	// Segment level index
        struct page_table_t * page_table) { // first level table
    
    /* Implement search on the first level table */
    int i;
    for (i = 0; i < page_table->size; i++) {
        if (page_table->table[i].v_index == index) {
            return page_table->table[i].next_lv;
        }
    }
    return NULL;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
        addr_t virtual_addr, 	// Given virtual address
        addr_t * physical_addr, // Physical address to be returned
        struct pcb_t * proc) {  // Process uses given virtual address

    /* Offset of the virtual address */
    addr_t offset = get_offset(virtual_addr);
    /* The first layer index */
    addr_t first_lv = get_first_lv(virtual_addr);
    /* The second layer index */
    addr_t second_lv = get_second_lv(virtual_addr);
    
    /* Search in the first level */
    struct trans_table_t * trans_table = NULL;
    trans_table = get_trans_table(first_lv, proc->page_table);
    if (trans_table == NULL) {
        return 0;
    }

    int i;
    for (i = 0; i < trans_table->size; i++) {
        if (trans_table->table[i].v_index == second_lv) {
            /* Found the mapping: reconstruct the physical address.
             * The physical page number is stored in p_index; shift it and add offset. */
            *physical_addr = (trans_table->table[i].p_index << OFFSET_LEN) | offset;
            return 1;
        }
    }
    return 0;	
}

/* Helper: add a mapping from virtual address to physical page in the process's page table */
static void add_page_mapping(struct pcb_t *proc, addr_t virtual_addr, addr_t physical_page) {
    addr_t first_lv = get_first_lv(virtual_addr);
    addr_t second_lv = get_second_lv(virtual_addr);
    int i, found = 0;
    struct trans_table_t *trans = NULL;
    
    // Search for an existing first-level entry
    for(i = 0; i < proc->page_table->size; i++){
        if(proc->page_table->table[i].v_index == first_lv){
            trans = proc->page_table->table[i].next_lv;
            found = 1;
            break;
        }
    }
    // If not found, create a new second-level table and add a first-level entry
    if(!found){
        trans = (struct trans_table_t *)malloc(sizeof(struct trans_table_t));
        trans->size = 0;
        proc->page_table->table[proc->page_table->size].v_index = first_lv;
        proc->page_table->table[proc->page_table->size].next_lv = trans;
        proc->page_table->size++;
    }
    // Add the mapping into the second-level table
    trans->table[trans->size].v_index = second_lv;
    trans->table[trans->size].p_index = physical_page;
    trans->size++;
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
    pthread_mutex_lock(&mem_lock);
    addr_t ret_mem = 0;
    
    /* Calculate number of pages required */
    uint32_t num_pages = (size % PAGE_SIZE) ? size / PAGE_SIZE + 1 : size / PAGE_SIZE;
    
    /* Check if there is enough free physical pages */
    uint32_t free_page_count = 0;
    int i;
    for(i = 0; i < NUM_PAGES; i++){
        if(_mem_stat[i].proc == 0)
            free_page_count++;
    }
    
    /* Check if virtual address space is available based on break pointer */
    if ((proc->bp + num_pages * PAGE_SIZE) > RAM_SIZE || free_page_count < num_pages) {
        pthread_mutex_unlock(&mem_lock);
        return 0;
    }
    
    /* Allocate new memory region */
    ret_mem = proc->bp;
    proc->bp += num_pages * PAGE_SIZE;
    
    /* Allocate physical pages and update _mem_stat */
    int allocated = 0;
    int prev_page = -1;
    for(i = 0; i < NUM_PAGES && allocated < (int)num_pages; i++){
        if(_mem_stat[i].proc == 0){
            _mem_stat[i].proc = proc->pid;
            _mem_stat[i].index = allocated;
            _mem_stat[i].next = -1;
            if (prev_page != -1) {
                _mem_stat[prev_page].next = i;
            }
            prev_page = i;
            
            /* Add entry in process's page table for this page */
            add_page_mapping(proc, ret_mem + allocated * PAGE_SIZE, i);
            
            allocated++;
        }
    }
    
    pthread_mutex_unlock(&mem_lock);
    return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
    /* A simple stub implementation.
     * A full implementation would remove page table entries and mark the physical pages as free.
     */
    return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
    addr_t physical_addr;
    if (translate(address, &physical_addr, proc)) {
        *data = _ram[physical_addr];
        return 0;
    }else{
        return 1;
    }
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
    addr_t physical_addr;
    if (translate(address, &physical_addr, proc)) {
        _ram[physical_addr] = data;
        return 0;
    }else{
        return 1;
    }
}

void dump(void) {
    int i;
    for (i = 0; i < NUM_PAGES; i++) {
        if (_mem_stat[i].proc != 0) {
            printf("%03d: ", i);
            printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
                i << OFFSET_LEN,
                ((i + 1) << OFFSET_LEN) - 1,
                _mem_stat[i].proc,
                _mem_stat[i].index,
                _mem_stat[i].next
            );
            int j;
            for (	j = i << OFFSET_LEN;
                j < ((i+1) << OFFSET_LEN) - 1;
                j++) {
                
                if (_ram[j] != 0) {
                    printf("\t%05x: %02x\n", j, _ram[j]);
                }
                    
            }
        }
    }
}