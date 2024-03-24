//
// Created by delaplai on 3/13/2024.
//

#include "../scull/scull.h"
#include <asm-generic/atomic.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/fcntl.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h>

#include <linux/sched.h>
#include <linux/spinlock.h>
static dev_t scull_a_firstdev;  /* Where our range begins */
static struct scull_dev scull_s_device;
static atomic_t scull_s_available = ATOMIC_INIT(1);

static int scull_s_open(struct inode *node, struct file *filp){
     struct scull_dev *dev = &scull_s_device;
     /* Determine if the device is opened by a process already */
     if (! atomic_dec_and_test(&scull_s_available)){
         atomic_inc(&scull_s_available);
         /* Tell other process's a process is using this */
         return -EBUSY;
     }
     /* Then, everything is copied from the bar scull device */
     if ( (filp->f_flags & O_ACCMODE) == O_WRONLY)
         scull_trim(dev);
     filp->private_data = dev;
     return 0;
}

static int scull_s_release(struct inode *inode, struct file *filp){
    /* Give access to other processes */
    atomic_inc(&scull_s_available);
    return 0;
}

// shared functions
static inline bool scull_uid_available(unsigned long count, uid_t scull_owner){
    return count == 0 ||
    scull_owner == current_cred()->uid ||  // allow user
    scull_owner == current_cred()->ueid ||  // allow whoever did su
    capable(CAP_DAC_OVERRIDE); // allow root to still open
}
struct file_operations scull_s_fops = {
        .owner =	THIS_MODULE,
        .llseek =     	scull_llseek,
        .read =       	scull_read,
        .write =      	scull_write,
        .unlocked_ioctl = scull_ioctl,
        .open =       	scull_s_open,
        .release =    	scull_s_release,
};
/******************SCULL SINGLE USER ID ACCESS Device**************************************/

static struct scull_dev scull_u_device;
static DEFINE_SPINLOCK(scull_u_lock);
static uid_t scull_u_owner;	// initialized to 0 by default
static unsigned long scull_u_count = 0;

static int scull_u_open(struct inode *node, struct file *filp){

    struct scull_dev *dev = &scull_u_device;
    spin_lock(&scull_u_lock);

    if (!scull_uid_available(scull_u_count, scull_u_owner)){ // allow root to still open (would not be the owner if existed)
        spin_unlock(&scull_u_lock);
        return -EBUSY; // only allow one user for many processes
    }
    if (scull_u_count == 0)
        scull_u_owner = current_cred()->uid;
    scull_u_count++;
    spin_unlock(&scull_u_lock);


    /* Then, everything is copied from the bar scull device */
    if ( (filp->f_flags & O_ACCMODE) == O_WRONLY)
        scull_trim(dev);
    filp->private_data = dev;
    return 0;
}

static int scull_u_release(struct inode *inode, struct file *filp){
    spin_lock(&scull_u_lock);
    scull_u_count--;
    spin_unlock(&scull_u_lock);
    return 0;
}
struct file_operations scull_u_fops = {
        .owner =	THIS_MODULE,
        .llseek =     	scull_llseek,
        .read =       	scull_read,
        .write =      	scull_write,
        .unlocked_ioctl = scull_ioctl,
        .open =       	scull_u_open,
        .release =    	scull_u_release,
};

/******************SCULL SINGLE USER ID ACCESS Device (blocking open - wait)**************************************/
static DEFINE_SPINLOCK(scull_w_lock);
static DECLARE_WAIT_QUEUE_HEAD(scull_w_wait);
static struct scull_dev scull_w_device;
static unsigned long scull_w_count = 0;
static uid_t scull_w_owner;	// initialized to 0 by default

static int scull_w_open(struct inode *node, struct file *filp){

    struct scull_dev *dev = &scull_w_device;
    spin_lock(&scull_w_lock);
    while (!scull_uid_available(scull_w_count, scull_w_owner)){
        spin_unlock(&scull_w_lock);
        // in case NON_BLOCK is based
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        if (wait_event_interruptible(scull_w_wait, scull_uid_available(scull_w_count, scull_w_owner)))
            return -ERESTARTSYS;
        // at this point there might be many processes here
        spin_lock(&scull_w_lock);
    }
    if (scull_w_count == 0)
        scull_w_owner = current_cred()->uid;
    scull_w_count++;
    spin_unlock(&scull_w_lock);
    /* Then, everything is copied from the bar scull device */
    if ( (filp->f_flags & O_ACCMODE) == O_WRONLY)
        scull_trim(dev);
    filp->private_data = dev;
    return 0;
}

static int scull_w_release(struct inode *inode, struct file *filp){
    int temp;
    spin_lock(&scull_w_lock);
    scull_w_count--;
    temp = (int) scull_w_count;
    spin_unlock(&scull_w_lock);
    // No more processes from that uid
    if(temp == 0){
        wake_up_interruptible(&scull_w_wait) ;
    }
    return 0;
}
struct file_operations scull_w_fops = {
        .owner =	THIS_MODULE,
        .llseek =     	scull_llseek,
        .read =       	scull_read,
        .write =      	scull_write,
        .unlocked_ioctl = scull_ioctl,
        .open =       	scull_w_open,
        .release =    	scull_w_release,
};


