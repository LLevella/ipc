#ifndef __ipc_h
#define __ipc_h
#include <linux/fs.h>

static int ipc_open(struct inode *, struct file *);
static int ipc_release(struct inode *, struct file *);
static ssize_t ipc_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t ipc_write(struct file *, const char __user *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "ipcdev" /* Dev name as it appears in /proc/devices   */
#define BUF_LEN 128          /* Max length of the message from the device */

enum {
  CDEV_NOT_USED = 0,
  CDEV_EXCLUSIVE_OPEN = 1,
};

#endif