/*
    File: eoq_timer.C

    Author: PhaniJyothi Kurada
            Master of Computer Science
            Texas A&M University

    Implements the EOQTimer that enables round-robin preemption using timer interrupts.
*/

#include "console.H"
#include "machine.H"
#include "eoq_timer.H"
#include "interrupts.H"

extern Scheduler * SYSTEM_SCHEDULER;

EOQTimer::EOQTimer(int _hz) : SimpleTimer(_hz) {
}

void EOQTimer::handle_interrupt(REGS * _r) {
    // Acknowledge the interrupt to the PIC
    Machine::outportb(0x20, 0x20);  // Acknowledge IRQ 0 (timer)

    // Preempt the current thread (if one exists)
    Thread * self = Thread::CurrentThread();
    if (self != nullptr && !self->is_terminating()) {
        SYSTEM_SCHEDULER->resume(self);
    }
    // Only yield if we have a current thread, otherwise just return
    if (self != nullptr) {
        SYSTEM_SCHEDULER->yield();
    }
}
