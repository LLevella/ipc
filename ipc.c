#include "ipc.h"
#include "msg.h"

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/sched.h>

static DECLARE_WAIT_QUEUE_HEAD(waitpids);

static int major;
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
  unregister_chrdev(major, DEVICE_NAME);
}

/* Methods */

/* Called when a process tries to open the device file, like
 * "sudo cat /dev/ipcdev"
 */
int ipc_open(struct inode *inode, struct file *filp) {
  int pid = task_pid_nr(current);
  int i, is_sig = 0;

  if ((filp->f_flags & O_NONBLOCK) && pids_full())
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
  filp->private_data = get_pid_msg(pid);

  return SUCCESS;
}

/* Called when a process closes the device file. */
int ipc_release(struct inode *inode, struct file *filp) {
  int pid = task_pid_nr(current);
  pid_unregister(pid);
  filp->private_data = NULL;
  wake_up(&waitpids);
  module_put(THIS_MODULE);
  return SUCCESS;
}

/* Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
ssize_t ipc_read(struct file *filp, char __user *buffer, size_t length,
                 loff_t *offset) {

  struct message *msg;
  int nbytes = 0;
  struct pid_msg *pidp = filp->private_data;

  if (mutex_lock_interruptible(&pidp->lock))
    return -ERESTARTSYS;

  msg = get_tail_msg(pidp->head);
  nbytes = ((length < msg->length) ? length : msg->length);

  if ((msg == NULL) || copy_to_user(buffer, msg->data, nbytes)) {
    mutex_unlock(&pidp->lock);
    return -EFAULT;
  }

  *offset += nbytes;
  clean_tail_msg(&(pidp->head));

  mutex_unlock(&pidp->lock);
  return nbytes;
}

ssize_t ipc_write(struct file *filp, const char __user *buffer, size_t length,
                  loff_t *offset) {
  struct message *msg;
  int nbytes = 0;
  int pid = task_pid_nr(current);
  struct pid_msg *pidp = filp->private_data;

  if (msg_alloc(&msg, length) < 0)
    return -ENOMEM;

  if (mutex_lock_interruptible(&pidp->lock))
    return -ERESTARTSYS;

  if (copy_from_user(msg->data, buffer, length)) {
    mutex_unlock(&pidp->lock);
    return -EFAULT;
  }

  nbytes = length;
  *offset += nbytes;
  mutex_unlock(&pidp->lock);

  if (!msg_matching(msg, pid))
    return -EFAULT;

  return nbytes;
}

module_init(ipc_init);
module_exit(ipc_exit);

MODULE_LICENSE("GPL");
