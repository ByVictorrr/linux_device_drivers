#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "../scull.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor Delaplaine");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("0.01");

extern int scull_major;
extern int scull_minor;
extern int scull_nr_devs;
extern int scull_quantum;
extern int scull_qset;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor,int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);
struct scull_dev *scull_devices;	/* allocated in scull_init_module */




static int scull_open(struct inode *inode, struct file *filp) {
    struct scull_dev *dev;
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

struct file_operations scull_fops = {
        .owner=THIS_MODULE,
        .open=scull_open,
        .read=scull_read,
        .write=scull_write,
        .unlocked_ioctl=scull_ioctl,
        .llseek=scull_llseek,

};


static void scull_exit(void){

    int i;
    for (i=0; scull_devices && i < scull_nr_devs; i++){
        scull_trim(scull_devices+i) ;
        cdev_del(&scull_devices[i].cdev);
    }
    kfree(scull_devices);
    unregister_chrdev((unsigned int) scull_major, "scull");

}


static int __init scull_init(void){
    int result, i;
    dev_t dev = 0;
    // register the char device
    if (scull_major) {
        // Static - method
        dev = (dev_t) MKDEV(scull_major, scull_minor);
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
        dev = (dev_t) MKDEV(scull_major, scull_minor + i);
        s_dev->cdev.owner = THIS_MODULE;
        s_dev->cdev.ops = &scull_fops;
        if (cdev_add (&s_dev->cdev, dev, 1)){
            printk(KERN_NOTICE "Error adding scull %d", i);
        }

    }
    return 0;

    fail:
        scull_exit();
        return result;
}



module_init(scull_init);
module_exit(scull_exit);

