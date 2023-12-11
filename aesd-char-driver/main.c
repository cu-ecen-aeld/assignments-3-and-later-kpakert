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
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("kpakert"); 
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *new_dev;
    PDEBUG("open");
    new_dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = new_dev;
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
    struct aesd_dev *read_dev;
    struct aesd_buffer_entry *current_entry;
    size_t offset =0;
    size_t copy_bytes, uncopied_bytes;
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    //get the device
    read_dev = filp->private_data;

    //lock the device
    mutex_lock(&read_dev->lock);

    //get the entry in the circular buffer at position and offset
    current_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&read_dev->circ_buffer, *f_pos, &offset);
    
    if (current_entry==NULL) {

        PDEBUG("no entry found for offset");
        mutex_unlock(&read_dev->lock);
        return retval;
    }
    //determine number of bytes remaining to copy
    copy_bytes = current_entry->size -offset;
    uncopied_bytes = 0;

    //if copy bytes is less than max, copy all. else copy max (count) 
    if (copy_bytes < count)
    {
        uncopied_bytes = copy_to_user(buf, current_entry->buffptr +offset, copy_bytes);
        if (uncopied_bytes) {
            retval = -EFAULT;
        } else {
            retval = copy_bytes;
        }
    } else {
        uncopied_bytes = copy_to_user(buf, current_entry->buffptr +offset, count);
        if (uncopied_bytes) {
            retval = -EFAULT;
        } else {
            retval = count;
        }
    }
    //set reval to be the total bytes copied and update pointer pos
    *f_pos+= retval;
    

    //unlock the device
    mutex_unlock(&read_dev->lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *write_dev;
    char *write_data; 
    size_t newline_index, this_chunk;
    bool full_packet;

    struct aesd_buffer_entry new_entry;
    struct aesd_buffer_entry *last_entry;



    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    //get the device
    write_dev = filp->private_data;

    //allocate memoy for write data
    write_data = kmalloc(count, GFP_KERNEL);

    //if allocation fails, return current value
    if (!write_data)
    {
        return retval;
    }

    //copy data into write data from user space, return error on failure
    if(copy_from_user(write_data, buf, count))
    {
        retval = -EFAULT;
        kfree(write_data);
        return retval;
    }

    //lock the device
    mutex_lock(&write_dev->lock);

    //iterate through data to look for newline character
    
    newline_index = 0;
    full_packet = false;
    while (newline_index < count)
    {
        if (write_data[newline_index] == '\n')
        {
            full_packet = true;
            break;
        }
        newline_index++;
    } 
    if (full_packet) {
        this_chunk = newline_index+1; //include the newline in the append
    } else {
        this_chunk = count; //write all to write buffer
    }
    
    //initialize write buffer if not already
    if(write_dev->buffer_size == 0)
    {
        write_dev->dev_buffer = kmalloc(this_chunk, GFP_KERNEL);
        if(write_dev->dev_buffer == NULL) {
            kfree(write_data);
            mutex_unlock(&write_dev->lock);
            return retval;
        }
    } else {
        write_dev->dev_buffer = krealloc(write_dev->dev_buffer, (write_dev->buffer_size + this_chunk), GFP_KERNEL);
        if(write_dev->dev_buffer == NULL) {
            kfree(write_data);
            mutex_unlock(&write_dev->lock);
            return retval;
        }
    }

    //copy the indexed data into the device buffer to be written
    memcpy(write_dev->dev_buffer + write_dev->buffer_size, write_data, this_chunk);
    write_dev->buffer_size += this_chunk;
    //printf("I copied data for write\n");

    //if a full packet was recieved we add the buffer into an entry
    if (full_packet) {

        new_entry.size = write_dev->buffer_size;
        new_entry.buffptr = write_dev->dev_buffer;


        if (write_dev->circ_buffer.full) 
        {
            last_entry = &write_dev->circ_buffer.entry[write_dev->circ_buffer.in_offs];
            if (last_entry != NULL && last_entry->buffptr != NULL){
                kfree(last_entry->buffptr);
            }
            last_entry->buffptr=NULL;
            last_entry->size=0;
        }
        aesd_circular_buffer_add_entry(&write_dev->circ_buffer, &new_entry);

        //kfree(write_dev->dev_buffer);
        write_dev->buffer_size=0;
    }

    retval = this_chunk;

    if(write_data){
        kfree(write_data);
    }

    *f_pos += retval;

    //unlock the device
    mutex_unlock(&write_dev->lock);

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

    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.circ_buffer);
    //aesd_device.working_entry=NULL;


    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circ_buffer, index) {
        kfree(entry->buffptr);
        }

    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);