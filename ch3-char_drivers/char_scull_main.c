
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <asm-generic/fcntl.h>
#include <linux/slab.h>
#include <asm-generic/errno.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor Delaplaine");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("0.01");

int scull_major =   0;
unsigned int scull_minor =   0;
unsigned int scull_nr_devs = 4;	/* number of bare scull devices */
int scull_quantum = 400;
int scull_qset =  100;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor,int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);


struct scull_dev *scull_devices;	/* allocated in scull_init_module */
struct scull_qset {
    void **data;
    struct scull_qset *next;
};

struct scull_dev {
    struct scull_qset *data; /* Pointer to first quantum set */
    int quantum; /* the current quantum size */
    int qset; /* the current array size */
    unsigned long size; /* amount of data stored here */
    unsigned int access_key; /* used by sculluid and scullpriv */
    struct semaphore sem; /* mutual exclusion semaphore */
    struct cdev cdev; /* Char device structure */
};




static int scull_trim(struct scull_dev *dev){

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

static ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
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




static ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
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


struct file_operations scull_fops = {
        .owner=THIS_MODULE,
        .open=scull_open,
        .read=scull_read,
        .write=scull_write,
};




static void scull_char_exit(void){

    int i;
    for (i=0; scull_devices && i < scull_nr_devs; i++){
        scull_trim(scull_devices+i) ;
        cdev_del(&scull_devices[i].cdev);
    }
    kfree(scull_devices);

    unregister_chrdev((unsigned int) scull_major, "scull");
    printk(KERN_ALERT "Goodbye, cruel world.\n");
}


static int __init scull_char_init(void){
    int result, i;
    dev_t dev = 0;
    printk(KERN_WARNING "scull: in char init");
    // register the char device
    if (scull_major) {
        // Static - method
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev, scull_nr_devs, "scull");
    }else{
        result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
        scull_major = MAJOR(dev);
    }
    if (result < 0){
        printk(KERN_WARNING "scull: cant get major %d.\n", scull_major);
        return result;
    }

    // allocate the devices
    scull_devices = (struct scull_dev *) kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
    if (!scull_devices){
        result = -ENOMEM;
        goto fail;
    }
    memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));
    //Init for each device
    for (i=0; i < scull_nr_devs; i++){
        struct scull_dev *s_dev = scull_devices + i;
        s_dev->quantum = scull_quantum;
        s_dev->qset = scull_qset;
        sema_init(&s_dev->sem, 1);
        // init char driver
        cdev_init(&s_dev->cdev, &scull_fops);
        dev = MKDEV(scull_major, scull_minor + i);
        s_dev->cdev.owner = THIS_MODULE;
        s_dev->cdev.ops = &scull_fops;
        if (cdev_add (&s_dev->cdev, dev, 1)){
            printk(KERN_NOTICE "Error adding scull %d", i);
        }
        // At this point call the init function for any friend device
        //  dev = MKDEV(scull_major, scull_minor + scull_major);
    }
    // initialize pipe


    return 0;
    fail:
        scull_char_exit();
        return result;
}



module_init(scull_char_init);
module_exit(scull_char_exit);

