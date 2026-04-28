#include "msg.h"

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>

static MSG_THEAD pids[NPIDS];
static DEFINE_MUTEX(pidslock); // для блокирования доступа к массиву pids

static struct pid_msg pids_msg_list[NPIDS]; // список структур для процессов

static int pid_find_locked(int pid) {
  int i;

  for (i = 0; i < NPIDS; i++)
    if (pids[i] == pid)
      return i;

  return NOT_FOUND;
}

// очищает цепочку сообщения начиная с q и до хвоста
static void msg_q_free(struct messages_list *q) {
  struct messages_list *q_next;

  while (q != NULL) {
    q_next = q->next;
    msg_free(q->msg);
    kfree(q);
    q = q_next;
  }
}

int msg_alloc(struct message **msg, size_t length) {
  struct message *new_msg;

  if (!msg)
    return INVALID;

  *msg = NULL;

  if (length < sizeof(struct msg_head))
    return INVALID;

  if (length > MAXMSGSIZE)
    return INVALID;

  new_msg = kzalloc(sizeof(*new_msg), GFP_KERNEL);
  if (!new_msg)
    return ERROR;

  new_msg->data = kmalloc(length + 1, GFP_KERNEL);
  if (!new_msg->data) {
    kfree(new_msg);
    return ERROR;
  }

  new_msg->data[length] = '\0';
  new_msg->head = (struct msg_head *)new_msg->data;

  new_msg->body = NULL;
  if (length > sizeof(struct msg_head))
    new_msg->body = new_msg->data + sizeof(struct msg_head);

  new_msg->length = length;
  *msg = new_msg;
  return SUCCESS;
}

void msg_free(struct message *msg) {
  if (!msg)
    return;

  kfree(msg->data);
  kfree(msg);
}

void pids_init(void) {
  int i = 0;
  mutex_lock(&pidslock);
  for (i = 0; i < NPIDS; i++) {
    pids[i] = -1;
    mutex_init(&pids_msg_list[i].lock);
    init_waitqueue_head(&pids_msg_list[i].read_queue);
    pids_msg_list[i].pid = -1;
    pids_msg_list[i].head = NULL;
    pids_msg_list[i].tail = NULL;
    pids_msg_list[i].nmsgs = 0;
    pids_msg_list[i].users = 0;
  }
  mutex_unlock(&pidslock);
}

void pids_uninit(void) {
  int i = 0;

  mutex_lock(&pidslock);

  for (i = 0; i < NPIDS; i++) {
    mutex_lock(&pids_msg_list[i].lock);
    pids[i] = -1;
    msg_q_free(pids_msg_list[i].head);
    pids_msg_list[i].pid = -1;
    pids_msg_list[i].head = NULL;
    pids_msg_list[i].tail = NULL;
    pids_msg_list[i].nmsgs = 0;
    pids_msg_list[i].users = 0;
    mutex_unlock(&pids_msg_list[i].lock);
  }

  mutex_unlock(&pidslock);
}

int pids_full(void) {
  int i;
  int status = FULL;
  mutex_lock(&pidslock);

  for (i = 0; i < NPIDS; i++)
    if (pids[i] < 0) {
      status = SUCCESS;
      break;
    }

  mutex_unlock(&pidslock);
  return status;
}

int pid_nfind(int pid) {
  int i;

  mutex_lock(&pidslock);
  i = pid_find_locked(pid);
  mutex_unlock(&pidslock);

  return i;
}

int pid_register(int pid) {
  int i;

  mutex_lock(&pidslock);

  i = pid_find_locked(pid);
  if (i >= 0) {
    pids_msg_list[i].users++;
    mutex_unlock(&pidslock);
    return SUCCESS;
  }

  for (i = 0; i < NPIDS; i++)
    if (pids[i] < 0)
      break;

  if (i == NPIDS) {
    mutex_unlock(&pidslock);
    return FULL;
  }

  pids[i] = pid;
  pids_msg_list[i].pid = pid;
  pids_msg_list[i].head = NULL;
  pids_msg_list[i].tail = NULL;
  pids_msg_list[i].nmsgs = 0;
  pids_msg_list[i].users = 1;

  mutex_unlock(&pidslock);
  pr_debug("pid_register %d\n", i);
  return SUCCESS;
}

