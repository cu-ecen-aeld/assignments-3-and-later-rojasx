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
#include <linux/slab.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Xavier Rojas");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    // Guards
    if (f_pos == NULL || buf == NULL || filp == NULL)
    {
        return -ERESTARTSYS;
    }

    // Make vars
    ssize_t retval = 0;
    struct aesd_buffer_entry *read_entry = NULL;
    ssize_t byte_offset = 0;
    ssize_t not_copied = 0;
    struct aesd_dev *device = (struct aesd_dev *)(filp->private_data);

    // Lock
    if(mutex_lock_interruptible(&device->mutex))
    {
        // If lock fails, this is what is returned so return the same thing
        return -EINTR;
    }

    // Now read
    read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(device->buffer), *f_pos, &byte_offset);
    if (!read_entry)
    {
        mutex_unlock(&(device->mutex));
        return retval;
    }
    
    if (count > (read_entry->size - byte_offset))
    {
        count = read_entry->size - byte_offset;
    }

    not_copied = copy_to_user(buf, (read_entry->buffptr + byte_offset), count);
    retval = count - not_copied;
    *f_pos += retval;
    mutex_unlock(&(device->mutex));

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    // Guards
    if (filp == NULL || buf == NULL || f_pos == NULL)
    {
        return -ERESTARTSYS;
    }
    if (count == 0)
    {
        return 0;
    }
    
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    // Vars
    ssize_t retval = -ENOMEM;
    const char *entry = NULL;
    ssize_t not_copied = 0;
    struct aesd_dev *device = (struct aesd_dev *) filp->private_data;

    // Lock
    if (mutex_lock_interruptible(&(device->mutex)))
    {
        // If lock fails, this is what is returned to return the same thing
        return -EINTR;
    }

    // Allocate, ensure success
    /**
     * TODO: can probs just realloc, right?
    */
    if (device->entry.size == 0)
    {
        device->entry.buffptr = kmalloc(count * sizeof(char), GFP_KERNEL);
    }
    else
    {
        device->entry.buffptr = krealloc(device->entry.buffptr, (device->entry.size + count) * sizeof(char), GFP_KERNEL);
    }
    
    if (device->entry.buffptr == NULL)
    {
        mutex_unlock(&(device->mutex));
        return retval;
    }

    not_copied = copy_from_user((void *)(device->entry.buffptr + device->entry.size), buf, count);
    retval = count - not_copied;
    device->entry.size += retval;

    if (memchr(device->entry.buffptr, '\n', device->entry.size))
    {
        entry = aesd_circular_buffer_add_entry(&(device->buffer), &(device->entry));
        if (!entry)
        {
            kfree(entry);
        }
        device->entry.buffptr = NULL;
        device->entry.size = 0;
    }

    mutex_unlock(&device->mutex);
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

    // Init circular buff
    mutex_init(&aesd_device.mutex);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry = NULL;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) 
    {
        kfree(entry->buffptr);
    }
    mutex_destroy(&aesd_device.mutex);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
