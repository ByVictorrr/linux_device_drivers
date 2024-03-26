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

int scull_major =   SCULL_MAJOR;
int scull_minor =   0;
int scull_nr_devs = SCULL_NR_DEVS;	/* number of bare scull devices */
int scull_quantum = 4000;
int scull_qset =  1000;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor,int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);
struct scull_dev *scull_devices;	/* allocated in scull_init_module */


static void scull_exit(void){

    int i;
    for (i=0; scull_devices && i < scull_nr_devs; i++){
        scull_trim(scull_devices+i) ;
        cdev_del(&scull_devices[i].cdev);
    }
    kfree(scull_devices);
    unregister_chrdev((unsigned int) scull_major, "scull");

    /* and call the cleanup functions for friend devices */
    scull_p_cleanup();
    scull_access_cleanup();
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
    // At this point call the init function for any friend device
    dev = (dev_t) MKDEV(scull_major, scull_minor + scull_major);
    dev+= scull_p_init(dev);
    dev+= scull_access_init(dev);

    return 0;

    fail:
        scull_exit();
        return result;
}



module_init(scull_init);
module_exit(scull_exit);

