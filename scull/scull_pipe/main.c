#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/poll.h>
#include <linux/cdev.h>
#include "../scull.h"

MODULE_LICENSE("HPE");
MODULE_AUTHOR("Victor Delaplaine");
MODULE_DESCRIPTION("Module that allows you to read/write as many processes as possible.");
MODULE_VERSION("1.00");

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

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;	/* number of bare scull devices */
int scull_quantum = SCULL_QUANTUM;
int scull_qset =  SCULL_QSET;
int scull_p_buffer = SCULL_P_BUFFER;	/* buffer size */


module_param(scull_major, int, S_IRUGO);
module_param(scull_minor,int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);
module_param(scull_p_buffer, int, 0);

static struct scull_pipe *scull_devices;


static int scull_p_open(struct inode *inode, struct file *filp){
    struct scull_pipe *dev;
    // inode will be the same for every process opening the file, but filp will be differnt
    // same dev will be from each be 1-1 with /dev/file
    dev = container_of(inode->i_cdev, struct scull_pipe, cdev);
    filp->private_data = dev;
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    if (!dev->buffer){
        // allocate the buffer
        dev->buffer = (char *) kmalloc(scull_p_buffer, GFP_KERNEL);
        if (!dev->buffer) {
            up(&dev->sem);
            return -ENOMEM;
        }
    }
    dev->buffersize = scull_p_buffer;
    dev->end = dev->buffer + dev->buffersize;
    dev->rp = dev->wp = dev->buffer;  /* buffer is empty condition */
    /* use f_mode to keep track of readers & writers */
    if (filp->f_mode & FMODE_READ)
        dev->nreaders++;
    if (filp->f_mode & FMODE_WRITE)
        dev->nwriters++;
    up(&dev->sem);
    return nonseekable_open(inode, filp);
}
static int scull_p_release(struct inode *inode, struct file *filp){
    struct scull_pipe *dev = filp -> private_data;
    /* remove this filp from the asynchronously notified file's */
    scull_p_fasync(-1, filp, 0);
    down(&dev->sem);
    if (filp->f_mode & FMODE_READ)
        dev->nreaders--;
    if (filp->f_mode & FMODE_WRITE)
        dev->nwriters--;
    if (dev->nreaders + dev->nwriters == 0){
        kfree(dev->buffer);
        dev->buffer = NULL;
    }
    up(&dev->sem);
    return 0;
}


static ssize_t scull_p_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    struct scull_pipe *dev = filp->private_data;
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    /* wait until writer writes something */
    while(dev->rp == dev->wp) { // nothing to read
        up(&dev->sem); // release the lock
        /* For processes that cannot block return error if data not available */
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        PDEBUG("%s reading: going to sleep\n", current->comm);
        /* use the inq wait queue to wait on a condition or wake up */
        if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
            return -ERESTARTSYS;
        /* after all the processes have been awakened by the wait_queue */
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }
    /* Get the amount of indexes to read */
    if (dev->wp > dev->rp)
        count = min(count, dev->wp - dev->rp);
    else
        count = min(count, dev->end - dev->rp);

    if (copy_to_user(buf, dev->rp, count)) {
        up(&dev->sem);
        return -EFAULT;
    }
    dev->rp+=count;
    /* Reset reader pointer if its at the last index of the buffer */
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

    while(spacefree(dev) == 0){ /* full */
        /* defining the wait task */
        DEFINE_WAIT(wait);

        up(&dev->sem);
        /* For non-block tell the user-space access again */
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        /* set the wait scheduler flag to TASK_INTERRUPTABLE & add to the wait queue */
        prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
        if (spacefree(dev) == 0)
            /* Yield the current thread to the processor */
            schedule();
        /* Remove the current task from the wait queu and set back to TASK_RUNNING */
        finish_wait(&dev->outq, &wait);
        /* ensure that we are not woken up by another signal */
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
    PDEBUG("Going to accept %li bytes to %p from %p\n", (long)count, dev->wp, buf);
    if (copy_from_user(dev->wp, buf, count)) {
        up(&dev->sem);
        return -EFAULT;
    }
    dev->wp += count;
    if (dev->wp == dev->end)
        dev->wp = dev->buffer; // wrapped
    up(&dev->sem);

    wake_up_interruptible(&dev->inq); // blocked in read() and select()
    if (dev->async_queue)
        /* and signal asynchronous readers if there are any registered with fnctl(F_ASYNC) */
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
    PDEBUG("%s did write %li bytes\n",current->comm, (long)count);
    return count;

}

static unsigned int scull_p_poll(struct file *filp, poll_table *wait){

    struct scull_pipe *dev = filp->private_data;
    unsigned int mask = 0;
    /*
     * The buffer is circular;
     * if wp == rp + 1: full
     * if wp == rp: empty
     */
    down(&dev->sem);
    /* poll_wait does not put the process to sleep; it only registers the process on the wait queue */
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
    /* registers or de-registers a process from async notifications
    called when user app sets F_ASYNC flag using fcntl */
    struct scull_pipe *dev = filp->private_data;
    return fasync_helper(fd, filp, mode, &dev->async_queue);
}

struct file_operations scull_p_fops = {
        .owner = THIS_MODULE,
        .llseek = no_llseek,
        .read = scull_p_read,
        .write = scull_p_write,
        .poll = scull_p_poll,
        .unlocked_ioctl = scull_ioctl,
        .open = scull_p_open,
        .release = scull_p_release,
        .fasync = scull_p_fasync,
};

void scull_p_cleanup(void)
{
    int i;
    dev_t devno = (dev_t) MKDEV(scull_major, scull_minor);
    if (!scull_devices)
        return; /* nothing else to release */

    for (i = 0; i < scull_nr_devs; i++) {
        cdev_del(&scull_devices[i].cdev);
        kfree(scull_devices[i].buffer);
    }
    kfree(scull_devices);
    unregister_chrdev_region(devno, (unsigned int) scull_nr_devs);
    scull_devices = NULL; /* pedantic */
}

int __init scull_p_init(void){

    int i, err, result;
    if (scull_major) {
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(&dev, (unsigned int) scull_nr_devs, "scull_pipe");
    } else {
        result = alloc_chrdev_region(&dev, (unsigned int) scull_minor, (unsigned int) scull_nr_devs, "scull_pipe");
        scull_major = MAJOR(dev);
    }
    if (result < 0) {
        printk(KERN_WARNING "scull_pipe: can't get major %d\n", scull_major);
        return result;
    }
    scull_devices = (struct scull_pipe *) kmalloc(scull_nr_devs * sizeof(struct scull_pipe), GFP_KERNEL);
    if (!scull_devices) {
        result = -ENOMEM;
        goto fail;  /* Make this more graceful */
    }
    memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_pipe));
    /* Initialize each device. */

    for (i = 0; i < scull_nr_devs; i++){
        struct scull_pipe * dev = &scull_devices[i];
        init_waitqueue_head(&dev->inq);
        init_waitqueue_head(&dev->outq);
        mutex_init(&dev->sem);
        // init the cdev
        cdev_init(&dev->cdev, &scull_p_fops);
        dev->cdev.owner = THIS_MODULE;
        // gives access to container_of to dev
        err = cdev_add(&dev->cdev, i, 1);
        if(err)
            printk(KERN_NOTICE "Error %d adding scullpipe%d", err, i);
    }
    return 0;
    fail:
        scull_p_cleanup();
        return result;

}




module_init(scull_p_init);
module_exit(scull_p_cleanup);