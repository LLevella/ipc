#include "ipc.h"
#include "msg.h"

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>

static int major;
static struct class *cls;

static struct file_operations ipc_fops = {.read = ipc_read,
                                          .write = ipc_write,
                                          .open = ipc_open,
                                          .release = ipc_release,
                                          .poll = ipc_poll};

/*
 Инициализация, создание устройства, регистрация, инициализация списка для
 процессов, которые открыли это устройство (будем считать открытие
 регистрацией процесса)
*/
int __init ipc_init(void) {
  pids_init();
  pr_info("Pids buffer was initilized");
  major = register_chrdev(0, DEVICE_NAME, &ipc_fops);
  if (major < 0) {
    pr_alert("Registering char device failed with %d\n", major);
    return major;
  }
  pr_info("%s was assigned major number %d.\n", DEVICE_NAME, major);
  cls = class_create(THIS_MODULE, DEVICE_NAME);
  device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
  pr_info("Device created on /dev/%s\n", DEVICE_NAME);
  return SUCCESS;
}

/*
 Завершение, закрытие устройства, очистка списка зарегистрированных
 процессов)
*/
void __exit ipc_exit(void) {
  pids_uninit();
  pr_info("Pids buffer was uninitilized");
  device_destroy(cls, MKDEV(major, 0));
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

  pr_info("Device was opened by PID %d", pid);

  err = pids_full();
  if (err)
    return -EAGAIN;

  err = pid_register(pid);
  if (err)
    return -EINVAL;
  pr_info("PID was registered");

  pidp = get_pid_msg_list(pid);
  if (!pidp)
    return -EINVAL;

  filp->private_data = pidp;
  pr_info("PID msg list now in private data of file ");
  try_module_get(THIS_MODULE);
  return SUCCESS;
}

/*
  Закрытие файла устройства. При закрытии происходит отмена решистрации
  процесса. Пробудаются другие процессы, которые ожидают регистрации
*/
int ipc_release(struct inode *inode, struct file *filp) {
  int pid = task_pid_nr(current);
  pr_info("Device was closed by PID %d", pid);
  pid_unregister(pid);
  pr_info("PID was unregistered");
  module_put(THIS_MODULE);
  pr_info("PID %d closed device file", pid);
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
  int nbytes = 0;
  struct pid_msg *pidp = filp->private_data;
  pr_info("PID pointer write device file");
  if (mutex_lock_interruptible(&pidp->lock))
    return -ERESTARTSYS;
  pr_info("Mutex locked");
  msg = get_tail_msg(pidp->head);
  if (!msg) {
    pr_info("Nothing for reading");
    mutex_unlock(&pidp->lock);
    return nbytes;
  }
  pr_info("Message was founded");
  nbytes = ((length < msg->length) ? length : msg->length);
  if (copy_to_user(buffer, msg->data, nbytes)) {
    mutex_unlock(&pidp->lock);
    return -EFAULT;
  }
  pr_info("%d bytes was readed", nbytes);
  // //*offset += nbytes;
  clean_tail_msg(&(pidp->head));
  pr_info("Message was removed");
  mutex_unlock(&pidp->lock);
  pr_info("Mutex unlocked");
  return nbytes;
}

/*
  Запись в файл устройства. Получаем сообщение записанное процессом,
  сортируем его.
 */

ssize_t ipc_write(struct file *filp, const char __user *buffer, size_t length,
                  loff_t *offset) {
  struct message *msg;
  int nbytes = length;
  int pid = task_pid_nr(current);
  pr_info("Write function was called by PID %d\n", pid);
  struct pid_msg *pidp = filp->private_data;
  pr_info("PID pointer write device file");
  if (msg_alloc(&msg, length) < 0)
    return -ENOMEM;
  pr_info("Message allocated");
  if (mutex_lock_interruptible(&pidp->lock))
    return -ERESTARTSYS;
  pr_info("Mutex locked");
  if (copy_from_user(msg->data, buffer, length)) {
    mutex_unlock(&pidp->lock);
    return -EFAULT;
  }
  nbytes = length;
  pr_info("Data was copied from user space to msg buffer, %d bytes", nbytes);
  //*offset += nbytes;
  mutex_unlock(&pidp->lock);
  pr_info("Mutex unlocked");
  if (!msg_matching(msg, pid))
    return -ERROR;
  pr_info("MSG was filtered");
  return nbytes;
}

/*
  Поллинг - функция возвращает POLLIN | POLLRDNORM, если в буфере, вызвавшего
  процесса, есть сообщения, которые нужно прочитать.
 */
__poll_t ipc_poll(struct file *filp, struct poll_table_struct *poll_tbl) {
  __poll_t mask = 0;
  int pid = task_pid_nr(current);
  pr_info("Poll function was called by PID %d\n", pid);
  struct pid_msg *pidp = filp->private_data;
  pr_info("PID pointer write device file");
  if (pid_nfind(pid) < 0)
    return mask;
  pr_info("PID was not registered");
  mutex_lock(&pidp->lock);
  pr_info("Mutex locked");
  if (pidp->head != NULL) {
    // there is data in pids buffer
    mask = POLLIN | POLLRDNORM;
    pr_info("PID %d buffer has data for reading", pid);
  }
  mutex_unlock(&pidp->lock);
  pr_info("Mutex unlocked");
  return mask;
}

module_init(ipc_init);
module_exit(ipc_exit);

MODULE_LICENSE("GPL");
