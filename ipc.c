#include "ipc.h"
#include "msg.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>

static DECLARE_WAIT_QUEUE_HEAD(waitpids);
static atomic_t pids_full = ATOMIC_INIT(0);

static int major;
static char msg[BUF_LEN + 1]; /* The msg the device will give when asked */
static struct class *cls;

static struct file_operations ipc_fops = {
    .read = ipc_read,
    .write = ipc_write,
    .open = ipc_open,
    .release = ipc_release,
};

int __init ipc_init(void) {
  pids_init();
  pr_info("Pids buffer was initilized");
  major = register_chrdev(0, DEVICE_NAME, &ipc_fops);
  if (major < 0) {
    pr_alert("Registering char device failed with %d\n", major);
    return major;
  }
  pr_info("I was assigned major number %d.\n", major);
  cls = class_create(THIS_MODULE, DEVICE_NAME);
  device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
  pr_info("Device created on /dev/%s\n", DEVICE_NAME);
  return SUCCESS;
}

void __exit ipc_exit(void) {
  pids_uninit();
  pr_info("Pids buffer was uninitilized");
  device_destroy(cls, MKDEV(major, 0));
  class_destroy(cls);
  /* Unregister the device */
  unregister_chrdev(major, DEVICE_NAME);
}

/* Methods */

/* Called when a process tries to open the device file, like
 * "sudo cat /dev/ipcdev"
 */
int ipc_open(struct inode *inode, struct file *file) {
  int pid = task_pid_nr(current);
  int i, is_sig = 0;

  if ((file->f_flags & O_NONBLOCK) && pids_full())
    return -EAGAIN;

  try_module_get(THIS_MODULE);

  while (pids_full()) {
    wait_event_interruptible(waitpids, !pids_full());
    for (i = 0; i < _NSIG_WORDS && !is_sig; i++)
      is_sig = current->pending.signal.sig[i] & ~current->blocked.sig[i];
    if (is_sig) {
      module_put(THIS_MODULE);
      return -EINTR;
    }
  }

  pid_register(pid);
  return SUCCESS;
}

/* Called when a process closes the device file. */
int ipc_release(struct inode *inode, struct file *file) {
  int pid = task_pid_nr(current);
  pid_unregister(pid);
  wake_up(&waitpids);
  module_put(THIS_MODULE);
  return SUCCESS;
}

/* Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
ssize_t ipc_read(struct file *filp,   /* see include/linux/fs.h   */
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

  /* Most read functions return the number of bytes put into the buffer. */
  return bytes_read;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello */
ssize_t device_write(struct file *file, const char __user *buffer,
                     size_t length, loff_t *offset) {
  int i;
  pr_info("device_write(%p,%p,%ld)", file, buffer, length);
  for (i = 0; i < length && i < BUF_LEN; i++)
    get_user(msg[i], buffer + i);
  return i;
}

module_init(ipc_init);
module_exit(ipc_exit);

MODULE_LICENSE("GPL");