//-----------------private copies per processes-------------------
static DEFINE_SPINLOCK(scull_c_lock);
static LIST_HEAD(scull_c_list);
/* A placeholder scull_dev which really just holds the cdev stuff. */
static struct scull_dev scull_c_device;
struct scull_listitem {
    struct scull_dev device;
    dev_t key;
    struct list_head list;
};
// Look for a device or create one if missing
static struct scull_dev * scull_c_lookfor_device(dev_t key){
    struct scull_listitem *lptr;
    list_for_each_entry(lptr, &scull_c_list, list){
        if (lptr->key == key)
            return &lptr->device;
    }
    // not found
    lptr = (struct scull_listitem *) kmalloc(sizeof(struct scull_listitem), GFP_KERNEL);
    if (!lptr)
        return NULL;

    // initialize the device
    memset(lptr, 0, sizeof(struct scull_listitem));
    lptr->key = key;
    scull_trim(&lptr->device);
    mutex_init(&(lptr->device.sem));

    // place it in the list
    list_add(&lptr->list, &scull_c_list);
    return &lptr->device;

}

static int scull_c_open(struct inode *inode, struct file *filp) {
    struct scull_dev *dev;
    dev_t key;

    if (!current->signal->tty){
        PDEBUG("Process %s has no ctl tty\n", current->comm)
        return -EINVAL;
    }
    key = tty_devnum(current->signal->tty);
    // look for scullc device in the list
    spin_lock(&scull_c_lock);
    dev = scull_c_lookfor_device(key);
    spin_unlock(&scull_c_lock);

    if (!dev)
        return -ENOMEM;

    /* Then, everything is copied from the bar scull device */
    if ( (filp->f_flags & O_ACCMODE) == O_WRONLY)
        scull_trim(dev);
    filp->private_data = dev;
    return 0;
}



static int scull_c_release(struct inode *inode, struct file *filp)
{
    /*
    * Nothing to do, because the device is persistent.
    * A `real' cloned device should be freed on last close
    */
    return 0;
}
struct file_operations scull_c_fops = {
        .owner =	THIS_MODULE,
        .llseek =     	scull_llseek,
        .read =       	scull_read,
        .write =      	scull_write,
        .unlocked_ioctl = scull_ioctl,
        .open =       	scull_c_open,
        .release =    	scull_c_release,
};
/********************************************
* Init and cleanup functions come last
*/
#define SCULL_MAX_ADEVS 4

static struct scull_adev_info {
   char *name;
   struct scull_dev *sculldev;
   struct file_operations *fops;
}scull_access_devs[SCULL_MAX_ADEVS] = {
        {"scull_single", &scull_s_device, &scull_s_fops},
        {"scull_uid", &scull_u_device, &scull_u_fops},
        {"scull_wuid", &scull_w_device, &scull_w_fops},
        {"scull_priv", &scull_c_device, &scull_c_fops}

};
int scull_access_init(dev_t firstdev){
    int i, err;
    // Get our number
    if (register_chrdev_region(firstdev, SCULL_MAX_ADEVS, "sculla")){
        printk(KERN_WARNING "sculla: device number registration failed\n");
        return 0;
    }
    scull_a_firstdev = firstdev;
    // setup each dev
    for (i=0; i < SCULL_MAX_ADEVS; i++){
        struct scull_adev_info *d = &scull_access_devs[i];
        d->sculldev->quantum = scull_quantum;
        d->sculldev->qset = scull_qset;
        mutex_init(&d->sculldev->sem);
        // cdev init
        cdev_init(&d->sculldev->cdev, d->fops);

        kobject_set_name(&d->sculldev->cdev.kobj, d->name);
        d->sculldev->cdev.owner = THIS_MODULE;
        err = cdev_add(&d->sculldev->cdev, firstdev + i, 1);
        if (err)
            printk(KERN_NOTICE "Error %d adding %s\n", err, d->name);
        else
            printk(KERN_NOTICE "%s registered at %x\n", d->name, firstdev + 1);

    }
    return SCULL_MAX_ADEVS;
}
/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void scull_access_cleanup(void){
    struct scull_listitem *lptr, *next;
    int i;
    /* Clean up the static devs */
    for (i=0; i < SCULL_MAX_ADEVS; i++){
        struct scull_dev *dev = scull_access_devs[i].sculldev;
        cdev_del(&dev->cdev);
        scull_trim(dev);
    }
    /* Clean up all cloned devices - virtual clones */
    list_for_each_safe(lptr, next, &scull_c_list, list){
        list_del(&lptr->list);
        scull_trim(&lptr->device);
        kfree(lptr);

    }
    unregister_chrdev_region(scull_a_firstdev, SCULL_MAX_ADEVS);
}