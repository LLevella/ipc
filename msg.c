#include "msg.h"
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>

static MSG_THEAD pids[NPIDS];
static struct messages_queque *pids_msg_q[NPIDS];
static DEFINE_MUTEX(pidsmutex);

static void msg_q_free(struct messages_queque *q) {
  struct messages_queque *q_next;

  while (q != NULL) {
    q_next = q->next;
    kfree(q->data);
    kfree(q);
    q = q_next;
  }
}
static int msg_q_alloc(struct messages_queque *q, size_t length) {
  if (datalen < sizeof(struct msg_head))
    return -1;

  q = (struct messages_queque *)kmalloc(sizeof(struct messages_queque),
                                        GFP_KERNEL);
  if (!q)
    return -1;

  q->data = (MSG_TDATA *)kmalloc(sizeof(MSG_TDATA) * length, GFP_KERNEL);

  if (!q->data)
    return -1;

  q->msg = (struct message *)q->data;
  q->msg->head = (struct msg_head *)q->data;

  q->msg->body = NULL;
  if (length > sizeof(struct msg_head))
    q->msg->body = q->data + sizeof(struct msg_head);

  q->length = length;
  q->next = NULL;

  return 0;
}

void pids_init(void) {
  int i = 0;

  for (i = 0; i < NPIDS; i++) {
    pids[i] = 0;
    pids_msg_q[i] = NULL;
  }
}

void pids_uninit(void) {
  int i = 0;

  mutex_lock(&pidsmutex);

  for (i = 0; i < NPIDS; i++) {
    pids[i] = -1;
    msg_q_free(pids_msg_q[i]);
    pids_msg_q[i] = NULL;
  }

  if (mutex_is_locked(&pidsmutex))
    mutex_unlock(&pidsmutex);
}

int pid_nfind(int pid) {
  int i;

  mutex_lock(&pidsmutex);

  for (i = 0; i < NPIDS; i++)
    if (pids[i] == pid)
      break;

  if (i == NPIDS) {
    if (mutex_is_locked(&pidsmutex))
      mutex_unlock(&pidsmutex);
    return -1;
  }

  if (mutex_is_locked(&pidsmutex))
    mutex_unlock(&pidsmutex);

  return i;
}

int pids_full(void) {

  mutex_lock(&pidsmutex);

  for (i = 0; i < NPIDS; i++)
    if (pids[i] < 0) {
      if (mutex_is_locked(&pidsmutex))
        mutex_unlock(&pidsmutex);
      return 0;
    }

  if (mutex_is_locked(&pidsmutex))
    mutex_unlock(&pidsmutex);

  return 1;
}

int pid_register(int pid) {
  int i;

  i = pid_nfind(pid);
  if (i > 0)
    return i;

  mutex_lock(&pidsmutex);
  for (i = 0; i < NPIDS; i++)
    if (pids[i] < 0)
      break;

  if (i == NPIDS) {
    if (mutex_is_locked(&pidsmutex))
      mutex_unlock(&pidsmutex);
    return -1;
  }

  pids[i] = pid;

  if (mutex_is_locked(&pidsmutex))
    mutex_unlock(&pidsmutex);

  return i;
}

void pid_unregister(int pid) {
  int i;

  mutex_lock(&pidsmutex);

  for (i = 0; i < NPIDS; i++)
    if (pids[i] == pid)
      break;

  if (i < NPIDS) {
    pids[i] = -1;
    msg_q_free(pids_msg_q[i]);
  }

  if (mutex_is_locked(&pidsmutex))
    mutex_unlock(&pidsmutex);
}