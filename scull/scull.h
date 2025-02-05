//
// Created by delaplai on 3/12/2024.
//

#ifndef SCULL_H
#define SCULL_H

#include <linux/ioctl.h>
#include <linux/types.h>
#undef PDEBUG /* undef it, just in case */
#ifdef SCULL_DEBUG
# ifdef __KERNEL__
 /* This one if debugging is on, and kernel space */
# define PDEBUG(fmt, args...) printk( KERN_DEBUG "scull: " fmt, ## args)
# else
 /* This one for user space */
# define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
# endif
#else
# define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif
#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define SCULL_MAJOR 0   /* dynamic major by default */
#define SCULL_NR_DEVS 4    /* scull0 through scull3 */
#define SCULL_QUANTUM 4000
#define SCULL_QSET    1000
#define SCULL_P_BUFFER 4000





struct scull_qset {
    void **data;
    struct scull_qset *next;
};

struct scull_dev {
    int quantum; /* the current quantum size */
    int qset; /* the current array size */
    struct scull_qset *data; /* Pointer to first quantum set */
    unsigned long size; /* amount of data stored here */
    unsigned int access_key; /* used by sculluid and scullpriv */
    struct semaphore sem; /* mutual exclusion semaphore */
    struct cdev cdev; /* Char device structure */
};
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
/**
 *
 *
 */
extern int scull_major;
extern int scull_minor;
extern int scull_nr_devs;	/* number of bare scull devices */
extern int scull_quantum;
extern int scull_qset;
extern int scull_p_buffer;

int scull_trim(struct scull_dev *dev);
ssize_t scull_read(struct file *, char __user *, size_t, loff_t *);
ssize_t scull_write(struct file *, const char __user *, size_t, loff_t *);
long scull_ioctl(struct file *, unsigned int, unsigned long);
loff_t scull_llseek(struct file *, loff_t, int);




/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define SCULL_IOC_MAGIC  'k'
/* Please use a different 8-bit number in your code */

#define SCULL_IOCRESET    _IO(SCULL_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC,  1, int)
#define SCULL_IOCSQSET    _IOW(SCULL_IOC_MAGIC,  2, int)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC,   3)
#define SCULL_IOCTQSET    _IO(SCULL_IOC_MAGIC,   4)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC,  5, int)
#define SCULL_IOCGQSET    _IOR(SCULL_IOC_MAGIC,  6, int)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC,   7)
#define SCULL_IOCQQSET    _IO(SCULL_IOC_MAGIC,   8)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOCXQSET    _IOWR(SCULL_IOC_MAGIC,10, int)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC,  11)
#define SCULL_IOCHQSET    _IO(SCULL_IOC_MAGIC,  12)

/*
 * The other entities only have "Tell" and "Query", because they're
 * not printed in the book, and there's no need to have all six.
 * (The previous stuff was only there to show different ways to do it.
 */
#define SCULL_P_IOCTSIZE _IO(SCULL_IOC_MAGIC,   13)
#define SCULL_P_IOCQSIZE _IO(SCULL_IOC_MAGIC,   14)
/* ... more to come */
#define SCULL_IOC_MAXNR 14
#endif //SCULL_H
