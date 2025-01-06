
#include "ipc.h"
#include "msg.h"

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>

static int major; /* major number assigned to our device driver */
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);
static char msg[BUF_LEN + 1]; /* The msg the device will give when asked */
static struct class *cls;

static struct file_operations ipc_fops = {
    .read = ipc_read,
    .write = ipc_write,
    .open = ipc_open,
    .release = ipc_release,
};

static struct messages_queque *msg_qs[NPIDS];
static MSG_HEAD pids[NPIDS];

static int __init ipc_init(void) {
  int i;
  major = register_chrdev(0, DEVICE_NAME, &ipc_fops);

  if (major < 0) {
    pr_alert("Registering char device failed with %d\n", major);
    return major;
  }

  pr_info("IPC device was assigned major number %d.\n", major);

  cls = class_create(THIS_MODULE, DEVICE_NAME);
  device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

  pr_info("Device created on /dev/%s\n", DEVICE_NAME);

  for (i = 0; i < NPIDS; i++) {
    pids[i] = -1;
    msg_qs[i] = NULL;
  }

  return SUCCESS;
}

static void __exit ipc_exit(void) {
  int i;
  device_destroy(cls, MKDEV(major, 0));
  class_destroy(cls);

  for (i = 0; i < NPIDS; i++) {
    pids[i] = -1;
    erase_msg_qs(msg_qs[i]);
  }
  /* Unregister the device */
  unregister_chrdev(major, DEVICE_NAME);
}

/* Methods */

/*
  Called when a process tries to open the device file
 */
static int ipc_open(struct inode *inode, struct file *file) {
  char pid;
  if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
    return -EBUSY;
  pid = task_pid_nr(current);
  pr_info("IPC device was opened by pid %c!\n", pid);
  try_module_get(THIS_MODULE);

  return SUCCESS;
}

/*
  Called when a process closes the device file.
*/
static int ipc_release(struct inode *inode, struct file *file) {
  /* We're now ready for our next caller */
  pr_info("IPC device was released!\n");
  atomic_set(&already_open, CDEV_NOT_USED);
  module_put(THIS_MODULE);

  return SUCCESS;
}

/*
  Called when a process, which already opened the dev file, attempts to read
  from it.
*/
static ssize_t ipc_read(struct file *file,
                        char __user *buffer, /* buffer to fill with data */
                        size_t length,       /* length of the buffer     */
                        loff_t *offset) {
  /* Number of bytes actually written to the buffer */
  int bytes_read = 0;
  const char *msg_ptr = msg;

  if (!*(msg_ptr + *offset)) { /* we are at the end of message */
    *offset = 0;               /* reset the offset */
    return 0;                  /* signify end of file */
  }

  msg_ptr += *offset;

  /* Actually put the data into the buffer */
  while (length && *msg_ptr) {
    /* The buffer is in the user data segment, not the kernel
     * segment so "*" assignment won't work.  We have to use
     * put_user which copies data from the kernel data segment to
     * the user data segment.
     */
    put_user(*(msg_ptr++), buffer++);
    length--;
    bytes_read++;
  }

  *offset += bytes_read;
  pr_info("ipc_read(%p,%p,%ld)", file, buffer, length);
  /* Most read functions return the number of bytes put into the buffer. */
  return bytes_read;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello */
static ssize_t ipc_write(struct file *file, const char __user *buffer,
                         size_t length, loff_t *offset) {
  int i;

  pr_info("ipc_write(%p,%p,%ld)", file, buffer, length);

  for (i = 0; i < length && i < BUF_LEN; i++)
    get_user(msg[i], buffer + i);

  /* Again, return the number of input characters used. */
  return i;
}

module_init(ipc_init);
module_exit(ipc_exit);

MODULE_LICENSE("GPL");