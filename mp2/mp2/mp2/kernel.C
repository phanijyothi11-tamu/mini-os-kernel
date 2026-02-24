/*
    File: kernel.C

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 2024/08/20

    This file has the main entry point to the operating system.

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define MB * (0x1 << 20)
#define KB * (0x1 << 10)
/* Makes things easy to read */

#define KERNEL_POOL_START_FRAME ((2 MB) / (4 KB))
#define KERNEL_POOL_SIZE ((2 MB) / (4 KB))
#define PROCESS_POOL_START_FRAME ((4 MB) / (4 KB))
#define PROCESS_POOL_SIZE ((28 MB) / (4 KB))
/* Definition of the kernel and process memory pools */

#define MEM_HOLE_START_FRAME ((15 MB) / (4 KB))
#define MEM_HOLE_SIZE ((1 MB) / (4 KB))
/* We have a 1 MB hole in physical memory starting at address 15 MB */

#define TEST_START_ADDR_PROC (4 MB)
#define TEST_START_ADDR_KERNEL (2 MB)
/* Used in the memory test below to generate sequences of memory references. */
/* One is for a sequence of memory references in the kernel space, and the   */
/* other for memory references in the process space. */

#define N_TEST_ALLOCATIONS 32
/* Number of recursive allocations that we use to test.  */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "machine.H"     /* LOW-LEVEL STUFF   */
#include "console.H"

#include "assert.H"
#include "cont_frame_pool.H"  /* The physical memory manager */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

void test_memory(ContFramePool * _pool, unsigned int _allocs_to_go);

/* Additional test functions can be declared here */
void test_allocation_release(ContFramePool* pool);
void test_mark_inaccessible(ContFramePool* pool);
void test_zero_allocation(ContFramePool* pool);
void test_max_allocation(ContFramePool* pool);
void test_needed_info_frames(ContFramePool* pool);

/*--------------------------------------------------------------------------*/
/* MAIN ENTRY INTO THE OS */
/*--------------------------------------------------------------------------*/

int main() {

    Console::init();
    Console::redirect_output(true); // comment if you want to stop redirecting qemu window output to stdout

    /* -- INITIALIZE FRAME POOLS -- */

    /* ---- KERNEL POOL -- */
    
    ContFramePool kernel_mem_pool(KERNEL_POOL_START_FRAME,
                                  KERNEL_POOL_SIZE,
                                  0);
    
    /* ---- PROCESS POOL -- */

/*  // In later machine problems, we will be using two pools. You may want to comment this out and test 
    // the management of two pools.

    unsigned long n_info_frames = ContFramePool::needed_info_frames(PROCESS_POOL_SIZE);

    unsigned long process_mem_pool_info_frame = kernel_mem_pool.get_frames(n_info_frames);
    
    ContFramePool process_mem_pool(PROCESS_POOL_START_FRAME,
                                   PROCESS_POOL_SIZE,
                                   process_mem_pool_info_frame);
    
    process_mem_pool.mark_inaccessible(MEM_HOLE_START_FRAME, MEM_HOLE_SIZE);
*/

    /* -- MOST OF WHAT WE NEED IS SETUP. THE KERNEL CAN START. */

    Console::puts("Hello World!\n");

    /* -- TEST MEMORY ALLOCATOR */
    
    test_memory(&kernel_mem_pool, N_TEST_ALLOCATIONS);

    /* ---- Additional test cases can be run here ---- */
/*
    test_allocation_release(&kernel_mem_pool);
    test_mark_inaccessible(&kernel_mem_pool);
    test_zero_allocation(&kernel_mem_pool);
    test_max_allocation(&kernel_mem_pool);
    test_needed_info_frames(&kernel_mem_pool);
*/

    /* -- NOW LOOP FOREVER */
    Console::puts("Testing is DONE. We will do nothing forever\n");
    Console::puts("Feel free to turn off the machine now.\n");

    for(;;);

    /* -- WE DO THE FOLLOWING TO KEEP THE COMPILER HAPPY. */
    return 1;
}

/*--------------------------------------------------------------------------*/
/* ORIGINAL MEMORY TEST FUNCTION */
/*--------------------------------------------------------------------------*/

