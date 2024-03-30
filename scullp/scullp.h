//
// Created by delaplai on 3/12/2024.
//

#ifndef SCULL_H
#define SCULL_H
#include <linux/ioctl.h>
#include <linux/cdev.h>

/*
 * Macros to help debugging
 */

#undef PDEBUG             /* undef it, just in case */
#ifdef SCULLP_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "scullp: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define SCULLP_MAJOR 0   /* dynamic major by default */
#define SCULLP_DEVS 4    /* scullp0 through scullp3 */

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "scullp_dev->data" points to an array of pointers, each
 * pointer refers to a memory page.
 *
 * The array (quantum-set) is SCULLP_QSET long.
 */
#define SCULLP_ORDER    0 /* Use one page per qunatum */
#define SCULLP_QSET     500



struct scullp_dev{
    void **data;
    struct scullp_dev *next;
    int vmas;
    int qset;
    int order;                /* the current allocation order */
    size_t size;
    struct semaphore sem; /* mutual exclusion semaphore */
    struct cdev cdev; /* Char device structure */
};





/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define SCULLP_IOC_MAGIC  'k'
/* Please use a different 8-bit number in your code */

#define SCULLP_IOCRESET    _IO(SCULLP_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULLP_IOCSORDER _IOW(SCULLP_IOC_MAGIC,  1, int)
#define SCULLP_IOCSQSET    _IOW(SCULLP_IOC_MAGIC,  2, int)
#define SCULLP_IOCTORDER _IO(SCULLP_IOC_MAGIC,   3)
#define SCULLP_IOCTQSET    _IO(SCULLP_IOC_MAGIC,   4)
#define SCULLP_IOCGORDER _IOR(SCULLP_IOC_MAGIC,  5, int)
#define SCULLP_IOCGQSET    _IOR(SCULLP_IOC_MAGIC,  6, int)
#define SCULLP_IOCQORDER _IO(SCULLP_IOC_MAGIC,   7)
#define SCULLP_IOCQQSET    _IO(SCULLP_IOC_MAGIC,   8)
#define SCULLP_IOCXORDER _IOWR(SCULLP_IOC_MAGIC, 9, int)
#define SCULLP_IOCXQSET    _IOWR(SCULLP_IOC_MAGIC,10, int)
#define SCULLP_IOCHORDER _IO(SCULLP_IOC_MAGIC,  11)
#define SCULLP_IOCHQSET    _IO(SCULLP_IOC_MAGIC,  12)


#define SCULLP_IOC_MAXNR 12
#endif //SCULL_H
