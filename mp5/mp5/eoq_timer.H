/* 
    Author: Phani Jyothi Kurada
            Master of Computer Science
            Texas A&M University
			
	    End-of-quantum functionality for the scheduler.

*/

#ifndef _EOQ_TIMER_H_
#define _EOQ_TIMER_H_

#include "simple_timer.H"
#include "scheduler.H"
#include "thread.H"

class EOQTimer : public SimpleTimer {
public:
    EOQTimer(int _hz);
    virtual void handle_interrupt(REGS *_r);
};

#endif