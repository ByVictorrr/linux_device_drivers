//
// Created by delaplai on 4/4/2024.
//

#include <linux/irqreturn.h>
#include <linux/ktime.h>
#include <linux/timecounter.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/delay.h>	/* udelay */
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include <asm/io.h>
#include <linux/ktime.h>
#include <linux/time64.h>
#define SHORT_NR_PORTS	8	/* use 8 ports by default */

/*
 * all of the parameters have no "short_" prefix, to save typing when
 * specifying them at load time
 */
static int short_major = 0;	/* dynamic by default */
module_param(short_major, int, 0);

static int use_mem = 0;	/* default is I/O-mapped */
module_param(use_mem, int, 0);

/* default is the first printer port on PC's. "short_base" is there too
   because it's what we want to use in the code */
static unsigned long base = 0x378;
unsigned long short_base = 0;
module_param(base, long, 0);

/* The interrupt line is undefined by default. "short_irq" is as above */
static int irq = -1;
volatile int short_irq = -1;
module_param(irq, int, 0);


static int probe = 0;	/* select at load time how to probe irq line */
module_param(probe, int, 0);

static int wq = 0;	/* select at load time whether a workqueue is used */
module_param(wq, int, 0);

static int tasklet = 0;	/* select whether a tasklet is used */
module_param(tasklet, int, 0);

static int share = 0;	/* select at load time whether install a shared irq */
module_param(share, int, 0);

MODULE_AUTHOR ("Victor Delaplaine");
MODULE_LICENSE("Dual BSD/GPL");

unsigned long short_buffer = 0;
unsigned long volatile short_head;
volatile unsigned long short_tail;
DECLARE_WAIT_QUEUE_HEAD(short_queue);

/* Set up our tasklet if we're doing that. */
void short_do_tasklet(unsigned long);

DECLARE_TASKLET(short_tasklet, short_do_tasklet, 0);

static inline void short_incr_bp(volatile unsigned long *index, int delta){
    unsigned long new = *index + delta;
    barrier();
    *index= (new >= (short_buffer + PAGE_SIZE)) ? short_buffer : new;
}

irqreturn_t short_interrupt(int irq, void *dev_id, struct pt_regs *regs){
    struct timespec64 ts;
    struct timeval tv;
    int written;

    ktime_get_real_ts64(&ts);
    written = sprintf((char *)short_head, "%o8u.%o6u\n",
                      (int)(ts.tv_sec % 100000000), (int)ts.tv_sec);
    BUG_ON(written != 16);
    short_incr_bp(&short_head, written);
    wake_up_interruptible(&short_queue);
    return IRQ_HANDLED;

}

ssize_t short_i_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos){

}