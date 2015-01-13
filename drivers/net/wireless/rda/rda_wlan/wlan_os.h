#ifndef __WLAN_TIMER_H_
#define __WLAN_TIMER_H_

typedef struct __WLAN_DRV_TIMER
{
	struct timer_list tl;
	void (*timer_function) (void *context);
	void *function_context;
	u32 time_period;
	u8 timer_is_periodic;
	u8 timer_is_canceled;
	u8 EventType;
} WLAN_DRV_TIMER, *PWLAN_DRV_TIMER;

extern void wlan_timer_handler(unsigned long fcontext);
static inline void
wlan_initialize_timer(PWLAN_DRV_TIMER timer,
					  void (*TimerFunction) (void *context),
					  void *FunctionContext, u8 eventType)
{
	/* first, setup the timer to trigger the wlan_timer_handler proxy */
	init_timer(&timer->tl);
	timer->tl.function = wlan_timer_handler;
	timer->tl.data = (u32) timer;

	/* then tell the proxy which function to call and what to pass it */
	timer->timer_function = TimerFunction;
	timer->function_context = FunctionContext;
	timer->EventType = eventType;
	timer->timer_is_canceled = TRUE;
	timer->timer_is_periodic = FALSE;
}

static inline void
wlan_set_timer(PWLAN_DRV_TIMER timer, u32 MillisecondPeriod)
{
	timer->time_period = MillisecondPeriod;
	timer->timer_is_periodic = FALSE;
	timer->tl.expires = jiffies + (MillisecondPeriod * HZ) / 1000;
	add_timer(&timer->tl);
	timer->timer_is_canceled = FALSE;
}

static inline void
wlan_mod_timer(PWLAN_DRV_TIMER timer, u32 MillisecondPeriod)
{
	timer->time_period = MillisecondPeriod;
	timer->timer_is_periodic = FALSE;
	mod_timer(&timer->tl, jiffies + (MillisecondPeriod * HZ) / 1000);
	timer->timer_is_canceled = FALSE;
}

static inline void
wlan_set_periodic_timer(PWLAN_DRV_TIMER timer, u32 MillisecondPeriod)
{
	timer->time_period = MillisecondPeriod;
	timer->timer_is_periodic = TRUE;
	timer->tl.expires = jiffies + (MillisecondPeriod * HZ) / 1000;
	add_timer(&timer->tl);
	timer->timer_is_canceled = FALSE;
}

#define FreeTimer(x)	do {} while (0)

static inline void
wlan_cancel_timer(WLAN_DRV_TIMER * timer)
{
	if(!timer->timer_is_canceled){
		del_timer(&timer->tl);
		timer->timer_is_canceled = TRUE;
	}
}

static inline void wlan_sched_timeout(u32 millisec)
{ 
	unsigned long timeout = 0, expires = 0;
	expires = jiffies + msecs_to_jiffies(millisec);
	timeout = millisec;

	while(timeout)
	{
		timeout = schedule_timeout(timeout);
		
		if(time_after(jiffies, expires))
			break;
	}
}

/*
 * OS Thread Specific
 */

#include	<linux/kthread.h>

typedef struct
{
	struct task_struct *task;
	struct completion comp;
	pid_t pid;
	void *priv;
} wlan_thread;

static inline void
wlan_activate_thread(wlan_thread * thr)
{
		/** Record the thread pid */
	thr->pid = current->pid;
	init_completion(&thr->comp);
}

static inline void
wlan_deactivate_thread(wlan_thread * thr)
{
	ENTER();

	/* Reset the pid */
	thr->pid = 0;
	
	LEAVE();
}

static inline void
wlan_create_thread(int (*wlanfunc) (void *), wlan_thread * thr, char *name)
{
	thr->task = kthread_run(wlanfunc, thr, "%s", name);
}

static inline int
wlan_terminate_thread(wlan_thread * thr)
{
	ENTER();

	/* Check if the thread is active or not */
	if (!thr->pid) {
		printk(KERN_INFO "Thread does not exist\n");
		return -1;
	}
	kthread_stop(thr->task);

	LEAVE();
	return 0;
}

#endif

