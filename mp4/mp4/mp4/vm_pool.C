/*
 File: vm_pool.C
 
 Author: ps41
 Date  : 2024/09/20
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table)
               :base_address(_base_address), size(_size), frame_pool(_frame_pool), page_table(_page_table) {
    
    page_table->register_pool(this);
    
    // Metadata (allocated and free region arrays) stored in first two pages
    allocated_regions = (Region*) base_address;
    free_regions = (Region*)(base_address + PageTable::PAGE_SIZE);

    // Initialize allocated region: metadata and free regions: entire pool (excluding metadata)
    num_allocated = 1;
    num_free = 1;
    allocated_regions[0].start_addr = base_address;
    allocated_regions[0].size = 2 * PageTable::PAGE_SIZE;                 // 2 pages allocated for metadata
    free_regions[0].start_addr = base_address + 2 * PageTable::PAGE_SIZE;
    free_regions[0].size = size - 2 * PageTable::PAGE_SIZE;               // entire pool (excluding metadata)

    Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long _size) {
    if (this == nullptr) {
        Console::puts("ERROR: `this` is NULL in VMPool::allocate()!\n");
        assert(false);
    }

    if(num_allocated >= MAX_REGIONS or num_free <= 0) {         // Check if memory regions are left
		Console::puts("Error in allocator, no memory\n");
        assert(false);
    }

    unsigned long pages_needed = (_size + PageTable::PAGE_SIZE - 1) / PageTable::PAGE_SIZE;
    unsigned long bytes_needed = pages_needed * PageTable::PAGE_SIZE;

    // Find a free region that can fit the allocation
    for (unsigned int i = 0; i < num_free; i++) {
        if (free_regions[i].size >= bytes_needed) {
            unsigned long allocated_addr = free_regions[i].start_addr;
            allocated_regions[num_allocated++] = {allocated_addr, bytes_needed};

            // Update free regions (split if remaining space)
            if (free_regions[i].size > bytes_needed) {
                free_regions[i].start_addr += bytes_needed;
                free_regions[i].size -= bytes_needed;
            } else {
                free_regions[i] = free_regions[num_free - 1];
                num_free--;
            }
            Console::puts("Allocated region of memory.\n");
            return allocated_addr;
        }
    }
    Console::puts("Allocation failed.\n");
    return 0;
}

void VMPool::release(unsigned long _start_address) {
    // Find and remove from allocated regions
    for (unsigned int i = 0; i < num_allocated; i++) {
        if (allocated_regions[i].start_addr == _start_address) {
            free_regions[num_free++] = allocated_regions[i];                // Add to free regions
            allocated_regions[i] = allocated_regions[num_allocated - 1];    // Remove from allocated
            num_allocated--;

            // Free each page
            for (unsigned long addr = _start_address; addr < _start_address + allocated_regions[i].size; addr += PageTable::PAGE_SIZE) {
                page_table->free_page(addr);
            }
            break;
        }
    }
    Console::puts("Released region of memory.\n");
}

bool VMPool::is_legitimate(unsigned long _address) {
    Console::puts("Checking whether address is part of an allocated region.\n");
    if((_address == base_address)) {
        return true;
    }
    // Check if address is in any allocated region
    for (unsigned int i = 0; i < num_allocated; i++) {
        unsigned long start = allocated_regions[i].start_addr;
        unsigned long end = start + allocated_regions[i].size;
        if (_address >= start && _address <= end) {
            return true;
        }
    }
    return false;
}
