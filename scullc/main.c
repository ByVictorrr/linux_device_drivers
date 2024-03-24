//
// Created by delaplai on 3/18/2024.
//


#include <linux/module.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <asm-generic/fcntl.h>
#include <linux/semaphore.h>
// use look-aside cache - for allocation of like size objects
/* declare one cache pointer: use it for all devices */
static struct kmem_cache  *scullc_cache;
static int quantum = 4000;
static int qset = 100;

module_param(quantum, int, S_IRUGO);
module_param(qset, int, S_IRUGO);

struct scullc_dev{
    void **data;
    struct scullc_dev *next;
    int vmas;
    int quantum;
    int qset;
    size_t size;
    struct semaphore sem; /* mutual exclusion semaphore */
    struct cdev cdev; /* Char device structure */
};

/*
 * Follow the list
 */
struct scullc_dev *scullc_follow(struct scullc_dev *dev, int n)
{
    while (n--){
        if (!dev->next){
            dev->next = (struct scullc_dev *) kmalloc(sizeof(struct scullc_dev), GFP_KERNEL);
            memset(dev->next, 0, sizeof(struct scullc_dev));
        }
        dev = dev->next;
    }
    return dev;

}
/*
 * Open and close
 */

int scullc_open (struct inode *inode, struct file *filp)
{
    struct scullc_dev *dev; /* device information */

    /*  Find the device */
    dev = container_of(inode->i_cdev, struct scullc_dev, cdev);

    /* now trim to 0 the length of the device if open was write-only */
    if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible (&dev->sem))
            return -ERESTARTSYS;
        scullc_trim(dev); /* ignore errors */
        up (&dev->sem);
    }

    /* and use filp->private_data to point to the device data */
    filp->private_data = dev;

    return 0;          /* success */
}

int scullc_release (struct inode *inode, struct file *filp){return 0;}

ssize_t scullc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    struct scullc_dev *dev = filp->private_data, *dptr;
    int qset = dev->qset, quantum = dev->quantum;
    int itemsize = qset * quantum;
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    if (down_interruptible(&dev->sem)){
        return -ERESTARTSYS;
    }
    if (*f_pos > dev->size)
        goto out;

    if (*f_pos + count > dev -> size){
        count = dev->size - * f_pos;
    }
    // find the list items, qset index, & offset in the quantum
    item = (int) (((long) *f_pos) / itemsize);
    rest = (int) (((long) *f_pos) % itemsize);
    s_pos = rest / quantum, q_pos = rest % quantum;

    dptr = scullc_follow(dev, item);
    if (!dptr || !dptr->data || !dptr->data[s_pos])
        goto out;
    // read only up to the end of the quantum
    if (count > quantum - q_pos)
        count = (size_t) (quantum - q_pos);
    if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += (loff_t)count;
    retval = (ssize_t)count;
    out:
        up(&dev->sem);
        return retval;
}

ssize_t scullc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct scullc_dev *dev = filp->private_data, *dptr;
    int qset = dev->qset, quantum = dev->quantum;
    int itemsize = qset * quantum;
    int item, s_pos, q_pos, rest;
    ssize_t retval = -ENOMEM;


    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }

    // find the list items, qset index, & offset in the quantum
    item = (int) (((long) *f_pos) / itemsize);
    rest = (int) (((long) *f_pos) % itemsize);
    s_pos = rest / quantum, q_pos = rest % quantum;

    // follow the list up to the right position
    dptr = scullc_follow(dev, item);
    if (!dptr)
        goto out;
    if (!dptr->data){
        dptr->data = (void **) kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dptr->data)
            goto out;
        memset(dptr->data, 0, qset * sizeof(char *));
    }
    /* Allocate a quantum using the memory cache */
    if (!dptr->data[s_pos]){
        dptr->data[s_pos] = kmem_cache_alloc(scullc_cache, GFP_KERNEL);
        if (!dptr->data[s_pos])
            goto out;
        memset(dptr->data[s_pos], 0, quantum);
    }
    // write only up to the end of this quantum
    if (count > quantum - q_pos)
        count = (size_t) (quantum - q_pos);

    if (copy_to_user(dptr->data[s_pos] + q_pos, buf, count)){
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

    if (dev->size < *f_pos)
        dev->size = (unsigned long) *f_pos;
    out:
    up(&dev->sem);
    return retval;
}

struct file_operations scullc_fops = {
        .owner =     THIS_MODULE,
        //.llseek =    scullc_llseek,
        .read =	     scullc_read,
        .write =     scullc_write,
        //.unlocked_ioctl = scullc_ioctl,
        .open =	     scullc_open,
        .release =   scullc_release,
        //.aio_read =  scullc_aio_read,
        //.aio_write = scullc_aio_write,
};
int __init scullc_init(void){

}