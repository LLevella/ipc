#include "ipc.h"
#include "msg.h"

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/version.h>

static int major;
static struct class *cls;
static struct device *dev;

static const struct file_operations ipc_fops = {.owner = THIS_MODULE,
                                                .read = ipc_read,
                                                .write = ipc_write,
                                                .open = ipc_open,
                                                .release = ipc_release,
                                                .poll = ipc_poll,
                                                .llseek = noop_llseek};

/*
 Инициализация, создание устройства, регистрация, инициализация списка для
 процессов, которые открыли это устройство (будем считать открытие
 регистрацией процесса)
*/
int __init ipc_init(void) {
  int err;

  pids_init();
  pr_info("Pids buffer was initialized\n");
  major = register_chrdev(0, DEVICE_NAME, &ipc_fops);
  if (major < 0) {
    pr_alert("Registering char device failed with %d\n", major);
    return major;
  }
  pr_info("%s was assigned major number %d.\n", DEVICE_NAME, major);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
  cls = class_create(DEVICE_NAME);
#else
  cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif
  if (IS_ERR(cls)) {
    err = PTR_ERR(cls);
    unregister_chrdev(major, DEVICE_NAME);
    return err;
  }

  dev = device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
  if (IS_ERR(dev)) {
    err = PTR_ERR(dev);
    class_destroy(cls);
    unregister_chrdev(major, DEVICE_NAME);
    return err;
  }

  pr_info("Device created on /dev/%s\n", DEVICE_NAME);
  return SUCCESS;
}

/*
 Завершение, закрытие устройства, очистка списка зарегистрированных
 процессов)
*/
void __exit ipc_exit(void) {
  pids_uninit();
  pr_info("Pids buffer was uninitialized\n");
  if (!IS_ERR_OR_NULL(dev))
    device_destroy(cls, MKDEV(major, 0));
  if (!IS_ERR_OR_NULL(cls))
    class_destroy(cls);
  unregister_chrdev(major, DEVICE_NAME);
  pr_info("Device was removed from /dev/%s\n", DEVICE_NAME);
}

/*
  Открытие файла устройства. При открытии происходит регистрация процесса.
  Одновременно с драйвером может работать не более NPID процессов. Если NPID+1
  поцесс открывает файл, он засыпает пока не освободится место.
*/
int ipc_open(struct inode *inode, struct file *filp) {
  int pid = task_pid_nr(current);
  struct pid_msg *pidp;
  int err;

  err = pid_register(pid);
  if (err == FULL)
    return -EAGAIN;
  if (err != SUCCESS)
    return -EINVAL;

  pidp = get_pid_msg_list(pid);
  if (!pidp) {
    pid_unregister(pid);
    return -EINVAL;
  }

  filp->private_data = pidp;
  pr_debug("Device was opened by PID %d\n", pid);
  return SUCCESS;
}

/*
  Закрытие файла устройства. При закрытии происходит отмена решистрации
  процесса. Пробудаются другие процессы, которые ожидают регистрации
*/
int ipc_release(struct inode *inode, struct file *filp) {
  struct pid_msg *pidp = filp->private_data;

  if (pidp) {
    pr_debug("Device was closed by PID %d\n", pidp->pid);
    pid_unregister(pidp->pid);
    filp->private_data = NULL;
  }

  return SUCCESS;
}

/*
  Чтение из файла устройства, дескриптор в приватных данных содержит описание
  буфера для сообщений, отправленных данному процессу.
  Чтение по сообщениям, начиная с самых ранних
 */
ssize_t ipc_read(struct file *filp, char __user *buffer, size_t length,
                 loff_t *offset) {

  struct message *msg;
  size_t nbytes;
  struct pid_msg *pidp = filp->private_data;

  if (!pidp)
    return -EINVAL;

  for (;;) {
    if (mutex_lock_interruptible(&pidp->lock))
      return -ERESTARTSYS;

    msg = pidp->head ? pidp->head->msg : NULL;
    if (msg)
      break;

    mutex_unlock(&pidp->lock);
    if (filp->f_flags & O_NONBLOCK)
      return -EAGAIN;

    if (wait_event_interruptible(pidp->read_queue,
                                 READ_ONCE(pidp->head) != NULL))
      return -ERESTARTSYS;
  }

  if (length < msg->length) {
    mutex_unlock(&pidp->lock);
    return -EMSGSIZE;
  }

  nbytes = msg->length;
  if (copy_to_user(buffer, msg->data, nbytes)) {
    mutex_unlock(&pidp->lock);
    return -EFAULT;
  }

  msg_drop_head(pidp);
  mutex_unlock(&pidp->lock);
  return nbytes;
}

/*
  Запись в файл устройства. Получаем сообщение записанное процессом,
  сортируем его.
 */

ssize_t ipc_write(struct file *filp, const char __user *buffer, size_t length,
                  loff_t *offset) {
  struct message *msg;
  int pid = task_pid_nr(current);
  struct pid_msg *pidp = filp->private_data;
  int status;

  if (!pidp)
    return -EINVAL;

  if (length < sizeof(struct msg_head))
    return -EINVAL;

  if (length > MAXMSGSIZE)
    return -EMSGSIZE;

  status = msg_alloc(&msg, length);
  if (status == INVALID)
    return -EINVAL;
  if (status != SUCCESS)
    return -ENOMEM;

  if (copy_from_user(msg->data, buffer, length)) {
    msg_free(msg);
    return -EFAULT;
  }

  status = msg_matching(msg, pid);
  msg_free(msg);

  switch (status) {
  case SUCCESS:
    return length;
  case FULL:
    return -EAGAIN;
  case NOT_FOUND:
    return -ESRCH;
  case INVALID:
    return -EINVAL;
  default:
    return -ENOMEM;
  }
}

/*
  Поллинг - функция возвращает POLLIN | POLLRDNORM, если в буфере, вызвавшего
  процесса, есть сообщения, которые нужно прочитать.
 */
__poll_t ipc_poll(struct file *filp, struct poll_table_struct *poll_tbl) {
  __poll_t mask = (POLLOUT | POLLWRNORM);
  struct pid_msg *pidp = filp->private_data;

  if (!pidp)
    return POLLERR;

  poll_wait(filp, &pidp->read_queue, poll_tbl);

  mutex_lock(&pidp->lock);
  if (pidp->head != NULL)
    mask |= POLLIN | POLLRDNORM;
  mutex_unlock(&pidp->lock);

  return mask;
}

module_init(ipc_init);
module_exit(ipc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple PID-addressed IPC character device");
