
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <asm-generic/fcntl.h>
#include <linux/slab.h>
#include <asm-generic/errno.h>
#include <asm-generic/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "../include/scull.h"




/**
 * scull_read_procmem - create a
 * @param buf
 * @param start
 * @param offset
 * @param count
 * @param eof
 * @param data
 * @return
 */
int scull_read_procmem(char *buf, char **start, off_t offset, int count, int *eof, void *data){

    int i, j, len = 0;
    int limit = count - 80; // Dont print more than this
    for (i=0; i < scull_nr_devs && len <= limit; i++){
        struct scull_dev *d = &scull_devices[i];
        struct scull_qset *qs = d->data;
        if (down_interruptible(&d->sem))
            return -ERESTARTSYS;
        len+= sprintf(buf+len, "\nDevice %i: qset %i, q %i, sz %li\n", i, d->qset, d->quantum, d->size);
        for (; qs && len <= limit; qs=qs->next){
            len+=sprintf(buf+len, " item at %p, qset at %p\n", qs, qs->data);
            if (qs->data && !qs->next) // dump only the last item
                for (j=0; j < d->qset; j++){
                    if (qs->data[j])
                        len+=sprintf(buf+len, " %4i: %8p\n", j, qs->data[j]);
                }
        }
        up(&scull_devices[i].sem);
    }
    *eof = 1; // tell the kernel no more data to return
    return len;
}
static void *scull_seq_start(struct seq_file *s, loff_t *pos){
    if (*pos >= scull_nr_devs)
        return NULL; // No more to read
    return scull_devices + *pos;
}
static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos){
    (*pos)++;
    if (*pos >= scull_nr_devs)
        return NULL; // no more devices
    return scull_devices + *pos;
}
static int scull_seq_show(struct seq_file *s, void *v){
    struct scull_dev *dev = (struct scull_dev *)v;
    struct scull_qset *d;
    int i;
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    seq_printf(s, "\nDevice %i: qset %i, q %i, sz %li\n",
               (int)(dev - scull_devices), dev->qset, dev->quantum, dev->size);
    for (d=dev->data; d; d = d->next) { // scan the list
        seq_printf(s, " item at %p, qset at %p\n", d, d->data);
        if (d->data && !d->next) // dump only the last item
            for (i = 0; i < dev->qset; i++) {
                if (d->data[i])
                    seq_printf(s, " %4i: %8p\n", i, d->data[i]);
            }
    }
    up(&dev->sem);
    return 0;
}
static void scull_seq_stop(struct seq_file *s, void *v){}
static struct seq_operations scull_seq_ops = {
        .start = scull_seq_start,
        .next = scull_seq_next,
        .stop = scull_seq_stop,
        .show = scull_seq_show
};




static int scull_proc_open(struct inode *inode, struct file *filp) {
    return seq_open(filp, &scull_seq_ops);
}

static struct proc_ops scull_proc_ops = {
        .proc_open    = scull_proc_open,
        .proc_read    = seq_read,
        .proc_lseek  = seq_lseek,
        .proc_release = seq_release
};
static void scull_create_proc(void)
{
    struct proc_dir_entry *entry = proc_create("sullseq", 0, NULL, &scull_proc_ops);
    if (!entry)
        printk(KERN_WARNING " Could not Create scullseq in procfs.\n");
}

static void scull_remove_proc(void)
{
    remove_proc_entry("sullseq", NULL);

}


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

static int scull_open(struct inode *inode, struct file *filp){
    struct scull_dev * dev;
    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;
    // Trim to 0 the length of the device if open was write only
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
        scull_trim(dev);
        up(&dev->sem);
    }
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
    long retval, tmp;
    switch(cmd){
        case SCULL_IOCRESET: // reset the device
            dev->quantum = 0;
            dev->qset = 0;
            break;
        case SCULL_IOCSQUANTUM: // Set: arg points to the value
            // TODO: dont i have to use access_ok ?
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __put_user(scull_quantum, (int __user *)arg):
            break;
        case SCULL_IOCSQSET: // Set: arg points to the value
            if (!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __put_user(scull_qset, (int __user *)arg):
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
        case SCULL_IOCHQUANTUM: // sHift: like Tell + Query
            if (! capable (CAP_SYS_ADMIN))
                return -EPERM;
            tmp = scull_qset;
            scull_qset = (int) arg;
            retval = tmp;
        default:
            retval = -ENOTTY;
            break;
    }
    return retval;
};

loff_t scull_llseek(struct file *filp, loff_t off, int whence){
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


struct file_operations scull_fops = {
        .owner=THIS_MODULE,
        .open=scull_open,
        .read=scull_read,
        .write=scull_write,
        .unlocked_ioctl=scull_ioctl,
        .llseek=scull_llseek,

};




