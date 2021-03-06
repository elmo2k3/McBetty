/*
    kernel.h
    Copyright (C) 2008  

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef KERNEL_H
#define KERNEL_H

#include "pt.h"

#ifdef TRACE
extern volatile unsigned char trace_flags;
#endif

/* Some definitions to compute ticks from seconds and 1/10 seconds */

/* Interval in microseconds, after which timer0 shall generate an interrupt
  Here: 10 ms
*/
#define T0PERIOD 10000

#define TICKS_PER_SEC	(1000000 / T0PERIOD)
#define TICKS_PER_TENTH_SEC (100000 / T0PERIOD)
#define TICKS_PER_HUNDR_SEC	(10000 / T0PERIOD)

/*
// TODO is this necessary here ?
inline void signal_set_irq(int taskid);
;inline void signal_clr_irq(int taskid);
*/

/* Possible states a task can be in: */
enum task_state {RUN, WAIT, FINISHED};

struct task {
  struct pt thread_pt;
  char (*thread) (struct pt *pt);
  enum task_state state;
};


/* Here we define the used signals */
#define SIG_DEBUG	0
#define SIG_BACKLIGHT	1
#define SIG_RTC		4
#define SIG_NEW_TIME	3
#define SIG_KEY_CHG	6
#define SIG_NEW_PACKET	7
#define SIG_TX		25
#define SIG_RX_PACKET	26
#define SIG_RTC_UNLOCK	27
#define SIG_KEYSCAN	28
#define SIG_RTC_INT	29
#define SIG_TIMER	30
#define SIG_NONE	31

/* TODO we should not need to export this variable. It is kernel only. */
//extern volatile unsigned int signals;

/* TODO we should not need to export this function. It is kernel only. */
/* Returns <> 0 iff the given signal is set */
//#define signal_is_set(sigid) ( signals & (1<<(sigid)) )

void signal_set(int sigid);
void signal_clr(int sigid);

void delay(unsigned int n);
void delay_ms(unsigned int n);

unsigned int system_time();

void  timerIRQ (void);
void kernel_init();

struct task *task_add(char (*func) (struct pt *pt));
void task_del(struct task *t);

//void check_events();
void schedule();

//extern int check_rx();
//extern void process_rx();

#endif
