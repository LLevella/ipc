#include "msg.h"

#include <linux/slab.h>

static MSG_THEAD pids[NPIDS];
static struct pid_msg *pids_msg_list[NPIDS];
static DEFINE_MUTEX(pidslock);

static void msg_q_free(struct messages_list *q) {
  struct messages_list *q_next;

  while (q != NULL) {
    q_next = q->next;
    kfree(q->msg->data);
    kfree(q->msg);
    kfree(q);
    q = q_next;
  }
}

int msg_alloc(struct message **msg, size_t length) {
  if (length < sizeof(struct msg_head))
    return -1;

  *msg = (struct message *)kmalloc(sizeof(struct message), GFP_KERNEL);
  if (!(*msg))
    return -1;

  (*msg)->data =
      (MSG_TDATA *)kmalloc(sizeof(MSG_TDATA) * (length + 1), GFP_KERNEL);
  if (!(*msg)->data)
    return -1;
  (*msg)->data[length] = '\0';
  (*msg)->head = (struct msg_head *)(*msg)->data;

  (*msg)->body = NULL;
  if (length > sizeof(struct msg_head))
    (*msg)->body = (*msg)->data + sizeof(struct msg_head);

  (*msg)->length = length;
  return 0;
}

void pids_init(void) {
  int i = 0;

  for (i = 0; i < NPIDS; i++) {
    pids[i] = 0;
    pids_msg_list[i] = NULL;
  }
}

void pids_uninit(void) {
  int i = 0;

  mutex_lock(&pidslock);

  for (i = 0; i < NPIDS; i++) {
    pids[i] = -1;
    msg_q_free(pids_msg_list[i]->head);
    kfree(pids_msg_list[i]);
    pids_msg_list[i] = NULL;
  }

  mutex_unlock(&pidslock);
}

int pids_full(void) {
  int i = 0;
  mutex_lock(&pidslock);

  for (i = 0; i < NPIDS; i++)
    if (pids[i] < 0) {
      mutex_unlock(&pidslock);
      return 0;
    }

  mutex_unlock(&pidslock);

  return 1;
}

int pid_nfind(int pid) {
  int i;

  mutex_lock(&pidslock);

  for (i = 0; i < NPIDS; i++)
    if (pids[i] == pid)
      break;

  if (i == NPIDS) {
    mutex_unlock(&pidslock);
    return -1;
  }

  mutex_unlock(&pidslock);

  return i;
}

int pid_register(int pid) {
  int i;

  i = pid_nfind(pid);
  if (i > 0)
    return i;

  mutex_lock(&pidslock);
  for (i = 0; i < NPIDS; i++)
    if (pids[i] < 0)
      break;

  if (i == NPIDS) {
    mutex_unlock(&pidslock);
    return -1;
  }

  pids[i] = pid;
  pids_msg_list[i] =
      (struct pid_msg *)kmalloc(sizeof(struct pid_msg), GFP_KERNEL);
  if (!pids_msg_list[i])
    return -1;
  mutex_init(&(pids_msg_list[i]->lock));

  mutex_unlock(&pidslock);

  return i;
}

void pid_unregister(int pid) {
  int i;

  mutex_lock(&pidslock);

  for (i = 0; i < NPIDS; i++)
    if (pids[i] == pid)
      break;

  if (i < NPIDS) {
    pids[i] = -1;
    msg_q_free(pids_msg_list[i]->head);
    kfree(pids_msg_list[i]);
    pids_msg_list[i] = NULL;
  }

  mutex_unlock(&pidslock);
}

struct pid_msg *get_pid_msg(int pid) {
  int ipid = pid_nfind(pid);
  if (ipid < 0)
    return NULL;
  return pids_msg_list[ipid];
}

struct message *get_tail_msg(struct messages_list *head) {
  struct messages_list *q_curr = head;
  if (q_curr == NULL)
    return NULL;
  while (q_curr->next != NULL) {
    q_curr = q_curr->next;
  }
  return q_curr->msg;
}

int clean_tail_msg(struct messages_list **head) {
  struct messages_list *q_prev = *head;
  struct messages_list *q_curr, *q_next;

  if (q_prev == NULL)
    return 0;

  q_curr = q_prev->next;
  if (q_curr == NULL) {
    msg_q_free(q_prev);
    *head = NULL;
    return 0;
  }

  q_next = q_curr->next;
  while (q_next != NULL) {
    q_prev = q_curr;
    q_curr = q_next;
    q_next = q_next->next;
  }

  msg_q_free(q_curr);
  q_prev->next = NULL;

  return 0;
}

static int add_msg(MSG_THEAD msg_id, void *data, size_t length, int sender_pid,
                   int receiver_pid) {
  struct pid_msg *cur_pid_msg = get_pid_msg(receiver_pid);
  struct messages_list *head = NULL;
  struct message *msg;
  mutex_lock(&cur_pid_msg->lock);
  head = cur_pid_msg->head;
  cur_pid_msg->head =
      (struct messages_list *)kmalloc(sizeof(struct messages_list), GFP_KERNEL);
  if (!cur_pid_msg->head) {
    mutex_unlock(&cur_pid_msg->lock);
    return -1;
  }
  cur_pid_msg->head->next = head;
  head = cur_pid_msg->head;
  mutex_unlock(&cur_pid_msg->lock);
  if (msg_alloc(&(cur_pid_msg->head->msg), length) < 0)
    return -1;
  memcpy(msg->body, data, length - sizeof(struct msg_head));
  msg->head->pid = sender_pid;
  msg->head->msg_id = msg_id;
  // оправить сигнал процессу на считывание
  return 0;
}

static int init_msg(int pid) {
  return add_msg(INIT_STATUS, (void *)pids,
                 sizeof(struct msg_head) + NPIDS * sizeof(MSG_THEAD), 0, pid);
}

static int send_msg(struct message *msg, int sender_pid, int receiver_pid) {
  return add_msg(SEND, (void *)msg->body, msg->length, sender_pid,
                 receiver_pid);
}

int msg_matching(struct message *msg, int pid) {
  MSG_THEAD sender_pid = pid;
  MSG_THEAD receiver_pid = msg->head->pid;
  MSG_THEAD msg_id = msg->head->msg_id;
  int status = 0;
  switch (msg_id) {
  case INIT:
    status = init_msg(sender_pid);
    break;
  case SEND:
    status = send_msg(msg, sender_pid, receiver_pid);
    break;
  default:
    status = -1;
    break;
  }
  return status;
}