void pid_unregister(int pid) {
  int i;
  struct pid_msg *pidp;

  mutex_lock(&pidslock);

  i = pid_find_locked(pid);
  if (i < 0) {
    mutex_unlock(&pidslock);
    return;
  }

  pidp = &pids_msg_list[i];
  mutex_lock(&pidp->lock);

  if (pidp->users > 1) {
    pidp->users--;
    mutex_unlock(&pidp->lock);
    mutex_unlock(&pidslock);
    return;
  }

  pids[i] = -1;
  pidp->pid = -1;
  pidp->users = 0;
  msg_q_free(pidp->head);
  pidp->head = NULL;
  pidp->tail = NULL;
  pidp->nmsgs = 0;

  mutex_unlock(&pidp->lock);
  mutex_unlock(&pidslock);
  wake_up_interruptible(&pidp->read_queue);
}

struct pid_msg *get_pid_msg_list(int pid) {
  int ipid = pid_nfind(pid);
  if (ipid < 0)
    return NULL;
  return &(pids_msg_list[ipid]);
}

void msg_drop_head(struct pid_msg *pidp) {
  struct messages_list *node;

  if (!pidp || !pidp->head)
    return;

  node = pidp->head;
  pidp->head = node->next;
  if (!pidp->head)
    pidp->tail = NULL;
  if (pidp->nmsgs > 0)
    pidp->nmsgs--;

  node->next = NULL;
  msg_q_free(node);
}

static struct pid_msg *lock_pid_msg(int pid) {
  int ipid;
  struct pid_msg *pidp;

  mutex_lock(&pidslock);
  ipid = pid_find_locked(pid);
  if (ipid < 0) {
    mutex_unlock(&pidslock);
    return NULL;
  }

  pidp = &pids_msg_list[ipid];
  mutex_lock(&pidp->lock);
  mutex_unlock(&pidslock);

  return pidp;
}

// добавляет сообщение в хвост списка
static int add_msg(MSG_THEAD msg_id, void *data, size_t length,
                   MSG_THEAD sender_pid, MSG_THEAD receiver_pid,
                   MSG_THEAD response_msg_id) {
  struct pid_msg *cur_pid_msg;
  struct messages_list *node;
  struct message *msg;
  size_t body_len;
  int status;

  if (length < sizeof(struct msg_head))
    return INVALID;

  body_len = length - sizeof(struct msg_head);
  if (body_len > 0 && !data)
    return INVALID;

  status = msg_alloc(&msg, length);
  if (status != SUCCESS)
    return status;

  node = kmalloc(sizeof(*node), GFP_KERNEL);
  if (!node) {
    msg_free(msg);
    return ERROR;
  }

  if (body_len > 0)
    memcpy(msg->body, data, body_len);
  msg->head->pid = sender_pid;
  msg->head->msg_id = response_msg_id;

  node->msg = msg;
  node->next = NULL;

  cur_pid_msg = lock_pid_msg(receiver_pid);
  if (!cur_pid_msg) {
    msg_q_free(node);
    return NOT_FOUND;
  }

  if (cur_pid_msg->nmsgs >= MAXNMSG) {
    mutex_unlock(&cur_pid_msg->lock);
    msg_q_free(node);
    return FULL;
  }

  if (cur_pid_msg->tail)
    cur_pid_msg->tail->next = node;
  else
    cur_pid_msg->head = node;

  cur_pid_msg->tail = node;
  cur_pid_msg->nmsgs++;

  mutex_unlock(&cur_pid_msg->lock);
  wake_up_interruptible(&cur_pid_msg->read_queue);

  return SUCCESS;
}

static int init_msg(MSG_THEAD pid) {
  MSG_THEAD pids_snapshot[NPIDS];

  mutex_lock(&pidslock);
  memcpy(pids_snapshot, pids, sizeof(pids_snapshot));
  mutex_unlock(&pidslock);

  return add_msg(INIT_STATUS, pids_snapshot,
                 sizeof(struct msg_head) + sizeof(pids_snapshot), 0, pid,
                 INIT_STATUS);
}

static int send_msg(struct message *msg, MSG_THEAD sender_pid,
                    MSG_THEAD receiver_pid) {
  return add_msg(SEND, (void *)msg->body, msg->length, sender_pid, receiver_pid,
                 SEND);
}

int msg_matching(struct message *msg, MSG_THEAD pid) {
  MSG_THEAD sender_pid = pid;
  MSG_THEAD receiver_pid;
  MSG_THEAD msg_id;
  int status = SUCCESS;

  if (!msg || !msg->head)
    return INVALID;

  receiver_pid = msg->head->pid;
  msg_id = msg->head->msg_id;

  switch (msg_id) {
  case INIT:
    status = init_msg(sender_pid);
    break;
  case SEND:
    status = send_msg(msg, sender_pid, receiver_pid);
    break;
  default:
    status = INVALID;
    pr_debug("msg_matching: unknown message id %d\n", msg_id);
    break;
  }
  return status;
}
