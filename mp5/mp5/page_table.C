/*
    File: page_table.C

    Author: Implementation for MP5 - Process Support
            Department of Computer Science
            Texas A&M University

    Description: Implementation of Page Table and Page Directory Management.
*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "page_table.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "machine.H"

/*--------------------------------------------------------------------------*/
/* PAGE TABLE IMPLEMENTATION */
/*--------------------------------------------------------------------------*/

PageTable::PageTable() {
    /* Initialize all entries to 0 (not present) */
    for (int i = 0; i < Machine::PT_ENTRIES_PER_PAGE; i++) {
        entries[i] = 0;
    }
}

void PageTable::map_page(unsigned int virtual_addr, unsigned long physical_addr,
                         bool is_writable, bool is_user) {
    /* Extract page offset (bits 0-11) and page table index (bits 12-21) */
    unsigned int page_index = (virtual_addr >> 12) & 0x3FF;  /* 10 bits */
    
    /* Create page table entry */
    PageTableEntry entry = (physical_addr & 0xFFFFF000) |  /* Physical address, 4KB aligned */
                           0x001;                          /* Present bit */
    
    if (is_writable) {
        entry |= 0x002;  /* Read/Write bit */
    }
    
    if (is_user) {
        entry |= 0x004;  /* User/Supervisor bit */
    }
    
    entries[page_index] = entry;
}

void PageTable::unmap_page(unsigned int virtual_addr) {
    unsigned int page_index = (virtual_addr >> 12) & 0x3FF;
    entries[page_index] = 0;  /* Clear present bit */
}

PageTableEntry * PageTable::get_entry(unsigned int virtual_addr) {
    unsigned int page_index = (virtual_addr >> 12) & 0x3FF;
    return &entries[page_index];
}

/*--------------------------------------------------------------------------*/
/* PAGE DIRECTORY IMPLEMENTATION */
/*--------------------------------------------------------------------------*/

PageDirectory::PageDirectory(FramePool * _frame_pool) {
    frame_pool = _frame_pool;
    
    /* Allocate a frame for the page directory */
    unsigned long pd_frame = frame_pool->get_frame();
    if (pd_frame == 0) {
        Console::puts("ERROR: Failed to allocate frame for page directory!\n");
        assert(false);
    }
    
    page_dir = (PageDirectoryEntry *)pd_frame;
    
    /* Initialize all entries to 0 (not present) */
    for (int i = 0; i < Machine::PT_ENTRIES_PER_PAGE; i++) {
        page_dir[i] = 0;
    }
    
    Console::puts("Created new page directory at physical address: ");
    Console::putui((unsigned int)pd_frame);
    Console::puts("\n");
}

PageDirectory::~PageDirectory() {
    /* Free all page tables */
    for (int i = 0; i < Machine::PT_ENTRIES_PER_PAGE; i++) {
        if (page_dir[i] & 0x001) {  /* If present */
            unsigned long pt_physical = page_dir[i] & 0xFFFFF000;
            // Note: In a full implementation, we'd need to track which
            // page tables we allocated vs which are shared (kernel)
            // For now, we'll just clear the entry
            page_dir[i] = 0;
        }
    }
    
    /* Free the page directory frame */
    // Note: FramePool doesn't support release, so we can't actually free it
}

unsigned long PageDirectory::get_physical_address() {
    return (unsigned long)page_dir;
}

void PageDirectory::map_page(unsigned int virtual_addr, unsigned long physical_addr,
                              bool is_writable, bool is_user) {
    /* Extract page directory index (bits 22-31) and page table index (bits 12-21) */
    unsigned int pd_index = (virtual_addr >> 22) & 0x3FF;  /* 10 bits */
    unsigned int pt_index = (virtual_addr >> 12) & 0x3FF;  /* 10 bits */
    
    /* Check if page table exists */
    if (!(page_dir[pd_index] & 0x001)) {
        /* Page table doesn't exist, allocate one */
        unsigned long pt_frame = frame_pool->get_frame();
        if (pt_frame == 0) {
            Console::puts("ERROR: Failed to allocate frame for page table!\n");
            assert(false);
        }
        
        /* Create page directory entry pointing to the page table */
        page_dir[pd_index] = (pt_frame & 0xFFFFF000) | 0x003;  /* Present + Read/Write */
        
        /* Initialize the page table */
        PageTable * pt = (PageTable *)pt_frame;
        for (int i = 0; i < Machine::PT_ENTRIES_PER_PAGE; i++) {
            pt->entries[i] = 0;
        }
    }
    
    /* Get the page table */
    unsigned long pt_physical = page_dir[pd_index] & 0xFFFFF000;
    PageTable * pt = (PageTable *)pt_physical;
    
    /* Map the page */
    pt->map_page(virtual_addr, physical_addr, is_writable, is_user);
}

void PageDirectory::unmap_page(unsigned int virtual_addr) {
    unsigned int pd_index = (virtual_addr >> 22) & 0x3FF;
    
    if (page_dir[pd_index] & 0x001) {
        unsigned long pt_physical = page_dir[pd_index] & 0xFFFFF000;
        PageTable * pt = (PageTable *)pt_physical;
        pt->unmap_page(virtual_addr);
    }
}

void PageDirectory::identity_map_kernel(unsigned long start_addr, unsigned long end_addr) {
    /* Identity map kernel memory (virtual address = physical address) */
    unsigned long current = start_addr;
    while (current < end_addr) {
        map_page((unsigned int)current, current, true, false);  /* Writable, supervisor */
        current += Machine::PAGE_SIZE;
    }
}

void PageDirectory::copy_kernel_mappings(PageDirectory * source) {
    /* Copy kernel page directory entries from source page directory */
    /* For this simple implementation, we copy all present entries */
    /* In a full OS, we'd typically copy only kernel space entries (768-1023) */
    /* But since we identity-mapped low memory, we need to copy those too */
    PageDirectoryEntry * source_pd = source->get_page_dir();
    int copied = 0;
    for (int i = 0; i < Machine::PT_ENTRIES_PER_PAGE; i++) {
        if (source_pd[i] & 0x001) {  /* If entry is present */
            page_dir[i] = source_pd[i];
            copied++;
        }
    }
    Console::puts("  Copied ");
    Console::puti(copied);
    Console::puts(" page directory entries\n");
}

/*--------------------------------------------------------------------------*/
/* PAGING CONTROL FUNCTIONS */
/*--------------------------------------------------------------------------*/

void enable_paging(unsigned long page_dir_physical) {
    /* Load page directory into CR3 */
    __asm__ __volatile__ (
        "mov %0, %%cr3"
        :
        : "r" (page_dir_physical)
    );
    
    /* Enable paging by setting PG bit (bit 31) in CR0 */
    unsigned long cr0;
    __asm__ __volatile__ (
        "mov %%cr0, %0"
        : "=r" (cr0)
    );
    cr0 |= 0x80000000;  /* Set PG bit */
    __asm__ __volatile__ (
        "mov %0, %%cr0"
        :
        : "r" (cr0)
    );
    
    Console::puts("Paging enabled with page directory at: ");
    Console::putui((unsigned int)page_dir_physical);
    Console::puts("\n");
}

void load_page_directory(unsigned long page_dir_physical) {
    /* Load page directory into CR3 (for context switching) */
    __asm__ __volatile__ (
        "mov %0, %%cr3"
        :
        : "r" (page_dir_physical)
    );
}

unsigned long get_current_page_directory() {
    /* Get current page directory from CR3 */
    unsigned long cr3;
    __asm__ __volatile__ (
        "mov %%cr3, %0"
        : "=r" (cr3)
    );
    return cr3;
}

