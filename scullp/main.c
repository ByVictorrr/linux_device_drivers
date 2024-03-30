//
// Created by delaplai on 3/18/2024.
//


#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <asm-generic/fcntl.h>
#include <asm-generic/errno.h>
#include <asm-generic/ioctl.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <asm-generic/fcntl.h>
#include <linux/semaphore.h>
#include "scullp.h"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor Delaplaine");
MODULE_DESCRIPTION("Module that allows you to read/write as many processes as possible.");
MODULE_VERSION("1.00");
static int scullp_order = SCULLP_ORDER;
static int scullp_qset = SCULLP_QSET;
static int scullp_major = SCULLP_MAJOR;
static int scullp_minor = 0;
static int scullp_nr_devs = SCULLP_DEVS;



module_param(scullp_major, int, 0);
module_param(scullp_minor,int, 0);
module_param(scullp_nr_devs, int, 0);
module_param(scullp_order, int, 0);
module_param(scullp_qset, int, 0);

struct scullp_dev *scullp_devices; /* allocated in scullp_init */


/*
 * Follow the list
 */
struct scullp_dev *scullp_follow(struct scullp_dev *dev, int n)
{
    while (n--){
        if (!dev->next){
            dev->next = (struct scullp_dev *) kmalloc(sizeof(struct scullp_dev), GFP_KERNEL);
            memset(dev->next, 0, sizeof(struct scullp_dev));
        }
        dev = dev->next;
    }
    return dev;

}
int scullp_trim(struct scullp_dev *dev)
{
    // TODO: Need to add vms
    struct scullp_dev *next, *dptr;
    int qset = dev->qset, i;
    for (dptr = dev; dptr; dptr = next){
        if (dptr->data) {
            for (i = 0; i < qset; i++)
                if (dptr->data[i])
                    free_pages((unsigned long)(dptr->data[i]), dptr->order);
            kfree(dptr->data);
            dptr->data=NULL;
        }
        next = dptr->next;
        /* Dont free resources for the first dev */
        if (dptr != dev)
            kfree(dptr);
    }
    dev->size = 0;
    dev->order = scullp_order;
    dev->qset = scullp_qset;
    dev->next = NULL;
    return 0;
}

/*
 * Open and close
 */

int scullp_open (struct inode *inode, struct file *filp)
{
    struct scullp_dev *dev; /* device information */

    /*  Find the device */
    dev = container_of(inode->i_cdev, struct scullp_dev, cdev);

    /* now trim to 0 the length of the device if open was write-only */
    if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible (&dev->sem))
            return -ERESTARTSYS;
        scullp_trim(dev); /* ignore errors */
        up (&dev->sem);
    }

    /* and use filp->private_data to point to the device data */
    filp->private_data = dev;

    return 0;          /* success */
}

int scullp_release (struct inode *inode, struct file *filp){return 0;}

ssize_t scullp_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    struct scullp_dev *dev = filp->private_data, *dptr;
    int qset = dev->qset, quantum = (int) (PAGE_SIZE << dev->order);
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

    dptr = scullp_follow(dev, item);
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

