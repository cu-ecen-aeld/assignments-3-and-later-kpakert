/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("kpakert"); 
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *start_entry;
    size_t start_entry_off = 0, read_length = 0;

    if (buf == NULL) {
        return -EFAULT;
    }
    start_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->aesd_cb,
            *f_pos, &start_entry_off);

    if (start_entry == NULL) {
        return 0;
    }  
    read_length = start_entry->size - start_entry_off;
    if (read_length > count) {
        read_length = count;
    }
    if (copy_to_user(buf, &(start_entry->buffptr[start_entry_off]), read_length)) {
        return -EFAULT;
    }
    retval = read_length;
    *f_pos = *f_pos + read_length;

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry new_entry;
    uint8_t in_pos = 0;
    size_t bytes_missing = 0;

    if (buf == NULL) {
        return -EFAULT;
    }

    if (mutex_lock_interruptible(&dev->mx_lock)) {
        return -ERESTARTSYS;
    }
    if (dev->tmp_size > 0) {

        dev->tmp_buf = krealloc(dev->tmp_buf, dev->tmp_size + count, GFP_KERNEL);
        if (!dev->tmp_buf) {
            return -ENOMEM;
        }

        memset(&dev->tmp_buf[dev->tmp_size], 0, count);

        bytes_missing = copy_from_user(&dev->tmp_buf[dev->tmp_size], buf, count);

        retval = count - bytes_missing;
        dev->tmp_size += retval;

    } else {
        dev->tmp_buf = kmalloc(count, GFP_KERNEL);
        if (!dev->tmp_buf) {
            return -ENOMEM;
        }

        memset(dev->tmp_buf, 0, count);

        bytes_missing = copy_from_user(dev->tmp_buf, buf, count);

        retval = count - bytes_missing;
        dev->tmp_size = retval;
    }

    if (memchr(dev->tmp_buf, '\n', dev->tmp_size)) {
        if (dev->aesd_cb.full) {
            in_pos = dev->aesd_cb.in_offs;
            if (dev->aesd_cb.entry[in_pos].buffptr != NULL) {
                kfree(dev->aesd_cb.entry[in_pos].buffptr);
            }
            dev->aesd_cb.entry[in_pos].size = 0;
        }

        new_entry.buffptr = dev->tmp_buf;
        new_entry.size = dev->tmp_size;
        aesd_circular_buffer_add_entry(&dev->aesd_cb, &new_entry);

        dev->tmp_buf = NULL;
        dev->tmp_size = 0;
    } 

    mutex_unlock(&dev->mx_lock);
    dev->buffer_size += retval;

    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    aesd_circular_buffer_init(&aesd_device.aesd_cb);
    aesd_device.tmp_buf = NULL;
    aesd_device.tmp_size = 0;
    mutex_init(&aesd_device.mx_lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    uint8_t index;
    struct aesd_circular_buffer *buffer = &aesd_device.aesd_cb;
    struct aesd_buffer_entry *entry;

    AESD_CIRCULAR_BUFFER_FOREACH(entry, buffer, index) {
    if ((entry->size > 0) && (entry->buffptr != NULL)) {
        kfree(entry->buffptr);
        entry->size = 0;
    }
    }
    mutex_destroy(&aesd_device.mx_lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
