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
#ifdef SCULLC_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "scullc: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define SCULLC_MAJOR 0   /* dynamic major by default */
#define SCULLC_DEVS 4    /* scullc0 through scullc3 */

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "scullc_dev->data" points to an array of pointers, each
 * pointer refers to a memory page.
 *
 * The array (quantum-set) is SCULLC_QSET long.
 */
#define SCULLC_QUANTUM  4000 /* use a quantum size like scull */
#define SCULLC_QSET     500



struct scullc_dev{
    void **data;
    struct scullc_dev *next;
    int vmas;
    int quantum;
    int qset;
    size_t size;
    struct semaphore sem; /* mutual exclusion semaphore */
    struct cdev cdev; /* Char device structure */
};





/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define SCULLC_IOC_MAGIC  'k'
/* Please use a different 8-bit number in your code */

#define SCULLC_IOCRESET    _IO(SCULLC_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULLC_IOCSQUANTUM _IOW(SCULLC_IOC_MAGIC,  1, int)
#define SCULLC_IOCSQSET    _IOW(SCULLC_IOC_MAGIC,  2, int)
#define SCULLC_IOCTQUANTUM _IO(SCULLC_IOC_MAGIC,   3)
#define SCULLC_IOCTQSET    _IO(SCULLC_IOC_MAGIC,   4)
#define SCULLC_IOCGQUANTUM _IOR(SCULLC_IOC_MAGIC,  5, int)
#define SCULLC_IOCGQSET    _IOR(SCULLC_IOC_MAGIC,  6, int)
#define SCULLC_IOCQQUANTUM _IO(SCULLC_IOC_MAGIC,   7)
#define SCULLC_IOCQQSET    _IO(SCULLC_IOC_MAGIC,   8)
#define SCULLC_IOCXQUANTUM _IOWR(SCULLC_IOC_MAGIC, 9, int)
#define SCULLC_IOCXQSET    _IOWR(SCULLC_IOC_MAGIC,10, int)
#define SCULLC_IOCHQUANTUM _IO(SCULLC_IOC_MAGIC,  11)
#define SCULLC_IOCHQSET    _IO(SCULLC_IOC_MAGIC,  12)


#define SCULLC_IOC_MAXNR 12
#endif //SCULL_H
