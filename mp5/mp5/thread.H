/*
    File: thread.H

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 11/10/25

    Description: Thread Management. 
    
                 Defines the Thread Control Block data structure, and 
                 functions to create threads and to dispatch the 
                 execution of threads.
                 
*/

#ifndef _thread_H_                   // include file only once
#define _thread_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "machine.H"

#include "page_table.H"
/* Forward declaration */
class PageDirectory;

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- THREAD FUNCTION (CALLED WHEN THREAD STARTS RUNNING) */
typedef void (*Thread_Function)();

/*--------------------------------------------------------------------------*/
/* THREAD CONTROL BLOCK */
/*--------------------------------------------------------------------------*/

class Thread {

private: 
    char     * esp;         /* The current stack pointer for the thread.*/
                            /* Keep it at offset 0, since the thread 
                               dispatcher relies on this  location! */
    int        thread_id;   /* thread identifier. Assigned upon creation. */
    char     * stack;       /* pointer to the stack of the thread.*/
    unsigned int stack_size;/* size of the stack (in byte) */
    int        priority;    /* Maybe the scheduler wants to use priorities. */
    char     * cargo;       /* pointer to additional data that 
                               may need to be stored, typically by schedulers.
                               (for future use) */
    PageDirectory * page_directory;  /* Page directory for this thread/process.
                                         NULL means thread shares kernel address space.
                                         Non-NULL means process has its own address space. */

    static int nextFreePid; /* Used to assign unique id's to threads. */

    void push(unsigned long _val);
    /* Push the given value on the stack of the thread. */

    void setup_context(Thread_Function _tfunction);
    /* Sets up the initial context for the given kernel-only thread. 
       The thread is supposed the call the function _tfunction upon start.
    */
 
public: 
    Thread(Thread_Function _tf, char * _stack, unsigned int _stack_size, 
           PageDirectory * _page_dir = nullptr);
    /* Create a thread that is set up to execute the given thread function. 
       The thread is given a pointer to the stack to use. 
       If _page_dir is provided, the thread is a process with its own address space.
       If _page_dir is nullptr, the thread shares the kernel address space.
       NOTE: _stack points to the beginning of the stack area, 
       i.e., to the bottom of the stack.
    */

   int ThreadId();
   /* Returns the thread id of the thread. */

   char * Stack() { 
      return stack; 
   }

   bool terminating = false;
   bool is_terminating() { 
      return terminating; 
   }

    static void dispatch_to(Thread * _thread);
    /* This is the low-level dispatch function that invokes the context switch
       code. This function is used by the scheduler.
       NOTE: dispatch_to does not return until the scheduler context-switches back
             to the calling thread.
    */

    static Thread * CurrentThread();
    /* Returns the currently running thread. nullptr if no thread has started 
       yet. */
    
    PageDirectory * get_page_directory() {
        return page_directory;
    }
    /* Returns the page directory for this thread/process. */
    
    bool is_process() {
        return (page_directory != nullptr);
    }
    /* Returns true if this is a process (has its own address space). */
    
    void set_esp(char * _esp) { esp = _esp; }  /* Set stack pointer (for process setup) */
};

#endif
