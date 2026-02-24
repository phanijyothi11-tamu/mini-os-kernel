/*
    File: kernel.C

    Author: R. Bettati / Modified by Phani Jyothi Kurada
            Department of Computer Science
            Texas A&M University
    Date  : 24/10/18
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define _USES_SCHEDULER_
//#define _TERMINATING_FUNCTIONS_
//#define _USE_ROUND_ROBIN_TIMER_     // Enable RR for Option 2
#define _USE_PROCESSES_           // ← Uncomment ONLY for Option 3 (Processes)

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "machine.H"
#include "machine_low.H"
#include "console.H"
#include "gdt.H"
#include "idt.H"
#include "irq.H"
#include "exceptions.H"
#include "interrupts.H"

#include "simple_timer.H"
#include "eoq_timer.H"

#include "frame_pool.H"
#include "mem_pool.H"

#ifdef _USE_PROCESSES_
#include "page_table.H"
#endif

#include "thread.H"

#ifdef _USES_SCHEDULER_
#include "scheduler.H"
#endif

/*--------------------------------------------------------------------------*/
/* MEMORY MANAGEMENT */
/*--------------------------------------------------------------------------*/
//
FramePool * SYSTEM_FRAME_POOL;
MemPool * MEMORY_POOL;
PageDirectory * KERNEL_PAGE_DIR = nullptr;
typedef unsigned int size_t;

void * operator new(size_t size) { return (void *)MEMORY_POOL->allocate((unsigned long)size); }
void * operator new[](size_t size) { return (void *)MEMORY_POOL->allocate((unsigned long)size); }
void operator delete(void * p, size_t s) { MEMORY_POOL->release((unsigned long)p); }
void operator delete[](void * p) { MEMORY_POOL->release((unsigned long)p); }

/*--------------------------------------------------------------------------*/
/* SCHEDULER AND HAND-OFF */
/*--------------------------------------------------------------------------*/

#ifdef _USES_SCHEDULER_
Scheduler * SYSTEM_SCHEDULER;
#endif

void pass_on_CPU(Thread * _to_thread) {
#ifndef _USES_SCHEDULER_
    Thread::dispatch_to(_to_thread);
#else
    SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
    SYSTEM_SCHEDULER->yield();
#endif
}

/*--------------------------------------------------------------------------*/
/* THREAD FUNCTIONS */
/*--------------------------------------------------------------------------*/

Thread * thread1;
Thread * thread2;
Thread * thread3;
Thread * thread4;

