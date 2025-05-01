#ifndef __ipc_h
#define __ipc_h

#include <linux/fs.h>
#define DEVICE_NAME "ipcdev"

int ipc_open(struct inode *, struct file *);
int ipc_release(struct inode *, struct file *);
ssize_t ipc_read(struct file *, char __user *, size_t, loff_t *);
ssize_t ipc_write(struct file *, const char __user *, size_t, loff_t *);

int ipc_init(void);
void ipc_exit(void);

#endif