void test_memory(ContFramePool * _pool, unsigned int _allocs_to_go) {
    Console::puts("alloc_to_go = "); Console::puti(_allocs_to_go); Console::puts("\n");
    if (_allocs_to_go > 0) {
        // We have not reached the end yet. 
        int n_frames = _allocs_to_go % 4 + 1;               // number of frames you want to allocate
        unsigned long frame = _pool->get_frames(n_frames);  // we allocate the frames from the pool
        int * value_array = (int*)(frame * (4 KB));         // we pick a unique number that we want to write into the memory we just allocated
        for (int i = 0; i < (1 KB) * n_frames; i++) {       // we write this value int the memory locations
            value_array[i] = _allocs_to_go;
        }
        test_memory(_pool, _allocs_to_go - 1);              // recursively allocate and uniquely mark more memory
        for (int i = 0; i < (1 KB) * n_frames; i++) {       // We check the values written into the memory before we recursed 
            if(value_array[i] != _allocs_to_go){            // If the value stored in the memory locations is not the same that we wrote a few lines above
                                                            // then somebody overwrote the memory.
                Console::puts("MEMORY TEST FAILED. ERROR IN FRAME POOL\n");
                Console::puts("i ="); Console::puti(i);
                Console::puts("   v = "); Console::puti(value_array[i]); 
                Console::puts("   n ="); Console::puti(_allocs_to_go);
                Console::puts("\n");
                for(;;);                                    // We throw a fit.
            }
        }
        ContFramePool::release_frames(frame);               // We free the memory that we allocated above.
    }
}

/*--------------------------------------------------------------------------*/
/* ADDITIONAL TEST FUNCTIONS */
/*--------------------------------------------------------------------------*/

void test_allocation_release(ContFramePool* pool) {
    Console::puts("\n--- Test: Allocation and Release ---\n");

    // Allocate 1 frame
    int f1 = pool->get_frames(1);
    assert(f1 != 0);
    Console::puts("Allocated 1 frame at "); Console::puti(f1); Console::puts("\n");
    ContFramePool::release_frames(f1);
    Console::puts("Released frame "); Console::puti(f1); Console::puts("\n");

    // Allocate multiple frames
    int f2 = pool->get_frames(5);
    assert(f2 != 0);
    Console::puts("Allocated 5 frames starting at "); Console::puti(f2); Console::puts("\n");
    ContFramePool::release_frames(f2);
    Console::puts("Released 5 frames starting at "); Console::puti(f2); Console::puts("\n");

    Console::puts("Allocation and release test DONE.\n");
}

void test_mark_inaccessible(ContFramePool* pool) {
    Console::puts("\n--- Test: Mark Inaccessible Frames ---\n");

    int start = KERNEL_POOL_START_FRAME + 10;
    int size = 5;
    pool->mark_inaccessible(start, size);

    int f = pool->get_frames(size);
    if (f >= start && f < start + size) {
        Console::puts("ERROR: Allocated frame in inaccessible region!\n");
        for(;;);
    } else {
        Console::puts("Successfully avoided inaccessible frames.\n");
        ContFramePool::release_frames(f);
    }

    Console::puts("Mark inaccessible test DONE.\n");
}

void test_zero_allocation(ContFramePool* pool) {
    Console::puts("\n--- Test: Zero Frame Allocation ---\n");
    int f = pool->get_frames(0);
    if (f == 0) {
        Console::puts("Zero frame allocation correctly returned 0.\n");
    } else {
        Console::puts("ERROR: Zero frame allocation returned non-zero.\n");
        for(;;);
    }
    Console::puts("Zero allocation test DONE.\n");
}

void test_max_allocation(ContFramePool* pool) {
    Console::puts("\n--- Test: Max Allocation ---\n");
    int f = pool->get_frames(KERNEL_POOL_SIZE);
    if (f != 0) {
        Console::puts("Allocated maximum frames starting at: "); Console::puti(f); Console::puts("\n");
        ContFramePool::release_frames(f);
    } else {
        Console::puts("Failed to allocate maximum frames (expected if not enough memory).\n");
    }
    Console::puts("Max allocation test DONE.\n");
}

void test_needed_info_frames(ContFramePool* pool) {
    Console::puts("\n--- Test: Needed Info Frames ---\n");
    unsigned long needed = ContFramePool::needed_info_frames(KERNEL_POOL_SIZE);
    Console::puts("Needed info frames for kernel pool: "); Console::puti(needed); Console::puts("\n");
    Console::puts("Needed info frames test DONE.\n");
}
