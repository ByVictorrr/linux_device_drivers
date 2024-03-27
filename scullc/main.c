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
#include "scullc.h"
// use look-aside cache - for allocation of like size objects
/* declare one cache pointer: use it for all devices */
static struct kmem_cache  *scullc_cache;
static int scullc_quantum = SCULLC_QUANTUM;
static int scullc_qset = SCULLC_QSET;
static int scullc_major = SCULLC_MAJOR;
static int scullc_minor = 0;
static int scullc_nr_devs = SCULLC_DEVS;


module_param(scullc_quantum, int, S_IRUGO);
module_param(scullc_qset, int, S_IRUGO);

struct scullc_dev *scullc_devices; /* allocated in scullc_init */


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
int scullc_trim(struct scullc_dev *dev)
{
    // TODO: Need to add vms
    struct scullc_dev *next, *dptr;
    int qset = dev->qset, i;
    for (dptr = dev; dptr; dptr = next){
        if (dptr->data) {
            for (i = 0; i < qset; i++)
                if (dptr->data[i])
                    kmem_cache_free(scullc_cache, dptr->data[i]);
            kfree(dptr->data);
            dptr->data=NULL;
        }
        next = dptr->next;
        /* Dont free resources for the first dev */
        if (dptr != dev)
            kfree(dptr);
    }
    dev->size = 0;
    dev->quantum = scullc_quantum;
    dev->qset = scullc_qset;
    dev->next = NULL;
    return 0;
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
static long scullc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    int err;
    long retval, tmp;

    /* validation_1: ensure the type && the command number meets our need*/
    if (_IOC_TYPE(cmd) != SCULLC_IOC_MAGIC || _IOC_NR(cmd) > SCULLC_IOC_MAXNR)
        return -ENOTTY;

    if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))
        err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;

    switch(cmd){
        case SCULLC_IOCRESET: // reset the device
            scullc_quantum = 0;
            scullc_qset = 0;
            break;
        case SCULLC_IOCSQUANTUM: // Set: arg points to the value
            // TODO: dont i have to use access_ok ?
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __put_user(scullc_quantum, (int __user *)arg);
            break;
        case SCULLC_IOCSQSET: // Set: arg points to the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __put_user(scullc_qset, (int __user *)arg);
            break;
        case SCULLC_IOCTQUANTUM: // Tell: arg is the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            scullc_quantum = (int) arg;
            break;
        case SCULLC_IOCTQSET: // Tell: arg is the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            scullc_qset = (int) arg;
            break;
        case SCULLC_IOCGQSET: // Get: arg is pointer to result
            retval = __put_user(scullc_qset, (int __user *)arg);
            break;
        case SCULLC_IOCGQUANTUM: // Get: arg is pointer to result
            retval = __put_user(scullc_quantum, (int __user *)arg);
            break;
        case SCULLC_IOCQQUANTUM: // Query: return it (it's positive)
            retval = scullc_quantum;
            break;
        case SCULLC_IOCQQSET: // Query: return it (it's positive)
            retval = scullc_qset;
            break;
        case SCULLC_IOCXQUANTUM: // eXchange: use arg as pointer
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scullc_quantum;
            retval = __get_user(scullc_quantum, (int __user *)arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user *)arg);
            break;
        case SCULLC_IOCXQSET: // eXchange: use arg as pointer
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scullc_qset;
            retval = __get_user(scullc_qset, (int __user *)arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user *)arg);
            break;
        case SCULLC_IOCHQUANTUM: // sHift: like Tell + Query
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scullc_quantum;
            scullc_quantum = (int) arg;
            retval = tmp;
        case SCULLC_IOCHQSET: // sHift: like Tell + Query
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scullc_qset;
            scullc_qset = (int) arg;
            retval = tmp;
        default:
            retval = -ENOTTY;
            break;
    }
    return retval;
};

static loff_t scullc_llseek(struct file *filp, loff_t off, int whence){
    struct scullc_dev *dev = filp->private_data;
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


struct file_operations scullc_fops = {
        .owner =     THIS_MODULE,
        .llseek =    scullc_llseek,
        .read =	     scullc_read,
        .write =     scullc_write,
        .unlocked_ioctl = scullc_ioctl,
        .open =	     scullc_open,
        .release =   scullc_release,
        //.aio_read =  scullc_aio_read,
        //.aio_write = scullc_aio_write,
};

void scullc_cleanup(void) {

    /* Clean up each devices resource & del cdev */
    int i;
    for (i = 0; i < scullc_nr_devs; i++) {
        scullc_trim(scullc_devices + i);
        cdev_del(&scullc_devices[i].cdev);
    }
    if (scullc_cache){
        kmem_cache_destroy(scullc_cache);
        scullc_cache = NULL;
    }
    kfree(scullc_devices);
    scullc_devices = NULL;

    unregister_chrdev(scullc_major, "scullc");

}
int __init scullc_init(void){
    /* create a cache for our quanta */
    int result, i;
    dev_t dev;
    if (scullc_major){
        dev = (dev_t) MKDEV(scullc_major, 0);
        result = register_chrdev_region(dev, (unsigned int) scullc_nr_devs, "scullc");
    }else{
        result = alloc_chrdev_region(&dev, 0, (unsigned int) scullc_nr_devs, "scullc");
        scullc_major = MAJOR(dev);
    }
    if (result < 0)
        return result;
    /**
     * use the global variables to init stuff
     */
     scullc_devices = (struct scullc_dev *) kmalloc(scullc_nr_devs * sizeof(struct scullc_dev), GFP_KERNEL);
    if (!scullc_devices) {
        result = -ENOMEM;
        goto fail_malloc;
    }

    memset(scullc_devices, 0, sizeof(struct scullc_dev) *scullc_nr_devs);
    for (i=0; scullc_nr_devs; i++){
        scullc_devices[i].quantum = scullc_quantum;
        scullc_devices[i].qset = scullc_qset;
        scullc_devices[i].size = (unsigned long) (scullc_qset * scullc_quantum);
        sema_init(&scullc_devices[i].sem, 1);
        cdev_init(&scullc_devices[i].cdev, &scullc_fops);
        dev = (dev_t) MKDEV(scullc_major, scullc_minor + i);
        if (cdev_add(&scullc_devices[i].cdev, dev, 1))
            printk(KERN_NOTICE "Error adding scullc %d", i);
    }
    scullc_cache = kmem_cache_create("scullc", (unsigned int) scullc_quantum, 0, SLAB_HWCACHE_ALIGN, NULL);
    if (!scullc_cache){
        scullc_cleanup();
        return -ENOMEM;
    }

    return 0;


    fail_malloc:
        unregister_chrdev_region(dev, (unsigned int) scullc_nr_devs);
        return result;

}

module_init(scullc_init);
module_exit(scullc_cleanup);