void fun1() {
    Console::puts("Thread: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
    Console::puts("FUN 1 INVOKED!\n");
#ifdef _TERMINATING_FUNCTIONS_
    for (int j = 0; j < 10; j++)
#else
    for (int j = 0;; j++)
#endif
    {
        Console::puts("FUN 1 IN BURST["); Console::puti(j); Console::puts("]\n");
        for (int i = 0; i < 10; i++) {
            Console::puts("FUN 1: TICK ["); Console::puti(i); Console::puts("]\n");
        }
        pass_on_CPU(thread2);
    }
    Console::puts("Thread 1 completed all bursts. Terminating.\n");
    SYSTEM_SCHEDULER->terminate(Thread::CurrentThread());
    SYSTEM_SCHEDULER->yield();
}

void fun2() {
    Console::puts("Thread: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
    Console::puts("FUN 2 INVOKED!\n");
#ifdef _TERMINATING_FUNCTIONS_
    for (int j = 0; j < 10; j++)
#else
    for (int j = 0;; j++)
#endif
    {
        Console::puts("FUN 2 IN BURST["); Console::puti(j); Console::puts("]\n");
        for (int i = 0; i < 10; i++) {
            Console::puts("FUN 2: TICK ["); Console::puti(i); Console::puts("]\n");
        }
        pass_on_CPU(thread3);
    }
    Console::puts("Thread 2 completed all bursts. Terminating.\n");
    SYSTEM_SCHEDULER->terminate(Thread::CurrentThread());
    SYSTEM_SCHEDULER->yield();
}

void fun3() {
    Console::puts("Thread: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
    Console::puts("FUN 3 INVOKED!\n");
#ifdef _TERMINATING_FUNCTIONS_
    for (int j = 0; j < 10; j++)
#else
    for (int j = 0;; j++)
#endif
    {
        Console::puts("FUN 3 IN BURST["); Console::puti(j); Console::puts("]\n");
        for (int i = 0; i < 10; i++) {
            Console::puts("FUN 3: TICK ["); Console::puti(i); Console::puts("]\n");
        }
        pass_on_CPU(thread4);
    }
    Console::puts("Thread 3 completed all bursts. Terminating.\n");
    SYSTEM_SCHEDULER->terminate(Thread::CurrentThread());
    SYSTEM_SCHEDULER->yield();
}

void fun4() {
    Console::puts("Thread: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
    Console::puts("FUN 4 INVOKED!\n");
#ifdef _TERMINATING_FUNCTIONS_
    for (int j = 0; j < 10; j++)
#else
    for (int j = 0;; j++)
#endif
    {
        Console::puts("FUN 4 IN BURST["); Console::puti(j); Console::puts("]\n");
        for (int i = 0; i < 10; i++) {
            Console::puts("FUN 4: TICK ["); Console::puti(i); Console::puts("]\n");
        }
        pass_on_CPU(thread1);
    }
    Console::puts("Thread 4 completed all bursts. Terminating.\n");
    SYSTEM_SCHEDULER->terminate(Thread::CurrentThread());
    SYSTEM_SCHEDULER->yield();
}

/*--------------------------------------------------------------------------*/
/* MAIN */
/*--------------------------------------------------------------------------*/

int main() {
    GDT::init();
    Console::init();
    IDT::init();
    ExceptionHandler::init_dispatcher();
    IRQ::init();
    InterruptHandler::init_dispatcher();

    Console::redirect_output(true);
    Console::puts("redirecting output!\n");

    /* Exception handlers */
    class DBZ_Handler : public ExceptionHandler {
      public:
        void handle_exception(REGS * _regs) override {
            Console::puts("DIVISION BY ZERO!\n");
            for (;;) ;
        }
    } dbz_handler;
    ExceptionHandler::register_handler(0, &dbz_handler);

    class PageFault_Handler : public ExceptionHandler {
      public:
        void handle_exception(REGS * _regs) override {
            Console::puts("PAGE FAULT! Addr="); Console::putui(get_CR2());
            Console::puts(" Err="); Console::putui(_regs->err_code);
            Console::puts(" EIP="); Console::putui(_regs->eip);
            Console::puts("\n");
            Thread * t = Thread::CurrentThread();
            if (t) {
                Console::puts("ThreadID="); Console::puti(t->ThreadId());
                Console::puts(" IsProcess="); Console::puti(t->is_process());
                Console::puts("\n");
            }
            for (;;) ;
        }
    } pagefault_handler;
    ExceptionHandler::register_handler(14, &pagefault_handler);

    /* Memory initialization */
    FramePool system_frame_pool;
    SYSTEM_FRAME_POOL = &system_frame_pool;
    MemPool memory_pool(SYSTEM_FRAME_POOL, 256);
    MEMORY_POOL = &memory_pool;

#ifdef _USE_PROCESSES_
    /* Paging setup (Option 3 only) */
    Console::puts("Initializing paging system...\n");
    PageDirectory * kernel_page_dir = new PageDirectory(SYSTEM_FRAME_POOL);
    kernel_page_dir->identity_map_kernel(0x00000000, 0x00400000);
    enable_paging(kernel_page_dir->get_physical_address());
    KERNEL_PAGE_DIR = kernel_page_dir;
    Console::puts("Paging enabled successfully!\n");
#endif

    /* Timer setup */
#ifdef _USE_ROUND_ROBIN_TIMER_
    EOQTimer timer(20);
    InterruptHandler::register_handler(0, &timer);
#else
    SimpleTimer timer(100);
    InterruptHandler::register_handler(0, &timer);
#endif

#ifdef _USES_SCHEDULER_
    SYSTEM_SCHEDULER = new Scheduler();
#endif

    Machine::enable_interrupts();
    Console::puts("Hello World!\n");

#ifdef _USE_PROCESSES_
    /* ---------- Option 3: Mixed threads and processes ---------- */
    Console::puts("Creating processes...\n");

    thread1 = new Thread(fun1, new char[1024], 1024, nullptr);

    PageDirectory * pd2 = new PageDirectory(SYSTEM_FRAME_POOL);
    pd2->copy_kernel_mappings(kernel_page_dir);
    unsigned long p2_stack_phys = SYSTEM_FRAME_POOL->get_frame();
    unsigned int  p2_stack_virt = 0x400000;
    pd2->map_page(p2_stack_virt, p2_stack_phys, true, false);
    kernel_page_dir->map_page(p2_stack_virt, p2_stack_phys, true, false);
    thread2 = new Thread(fun2, (char *)p2_stack_virt, 1024, pd2);
    kernel_page_dir->unmap_page(p2_stack_virt);

    PageDirectory * pd3 = new PageDirectory(SYSTEM_FRAME_POOL);
    pd3->copy_kernel_mappings(kernel_page_dir);
    unsigned long p3_stack_phys = SYSTEM_FRAME_POOL->get_frame();
    unsigned int  p3_stack_virt = 0x500000;
    pd3->map_page(p3_stack_virt, p3_stack_phys, true, false);
    kernel_page_dir->map_page(p3_stack_virt, p3_stack_phys, true, false);
    thread3 = new Thread(fun3, (char *)p3_stack_virt, 1024, pd3);
    kernel_page_dir->unmap_page(p3_stack_virt);

    thread4 = new Thread(fun4, new char[1024], 1024, nullptr);
#else
    /* ---------- Option 1–2: Normal threads ---------- */
    Console::puts("Creating normal threads...\n");
    thread1 = new Thread(fun1, new char[1024], 1024);
    thread2 = new Thread(fun2, new char[1024], 1024);
    thread3 = new Thread(fun3, new char[1024], 1024);
    thread4 = new Thread(fun4, new char[1024], 1024);
#endif

#ifdef _USES_SCHEDULER_
    SYSTEM_SCHEDULER->add(thread2);
    SYSTEM_SCHEDULER->add(thread3);
    SYSTEM_SCHEDULER->add(thread4);
#endif

    Console::puts("STARTING THREAD 1 ...\n");
    Thread::dispatch_to(thread1);

    assert(false);
    return 1;
}
