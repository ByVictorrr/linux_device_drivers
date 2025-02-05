
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm-generic/fcntl.h>
#include <linux/slab.h>
#include <asm-generic/errno.h>
#include <asm-generic/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "scull.h"



/* Parameters */

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;	/* number of bare scull devices */
int scull_quantum = SCULL_QUANTUM;
int scull_qset =  SCULL_QSET;
int scull_p_buffer = SCULL_P_BUFFER;	/* buffer size */

/**
 * scull_trim - cleans up the memory space for a fresh write
 * @dev:  a scull_device
 *
 * Detailed description of what the function does.
 *
 */
int scull_trim(struct scull_dev *dev){
    struct scull_qset * next, *dptr;
    int qset = dev->qset, quantum = dev->quantum, i;
    for (dptr = dev->data; dptr; dptr=next){
        if (dptr->data){
            for (i=0; i < qset; i++)
                if (dptr->data[i])
                    kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }
    dev->size = 0;
    dev->quantum = quantum;
    dev->qset = qset;
    dev->data = NULL;
    return 0;
}


struct scull_qset * scull_follow(struct scull_dev *dev, int item) {
    struct scull_qset *dptr;
    //allocate the first q_set if not in
    if (!dev->data) {
        dev->data = (struct scull_qset *) kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (!dev->data)
            return NULL;
        memset(dev->data, 0, sizeof(struct scull_qset));
    }
    dptr = dev->data;
    while (item--) {
        if (!dptr->next) {
            dptr->next = (struct scull_qset *) kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (!dptr->next)
                return NULL;
            memset(dptr->next, 0, sizeof(struct scull_qset));
        }
        dptr = dptr->next;
    }
    return dptr;
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    struct scull_dev *dev = filp->private_data;
    struct scull_qset * dptr;
    int qset = dev->qset, quantum = dev->quantum;
    int itemsize = qset * quantum;
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    if (down_interruptible(&dev->sem)){
        return -ERESTARTSYS;
    }
    if (*f_pos >= dev->size)
        goto out;

    if (*f_pos + count > dev -> size){
       count = dev->size - * f_pos;
    }
    // find the list items, qset index, & offset in the quantum
    item = (int) (((long) *f_pos) / itemsize);
    rest = (int) (((long) *f_pos) % itemsize);
    s_pos = rest / quantum, q_pos = rest % quantum;

    dptr = scull_follow(dev, item);
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



ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
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
    dptr = scull_follow(dev, item);
    if (!dptr)
        goto out;
    if (!dptr->data){
        dptr->data = (void **) kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dptr->data)
            goto out;
        memset(dptr->data, 0, qset * sizeof(char *));
    }
    if (!dptr->data[s_pos]){
        dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
        if (!dptr->data[s_pos])
            goto out;
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
long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    int err;
    long retval, tmp;

    /* validation_1: ensure the type && the command number meets our need*/
    if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC || _IOC_NR(cmd) > SCULL_IOC_MAXNR)
        return -ENOTTY;

    if (_IOC_DIR(cmd) & (_IOC_READ | _IOC_WRITE))
        err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
        if (err)
            return -EFAULT;

    switch(cmd){
        case SCULL_IOCRESET: // reset the device
            scull_quantum = 0;
            scull_qset = 0;
            break;
        case SCULL_IOCSQUANTUM: // Set: arg points to the value
            // TODO: dont i have to use access_ok ?
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __put_user(scull_quantum, (int __user *)arg);
            break;
        case SCULL_IOCSQSET: // Set: arg points to the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __put_user(scull_qset, (int __user *)arg);
            break;
        case SCULL_IOCTQUANTUM: // Tell: arg is the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            scull_quantum = (int) arg;
            break;
        case SCULL_IOCTQSET: // Tell: arg is the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            scull_qset = (int) arg;
            break;
        case SCULL_IOCGQSET: // Get: arg is pointer to result
            retval = __put_user(scull_qset, (int __user *)arg);
            break;
        case SCULL_IOCGQUANTUM: // Get: arg is pointer to result
            retval = __put_user(scull_quantum, (int __user *)arg);
            break;
        case SCULL_IOCQQUANTUM: // Query: return it (it's positive)
            retval = scull_quantum;
            break;
        case SCULL_IOCQQSET: // Query: return it (it's positive)
            retval = scull_qset;
            break;
        case SCULL_IOCXQUANTUM: // eXchange: use arg as pointer
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_quantum;
            retval = __get_user(scull_quantum, (int __user *)arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user *)arg);
            break;
        case SCULL_IOCXQSET: // eXchange: use arg as pointer
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_qset;
            retval = __get_user(scull_qset, (int __user *)arg);
            if (retval == 0)
                retval = __put_user(tmp, (int __user *)arg);
            break;
        case SCULL_IOCHQUANTUM: // sHift: like Tell + Query
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_quantum;
            scull_quantum = (int) arg;
            retval = tmp;
        case SCULL_IOCHQSET: // sHift: like Tell + Query
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_qset;
            scull_qset = (int) arg;
            retval = tmp;
        case SCULL_P_IOCTSIZE:
            scull_p_buffer = (int) arg;
            break;
        case SCULL_P_IOCQSIZE:
            return scull_p_buffer;
        default:
            retval = -ENOTTY;
            break;
    }
    return retval;
};

loff_t scull_llseek(struct file *filp, loff_t off, int whence){
    /*
     * Same function as $ROOT/scull/scull.c:scull_llseek
     */
    struct scull_dev *dev = filp->private_data;
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




