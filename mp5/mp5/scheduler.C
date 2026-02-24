/*
 File: scheduler.C
 
 Author: Phani Jyothi Kurada
            Master of Computer Science
            Texas A&M University
  Implements the Scheduler class that manages thread scheduling, including ready and zombie queues.
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
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
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {
  ready_queue_head = nullptr;
  ready_queue_tail = nullptr;
  zombie_head = nullptr;
  zombie_tail = nullptr;
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
  if (Machine::interrupts_enabled())
    Machine::disable_interrupts();  // START critical section

  if (ready_queue_head == nullptr) {
    Console::puts("No more threads to run. System halting.\n");
    while (true);
    return;
  }

  // Next thread from the ready queue
  ReadyQueueNode * next_node = ready_queue_head;
  Thread * next_thread = next_node->thread;

  // Update the head of the queue
  ready_queue_head = next_node->next;
  if (ready_queue_head == nullptr) {
    // Queue became empty
    ready_queue_tail = nullptr;
  }

  delete next_node;

  Console::puts("Yielding to thread: ");
  Console::puti(next_thread->ThreadId());
  Console::puts("\n");

  if (!Machine::interrupts_enabled())
    Machine::enable_interrupts();   // END critical section

  // Dispatch to the next thread
  Thread::dispatch_to(next_thread);
}

void Scheduler::resume(Thread * _thread) {
  if (Machine::interrupts_enabled())
    Machine::disable_interrupts();  // START critical section

  // Do not reschedule terminated threads
  if (_thread->is_terminating()) {
    Console::puts("Not rescheduling terminating thread: ");
    Console::puti(_thread->ThreadId());
    Console::puts("\n");
    if (!Machine::interrupts_enabled()) 
      Machine::enable_interrupts();
    return;
  }

  Console::puts("Resuming thread: ");
  Console::puti(_thread->ThreadId());
  Console::puts("\n");

  ReadyQueueNode * new_node = new ReadyQueueNode;
  new_node->thread = _thread;
  new_node->next = nullptr;

  if (ready_queue_tail == nullptr) {
    // Queue is empty
    ready_queue_head = new_node;
    ready_queue_tail = new_node;
  } else {
    // Append to end
    ready_queue_tail->next = new_node;
    ready_queue_tail = new_node;
  }

  // Clean up zombies
  while (zombie_head != nullptr) {
    ZombieNode * dead = zombie_head;
    zombie_head = dead->next;

    Console::puts("Cleaning zombie thread: ");
    Console::puti(dead->thread->ThreadId());
    Console::puts("\n");

    delete[] (char *)dead->thread->Stack();
    delete dead->thread;
    delete dead;
  }
  zombie_tail = nullptr;
  
  if (!Machine::interrupts_enabled()) {
    Machine::enable_interrupts();  // END critical section
  }
}

void Scheduler::add(Thread * _thread) {
  Console::puts("Adding thread to ready queue: ");
  Console::puti(_thread->ThreadId());
  Console::puts("\n");

  ReadyQueueNode * new_node = new ReadyQueueNode;
  new_node->thread = _thread;
  new_node->next = nullptr;

  if (ready_queue_tail == nullptr) {
    // Queue is empty
    ready_queue_head = new_node;
    ready_queue_tail = new_node;
  } else {
    // Append to end
    ready_queue_tail->next = new_node;
    ready_queue_tail = new_node;
  }
}

void Scheduler::terminate(Thread * _thread) {
  if (Machine::interrupts_enabled())
    Machine::disable_interrupts();

  Console::puts("Scheduler is terminating thread: ");
  Console::puti(_thread->ThreadId());
  Console::puts("\n");

  _thread->terminating = true;      // Mark thread as terminating
  park_zombie(_thread);  // Defer cleanup to the next resume()

  if (!Machine::interrupts_enabled())
    Machine::enable_interrupts();
}

void Scheduler::park_zombie(Thread * _thread) {
  if (Machine::interrupts_enabled())
    Machine::disable_interrupts();

  ZombieNode * node = new ZombieNode();
  node->thread = _thread;
  node->next = nullptr;

  Console::puts("Thread parked in zombie queue: ");
  Console::puti(_thread->ThreadId());
  Console::puts("\n");

  if (zombie_tail == nullptr) {
      zombie_head = node;
      zombie_tail = node;
  } else {
      zombie_tail->next = node;
      zombie_tail = node;
  }

  if (!Machine::interrupts_enabled())
    Machine::enable_interrupts();
}
