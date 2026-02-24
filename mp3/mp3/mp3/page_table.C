#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

// Static member initialization
PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;

/*------------------------------------------------------------*/
/* Initialize the global paging system parameters */
/*------------------------------------------------------------*/
void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
    PageTable::kernel_mem_pool = _kernel_mem_pool;
    PageTable::process_mem_pool = _process_mem_pool;
    PageTable::shared_size = _shared_size;
    Console::puts("Initialized Paging System\n");
}

/*------------------------------------------------------------*/
/* Construct a page table */
/*------------------------------------------------------------*/
PageTable::PageTable()
{
    // Calculate number of frames to identity-map (up to 4 MB)
    unsigned long total_frames = shared_size / PAGE_SIZE;
    if (total_frames > 1024) total_frames = 1024;

    // Allocate one frame for page directory
    page_directory = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);

    // Allocate one frame for the first page table (4 MB region)
    unsigned long * page_table = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);

    // Identity map the first 4MB (virtual = physical)
    unsigned long phys_addr = 0;
    for (unsigned int i = 0; i < total_frames; i++) {
        page_table[i] = (phys_addr) | 3; // Present + Writable
        phys_addr += PAGE_SIZE;
    }

    // Initialize all page directory entries
    for (int i = 0; i < 1024; i++) {
        if (i == 0)
            page_directory[i] = ((unsigned long)page_table) | 3; // First 4MB mapped
        else
            page_directory[i] = 0; // Not present
    }

    Console::puts("Constructed Page Table object\n");
}

/*------------------------------------------------------------*/
/* Load this page table (updates CR3) */
/*------------------------------------------------------------*/
void PageTable::load()
{
    current_page_table = this;
    write_cr3((unsigned long)page_directory);
    Console::puts("Loaded page table\n");
}

/*------------------------------------------------------------*/
/* Enable paging on the CPU */
/*------------------------------------------------------------*/
void PageTable::enable_paging()
{
    paging_enabled = 1;
    write_cr0(read_cr0() | 0x80000000); // Set PG bit in CR0
    Console::puts("Enabled paging\n");
}

/*------------------------------------------------------------*/
/* Handle a page fault exception */
/*------------------------------------------------------------*/
void PageTable::handle_fault(REGS * _r)
{
    unsigned long faulting_address = read_cr2();  // Address that caused the fault
    unsigned long error_code = _r->err_code;

    Console::puts("Page fault occurred\n");

    // Extract directory and table indices
    unsigned long dir_index  = (faulting_address >> 22) & 0x3FF;
    unsigned long table_index = (faulting_address >> 12) & 0x3FF;

    // Get current page directory
    unsigned long * directory_base = (unsigned long *)(read_cr3());

    unsigned long dir_entry = directory_base[dir_index];
    unsigned long * page_table_ptr;

    // Check if directory entry is valid
    if (!(dir_entry & 1)) {
        // Allocate a new page table from kernel memory
        unsigned long new_table_frame = kernel_mem_pool->get_frames(1);
        page_table_ptr = (unsigned long *)(new_table_frame * PAGE_SIZE);

        // Initialize all entries as not present
        for (int i = 0; i < 1024; i++) {
            page_table_ptr[i] = 0;
        }

        // Update directory entry
        directory_base[dir_index] = ((unsigned long)page_table_ptr) | 3;
    } else {
        // Retrieve existing page table base
        page_table_ptr = (unsigned long *)(dir_entry & 0xFFFFF000);
    }

    // Allocate a new physical frame for the missing page
    unsigned long new_frame = process_mem_pool->get_frames(1);
    page_table_ptr[table_index] = (new_frame * PAGE_SIZE) | 3;  // Present + Writable

    // Refresh TLB
    write_cr3(read_cr3());

    Console::puts("Handled page fault successfully\n");
}
