/*
     File        : nonblocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */

/*--------------------------------------------------------------------------*/

    /* -- (none) -- */
    

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "nonblocking_disk.H"
#include "scheduler.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
concurrencyControl* concur_control = nullptr;
extern Scheduler * SYSTEM_SCHEDULER;
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(unsigned int _size) 
  : SimpleDisk(_size), InterruptHandler() {
    concur_control = new concurrencyControl();
    #ifdef _HANDLE_INTERRUPTS_
      InterruptHandler::register_handler(14, this);
    #endif
    Console::puts("[Disk] NonBlockingDisk constructed and interrupt handler registered (IRQ 14).\n");
}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::read(unsigned long _block_no, unsigned char * _buf) {
  Console::puts("[Disk] Read requested on block: ");
  Console::puti(_block_no);
  Console::puts("\n");

  #ifdef _THREAD_SYNCHRONIZATION_
    Console::puts("[Disk] Acquiring lock for read...\n");
    concur_control->acquireLock();                   
  #endif

  SimpleDisk::read(_block_no, _buf);
  wait_until_ready();

  #ifdef _THREAD_SYNCHRONIZATION_
    Console::puts("[Disk] Releasing lock after read.\n");
    concur_control->releaseLock();
  #endif

  Console::puts("[Disk] Read completed for block: ");
  Console::puti(_block_no);
  Console::puts("\n");
}

void NonBlockingDisk::write(unsigned long _block_no, unsigned char * _buf) {
  Console::puts("[Disk] Write requested on block: ");
  Console::puti(_block_no);
  Console::puts("\n");

  #ifdef _THREAD_SYNCHRONIZATION_
    Console::puts("[Disk] Acquiring lock for write...\n");
    concur_control->acquireLock();                  
  #endif

  SimpleDisk::write(_block_no, _buf);
  wait_until_ready();

  #ifdef _THREAD_SYNCHRONIZATION_
    Console::puts("[Disk] Releasing lock after write.\n");
    concur_control->releaseLock();
  #endif

  Console::puts("[Disk] Write completed for block: ");
  Console::puti(_block_no);
  Console::puts("\n");
}

bool NonBlockingDisk::block_ready()
{
  if(Machine::inportb(0x1F7) & 0x08 ==0 )
  {
    return false;
  }
  else
    return true;
}

void NonBlockingDisk::wait_until_ready() {
  Console::puts("[Disk] Waiting for disk to become ready...\n");

  while (!block_ready()) {
    #ifdef _HANDLE_INTERRUPTS_
      Console::puts("[Disk] Enqueuing thread and yielding (interrupt mode)...\n");
      if (Machine::interrupts_enabled())
        Machine::disable_interrupts();
      thread_queue.enqueue(Thread::CurrentThread());
      if (!Machine::interrupts_enabled())
        Machine::enable_interrupts();
    #else
      Console::puts("[Disk] Disk not ready — yielding (polling mode)...\n");
      SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
    #endif

    SYSTEM_SCHEDULER->yield();
  }
  Console::puts("[Disk] Disk is ready.\n");
}

#ifdef _HANDLE_INTERRUPTS_
  void NonBlockingDisk::handle_interrupt(REGS *_r) {
    Console::puts("[Interrupt] Disk interrupt received. Handling disk interrupt...\n");
    Thread* t = thread_queue.dequeue();
    if (t) {
      Console::puts("[Interrupt] Resuming thread with ID: ");
      Console::puti(t->ThreadId());
      Console::puts("\n");
      SYSTEM_SCHEDULER->resume(t);
    }
  }
#endif


