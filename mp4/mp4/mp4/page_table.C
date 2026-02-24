#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;


void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
    PageTable::kernel_mem_pool = _kernel_mem_pool;
    PageTable::process_mem_pool = _process_mem_pool;
    PageTable::shared_size = _shared_size;
    Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
    // Set initial base address for page table entries
    unsigned long start_address = 0;  
    unsigned long total_frames = PageTable::shared_size / PAGE_SIZE; // Calculate required frames for kernel pool

    // Reserve memory for page table and directory
    unsigned long* page_table = (unsigned long*) (process_mem_pool->get_frames(1) * PAGE_SIZE);
    page_directory = (unsigned long*) (process_mem_pool->get_frames(1) * PAGE_SIZE);

    // Populate the page table
    for (int i = 0; i < total_frames; i++) {
        page_table[i] = start_address | 3; // Set present and writable bits
        start_address += PAGE_SIZE;
    }

    // Set up the page directory
    for (int i = 0; i < 1024; i++) {
        if (i == 0) {
            page_directory[i] = (unsigned long) (page_table) | 3;
        } else if (i == 1023) {
            page_directory[i] = (unsigned long) page_directory | 3;
        } else {
            page_directory[i] = 2; // Mark as not present
        }
    }
    start_address = 0;
    Console::puts("Constructed Page Table object\n");
}


void PageTable::load()
{
    current_page_table = this;  // Assign the current instance to track the active page table
    write_cr3((unsigned long) (page_directory)); // Update CR3 register with page directory address
    Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
    paging_enabled = true;  // Mark paging as active
    write_cr0(read_cr0() | 0x80000000); // Set the paging bit in CR0 register to activate paging
    Console::puts("Enabled paging\n");
}

unsigned long* PageTable::PDE_address(unsigned long addr) {
    unsigned long pde_addrs = (addr >> 20);     // Shift address by 20 bits to get PDE index
    pde_addrs |= 0xFFFFF000;                    // Map PDE to right memory space
    pde_addrs &= 0xFFFFFFFC;                    // Clear last 2 bits
    return (unsigned long*) pde_addrs;          // Address of PDE
}

unsigned long* PageTable::PTE_address(unsigned long addr) {
    unsigned long pte_addrs = (addr >> 10);     // Shift address by 10 bits to get PTE index
    pte_addrs |= 0xFFC00000;                    // Map PTE to right memory space
    pte_addrs &= 0xFFFFFFFC;                    // Clear last 2 bits 
    return (unsigned long*) (pte_addrs);        // Address of PTE
}

void PageTable::handle_fault(REGS * _r)
{
    Console::puts("Handling page fault...\n");
    unsigned long error_code = _r->err_code;

   // If the fault is caused by a reserved bit violation, ignore it
   if ((error_code & 0x1) == 1) {
       return;
   }

   if (!PageTable::current_page_table) {
        Console::puts("ERROR: No active page table! System halted.\n");
        assert(false); // Stop execution
    }

    if (!PageTable::current_page_table->pool_count) {
        Console::puts("ERROR: No registered VM pools! System halted.\n");
        assert(false);
    }

    unsigned long faulting_address = read_cr2();

    // Check if the memory address is inside a valid Virtual Memory Pool
    bool valid = false;
    for (int i = 0; i < PageTable::current_page_table->pool_count; i++) {
        if (!PageTable::current_page_table->registered_pools[i]) {
            Console::puts("ERROR: Found NULL pool in registered_pools!\n");
            assert(false);
        }
        if (PageTable::current_page_table->registered_pools[i]->is_legitimate(faulting_address)) {  
            valid = true;
            break;
        }
    }

    // If the address is NOT in any valid pool - Segmentation Fault!
    if (!valid) {  
        Console::puts("Segmentation Fault: Invalid Memory Access!\n");
        assert(false);  
    }

    unsigned long* pde = PageTable::current_page_table->PDE_address(faulting_address);
    unsigned long* pte = PageTable::current_page_table->PTE_address(faulting_address);

    if (!(*pde & 1)) {
        *pde = (unsigned long) (PageTable::process_mem_pool->get_frames(1) * PAGE_SIZE) | 3;
    }

    if (!(*pte & 1)) {
        *pte = (unsigned long) (PageTable::process_mem_pool->get_frames(1) * PAGE_SIZE) | 3;
    }

    Console::puts("handled page fault\n");
}


void PageTable::register_pool(VMPool * _vm_pool)
{
    if (pool_count == MAX_POOLS) {  
        Console::puts("Virtual memory is full. Cannot register more pools!\n");
        assert(false);       
    }
    registered_pools[pool_count++] = _vm_pool;  // Add pool to array
    Console::puts("registered VM pool\n");
}

void PageTable::free_page(unsigned long _page_no) {
    unsigned long *pte = PTE_address(_page_no);
    if(*pte & 1) {
        process_mem_pool -> release_frames(*pte >> 12);
        *pte = 2;
    }
    write_cr3((unsigned long) page_directory); // Flush TLB
    Console::puts("freed page\n");
}
