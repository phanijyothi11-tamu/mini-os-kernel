/*
    File: page_table.H

    Author: Implementation for MP5 - Process Support
            Department of Computer Science
            Texas A&M University

    Description: Page Table and Page Directory Management for Process Support.
                 This implements x86 paging with 4KB pages and 4MB page directories.
*/

#ifndef _PAGE_TABLE_H_
#define _PAGE_TABLE_H_

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "machine.H"
#include "frame_pool.H"

/*--------------------------------------------------------------------------*/
/* PAGE TABLE ENTRY */
/*--------------------------------------------------------------------------*/

/* Page Table Entry (PTE) - 32 bits
   Bit 0: Present (P)
   Bit 1: Read/Write (R/W)
   Bit 2: User/Supervisor (U/S)
   Bit 3: Page Write Through (PWT)
   Bit 4: Page Cache Disable (PCD)
   Bit 5: Accessed (A)
   Bit 6: Dirty (D)
   Bit 7: Page Size (PS) - 0 for 4KB pages
   Bits 8-11: Reserved
   Bits 12-31: Physical Page Frame Address (20 bits, 4KB aligned)
*/

typedef unsigned int PageTableEntry;

/*--------------------------------------------------------------------------*/
/* PAGE DIRECTORY ENTRY */
/*--------------------------------------------------------------------------*/

/* Page Directory Entry (PDE) - 32 bits
   Similar structure to PTE, but points to a page table
*/

typedef unsigned int PageDirectoryEntry;

/*--------------------------------------------------------------------------*/
/* PAGE TABLE */
/*--------------------------------------------------------------------------*/

class PageTable {
public:
    PageTableEntry entries[Machine::PT_ENTRIES_PER_PAGE];
    
    PageTable();
    /* Initialize all entries to 0 (not present) */
    
    void map_page(unsigned int virtual_addr, unsigned long physical_addr, 
                  bool is_writable = true, bool is_user = false);
    /* Map a virtual page to a physical page */
    
    void unmap_page(unsigned int virtual_addr);
    /* Unmap a virtual page */
    
    PageTableEntry * get_entry(unsigned int virtual_addr);
    /* Get pointer to the page table entry for the given virtual address */
};

/*--------------------------------------------------------------------------*/
/* PAGE DIRECTORY */
/*--------------------------------------------------------------------------*/

class PageDirectory {
private:
    PageDirectoryEntry * page_dir;  /* Physical address of page directory */
    FramePool * frame_pool;        /* Frame pool for allocating page tables */
    
public:
    PageDirectory(FramePool * _frame_pool);
    /* Create a new page directory with identity mapping for kernel */
    
    ~PageDirectory();
    /* Cleanup page directory and associated page tables */
    
    unsigned long get_physical_address();
    /* Get the physical address of the page directory (for CR3) */
    
    void map_page(unsigned int virtual_addr, unsigned long physical_addr,
                  bool is_writable = true, bool is_user = false);
    /* Map a virtual page in this address space */
    
    void unmap_page(unsigned int virtual_addr);
    /* Unmap a virtual page */
    
    void identity_map_kernel(unsigned long start_addr, unsigned long end_addr);
    /* Identity map kernel memory (virtual = physical) */
    
    void copy_kernel_mappings(PageDirectory * source);
    /* Copy kernel mappings from another page directory */
    
    PageDirectoryEntry * get_page_dir() { return page_dir; }
    /* Get pointer to page directory entries (for copying) */
};

/*--------------------------------------------------------------------------*/
/* PAGING CONTROL FUNCTIONS */
/*--------------------------------------------------------------------------*/

void enable_paging(unsigned long page_dir_physical);
/* Enable paging and load the page directory into CR3 */

void load_page_directory(unsigned long page_dir_physical);
/* Load a page directory into CR3 (for context switching) */

unsigned long get_current_page_directory();
/* Get the current page directory address from CR3 */

#endif