ssize_t scullp_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct scullp_dev *dev = filp->private_data, *dptr;
    int qset = dev->qset, quantum = (int) (PAGE_SIZE << dev->order);
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
    dptr = scullp_follow(dev, item);
    if (!dptr)
        goto out;
    if (!dptr->data){
        dptr->data = (void **) kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dptr->data)
            goto out;
        memset(dptr->data, 0, qset * sizeof(char *));
    }
    /* Here's the allocation of a single quantum */
    if (!dptr->data[s_pos]){
        dptr->data[s_pos] = (void*) __get_free_pages(GFP_KERNEL, (unsigned int) dptr->order);
        if (!dptr->data[s_pos])
            goto out;
        memset(dptr->data[s_pos], 0, PAGE_SIZE << dptr->order);
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
static long scullp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    int err;
    long retval, tmp;

    /* validation_1: ensure the type && the command number meets our need*/
    if (_IOC_TYPE(cmd) != SCULLP_IOC_MAGIC || _IOC_NR(cmd) > SCULLP_IOC_MAXNR)
        return -ENOTTY;

    if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))
        err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;

    switch(cmd){
        case SCULLP_IOCRESET: // reset the device
            scullp_order = 0;
            scullp_qset = 0;
            break;
        case SCULLP_IOCSORDER: // Set: arg points to the value
            // TODO: dont i have to use access_ok ?
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __put_user(scullp_order, (int __user *)arg);
            break;
        case SCULLP_IOCSQSET: // Set: arg points to the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __put_user(scullp_qset, (int __user *)arg);
            break;
        case SCULLP_IOCTORDER: // Tell: arg is the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            scullp_order = (int) arg;
            break;
        case SCULLP_IOCTQSET: // Tell: arg is the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            scullp_qset = (int) arg;
            break;
        case SCULLP_IOCGQSET: // Get: arg is pointer to result
            retval = __put_user(scullp_qset, (int __user *)arg);
            break;
        case SCULLP_IOCGORDER: // Get: arg is pointer to result
            retval = __put_user(scullp_order, (int __user *)arg);
            break;
        case SCULLP_IOCQORDER: // Query: return it (it's positive)
            retval = scullp_order;
            break;
        case SCULLP_IOCQQSET: // Query: return it (it's positive)
            retval = scullp_qset;
            break;
        case SCULLP_IOCXORDER: // eXchange: use arg as pointer
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scullp_order;
            retval = __get_user(scullp_order, (int __user *)arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user *)arg);
            break;
        case SCULLP_IOCXQSET: // eXchange: use arg as pointer
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scullp_qset;
            retval = __get_user(scullp_qset, (int __user *)arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user *)arg);
            break;
        case SCULLP_IOCHORDER: // sHift: like Tell + Query
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scullp_order;
            scullp_order = (int) arg;
            retval = tmp;
        case SCULLP_IOCHQSET: // sHift: like Tell + Query
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scullp_qset;
            scullp_qset = (int) arg;
            retval = tmp;
        default:
            retval = -ENOTTY;
            break;
    }
    return retval;
};

static loff_t scullp_llseek(struct file *filp, loff_t off, int whence){
    struct scullp_dev *dev = filp->private_data;
    loff_t newpos;
    switch(whence){
        case SEEK_SET:
            newpos = off;
            break;
        case SEEK_CUR:
            newpos = filp->f_pos + off;
            break;
        case SEEK_END:
            newpos = (loff_t) (dev->size + off);
            break;
        default:
            return -EINVAL;
    }
    if (newpos < 0)
        return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}


struct file_operations scullp_fops = {
        .owner =     THIS_MODULE,
        .llseek =    scullp_llseek,
        .read =	     scullp_read,
        .write =     scullp_write,
        .unlocked_ioctl = scullp_ioctl,
        .open =	     scullp_open,
        .release =   scullp_release,
};

void scullp_cleanup(void) {

    /* Clean up each devices resource & del cdev */
    int i;
    for (i = 0; i < scullp_nr_devs; i++) {
        cdev_del(&scullp_devices[i].cdev);
        scullp_trim(scullp_devices+i);
    }
    kfree(scullp_devices);
    scullp_devices = NULL;

    unregister_chrdev_region((dev_t) MKDEV(scullp_major, 0), "scullp_devs");

}
int __init scullp_init(void){
    /* create a cache for our quanta */
    int result, i;
    dev_t dev;
    if (scullp_major){
        dev = (dev_t) MKDEV(scullp_major, 0);
        result = register_chrdev_region(dev, (unsigned int) scullp_nr_devs, "scullp");
    }else{
        result = alloc_chrdev_region(&dev, 0, (unsigned int) scullp_nr_devs, "scullp");
        scullp_major = MAJOR(dev);
    }
    if (result < 0)
        return result;
    /**
     * use the global variables to init stuff
     */
     scullp_devices = (struct scullp_dev *) kmalloc(scullp_nr_devs * sizeof(struct scullp_dev), GFP_KERNEL);
    if (!scullp_devices) {
        result = -ENOMEM;
        goto fail_malloc;
    }

    memset(scullp_devices, 0, sizeof(struct scullp_dev) *scullp_nr_devs);
    for (i=0; scullp_nr_devs; i++){
        scullp_devices[i].order = scullp_order;
        scullp_devices[i].qset = scullp_qset;
        scullp_devices[i].size = scullp_qset * PAGE_OFFSET << scullp_order;
        sema_init(&scullp_devices[i].sem, 1);
        cdev_init(&scullp_devices[i].cdev, &scullp_fops);
        dev = (dev_t) MKDEV(scullp_major, scullp_minor + i);
        if (cdev_add(&scullp_devices[i].cdev, dev, 1))
            printk(KERN_NOTICE "Error adding scullp %d", i);
    }

    return 0;


    fail_malloc:
        unregister_chrdev_region(dev, (unsigned int) scullp_nr_devs);
        return result;

}

module_init(scullp_init);
module_exit(scullp_cleanup);
