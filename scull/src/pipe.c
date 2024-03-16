#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>	/* printk(), min() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm-generic/fcntl.h>
#include <asm-generic/errno.h>
#include "../include/scull.h"

struct scull_pipe{
    wait_queue_head_t inq, outq;
    char *buffer, *end;
    int buffersize;
    char *rp, *wp;
    int nreaders, nwriters;
    struct fasync_struct *async_queue;
    struct semaphore sem;
    struct cdev cdev;

};
/* Parameters */
static int scull_p_nr_devs = 4;
static int scull_p_buffer = 4000;	/* buffer size */
module_param(scull_p_nr_devs, int, 0);	/* FIXME check perms */
module_param(scull_p_buffer, int, 0);
static struct scull_pipe *scull_p_devices;
dev_t scull_p_devno;			/* Our first device number */


static ssize_t scull_p_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    struct scull_pipe *dev = filp->private_data;
    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }

    while(dev->rp == dev->wp) { // nothing to read
        up(&dev->sem); // release the lock
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        printk(KERN_INFO "%s reading: going to sleep\n", current->comm);
        if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
            return -ERESTARTSYS;
        // otherwise loop, but first reacquire the lock
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }
    if (dev->wp > dev->rp)
        count = min(count, dev->wp - dev->rp);
    else
        count = min(count, dev->end - dev->rp);
    if (copy_to_user(buf, dev->rp, count)) {
        up(&dev->sem);
        return -EFAULT;
    }
    dev->rp+=count;
    if (dev->rp == dev->end)
        dev->rp = dev->buffer;

    /* finally, awake any writers and return */
    wake_up_interruptible(&dev->outq);
    return count;

}

static int spacefree(struct scull_pipe *dev){
    if (dev->rp == dev->wp)
        return dev->buffersize - 1;
    return (int) (((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1);
}
/* Wait for space for writing; caller must hold device semaphore. On
 * error the semaphore will be released before returning. */
static int scull_getwritespace(struct scull_pipe *dev, struct file *filp) {
    while(spacefree(dev) == 0){
        DEFINE_WAIT(wait);
        up(&dev->sem);
        // For non-block tell the user-space access again
        if (filp->f_flags & O_NONBLOCK){
            return -EAGAIN;
        }
        // add the current thread/process to the write wait queue
        prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
        if (spacefree(dev) == 0)
            schedule();
        // wake up other threads/processes
        finish_wait(&dev->outq, &wait);
        // ensure that we are not woken up by another signal
        if (signal_pending(current)){
            return -ERESTARTSYS;
        }
        if (down_interruptible(&dev->sem)){
            return -ERESTARTSYS;
        }
    }
    return 0;
}


static ssize_t scull_p_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct scull_pipe *dev = filp->private_data;
    int result;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    // Make sure there is no space to write
    result = scull_getwritespace(dev, filp);
    if (result)
        return result; /* scull_getwritespace called up(&dev->sem) */
    // ok, space is there, accept something
    count = min(count, (size_t) spacefree(dev));
    if (dev->wp >= dev->rp)
        count = min(count, dev->end - dev->wp); // to end-of-buff
    else
        count = min(count, dev->rp - dev->wp - 1);
    if (copy_from_user(dev->wp, buf, count)) {
        up(&dev->sem);
        return -EFAULT;
    }
    dev->wp += count;
    if (dev->wp == dev->end)
        dev->wp = dev->buffer; // wrapped
    up(&dev->sem);

    // finally, awake any reader
    wake_up_interruptible(&dev->inq); // blocked in read() and select()
    // and signal asynchronous readers if there are any registered with fnctl(F_ASYNC)
    if (dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
    return count;

}

static unsigned int scull_p_poll(struct file *filp, poll_table *wait){
    struct scull_pipe *dev = filp->private_data;
    unsigned int mask = 0;
    /*
     * The buffer is circular; it is considered full
     * if "wp" is right behind "rp" and empty if the two are equal
     */
    down(&dev->sem);
    poll_wait(filp, &dev->inq, wait);
    poll_wait(filp, &dev->outq, wait);
    if (dev->rp != dev->wp){
        mask |= POLLIN | POLLRDNORM;
    }
    if (spacefree(dev)){
        mask |= POLLOUT | POLLWRNORM;
    }
    up(&dev->sem);
    return mask;
}
static int scull_p_fasync(int fd, struct file *filp, int mode){
    // this method registers or de-registers a process from async notifications
    // called when user app sets F_ASYNC flag using fcntl
    struct scull_pipe *dev = filp->private_data;
    return fasync_helper(fd, filp, mode, &dev->async_queue);
}

struct file_operations scull_p_fops = {
        .owner = THIS_MODULE,
        .read = scull_p_read,
        .write = scull_p_write,
        .poll = scull_p_poll,
        .fasync = scull_p_fasync,
};


int scull_p_init(dev_t first_dev){
    int i, err;
    if (register_chrdev_region(&dev, (unsigned int) scull_p_nr_devs, "scullpipe") < 0){
        printk(KERN_WARNING "scull: cant get major %d.\n", scull_major);
        return 0;
    }
    scull_p_devno = first_dev;
    scull_p_devices = (struct scull_pipe *) kmalloc(scull_p_nr_devs * sizeof(struct scull_pipe), GFP_KERNEL);
    if (!scull_p_devices){
        unregister_chrdev_region(first_dev, (unsigned int) scull_p_nr_devs);
        return 0;
    }
    memset(scull_p_devices, 0, scull_p_nr_devs * sizeof(struct scull_pipe));
    for (i = 0; i < scull_p_nr_devs; i++){
        struct scull_pipe * dev = &scull_p_devices[i];
        init_waitqueue_head(&dev->inq);
        init_waitqueue_head(&dev->outq);
        mutex_init(&dev->sem);
        // init the cdev
        cdev_init(&dev->cdev, &scull_p_fops);
        dev->cdev.owner = THIS_MODULE;
        err = cdev_add(&dev->cdev, scull_p_devno + i, 1);
        if(err)
            printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);

    }

}


/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void scull_p_cleanup(void)
{
    int i;
    if (!scull_p_devices)
        return; /* nothing else to release */

    for (i = 0; i < scull_p_nr_devs; i++) {
        cdev_del(&scull_p_devices[i].cdev);
        kfree(scull_p_devices[i].buffer);
    }
    kfree(scull_p_devices);
    unregister_chrdev_region(scull_p_devno, scull_p_nr_devs);
    scull_p_devices = NULL; /* pedantic */
}