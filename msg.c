#include "msg.h"

#include <linux/slab.h>

static MSG_THEAD pids[NPIDS];
static DEFINE_MUTEX(pidslock); // для блокирования доступа к массиву pids

static struct pid_msg pids_msg_list[NPIDS]; // список структур для процессов

// очищает цепочку сообщения начиная с q и до хвоста
static void msg_q_free(struct messages_list *q) {
  struct messages_list *q_next;

  while (q != NULL) {
    q_next = q->next;
    if (q->msg->data)
      kfree(q->msg->data);
    if (q->msg)
      kfree(q->msg);
    if (q)
      kfree(q);
    q = q_next;
  }
}

int msg_alloc(struct message **msg, size_t length) {
  if (length < sizeof(struct msg_head))
    return ERROR;

  *msg = (struct message *)kmalloc(sizeof(struct message), GFP_KERNEL);
  if (!(*msg))
    return ERROR;

  (*msg)->data =
      (MSG_TDATA *)kmalloc(sizeof(MSG_TDATA) * (length + 1), GFP_KERNEL);
  if (!(*msg)->data)
    return ERROR;
  (*msg)->data[length] = '\0';
  (*msg)->head = (struct msg_head *)(*msg)->data;

  (*msg)->body = NULL;
  if (length > sizeof(struct msg_head))
    (*msg)->body = (*msg)->data + sizeof(struct msg_head);

  (*msg)->length = length;
  return SUCCESS;
}

void pids_init(void) {
  int i = 0;
  mutex_lock(&pidslock);
  for (i = 0; i < NPIDS; i++) {
    pids[i] = -1;
    pids_msg_list[i].head = NULL;
    pids_msg_list[i].nmsgs = 0;
  }
  mutex_unlock(&pidslock);
}

void pids_uninit(void) {
  int i = 0;

  mutex_lock(&pidslock);

  for (i = 0; i < NPIDS; i++) {
    pids[i] = -1;
    msg_q_free(pids_msg_list[i].head);
    pids_msg_list[i].head = NULL;
    pids_msg_list[i].nmsgs = 0;
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

  for (i = 0; i < NPIDS; i++)
    if (pids[i] == pid)
      break;

  if (i == NPIDS) {
    mutex_unlock(&pidslock);
    return ERROR;
  }
  mutex_unlock(&pidslock);
  return i;
}

int pid_register(int pid) {
  int i = pid_nfind(pid);
  if (i > 0)
    return SUCCESS;

  mutex_lock(&pidslock);
  for (i = 0; i < NPIDS; i++)
    if (pids[i] < 0)
      break;

  if (i == NPIDS) {
    mutex_unlock(&pidslock);
    return ERROR;
  }

  pids[i] = pid;
  mutex_init(&(pids_msg_list[i].lock));

  mutex_unlock(&pidslock);
  printk("pid_register %d", i);
  return SUCCESS;
}

void pid_unregister(int pid) {
  int i;

  mutex_lock(&pidslock);

  for (i = 0; i < NPIDS; i++)
    if (pids[i] == pid)
      break;

  if (i < NPIDS) {
    pids[i] = -1;
    msg_q_free(pids_msg_list[i].head);
    pids_msg_list[i].head = NULL;
    pids_msg_list[i].nmsgs = 0;
  }

  mutex_unlock(&pidslock);
}

struct pid_msg *get_pid_msg_list(int pid) {
  int ipid = pid_nfind(pid);
  if (ipid < 0)
    return NULL;
  return &(pids_msg_list[ipid]);
}

static void print_msg_data(struct message *msg) {
  int i;
  pr_info("print_msg_data: msg->head {%d, %d}", msg->head->msg_id,
          msg->head->pid);
  pr_info("print_msg_data: msg->body:");
  for (i = 0; i < msg->length - sizeof(struct msg_head); i++)
    pr_info("%x  ", msg->body[i]);
  pr_info("print_msg_data: msg->data:");
  for (i = 0; i < msg->length; i++)
    pr_info("%x  ", msg->data[i]);
}
struct message *get_tail_msg(struct messages_list *head) {
  struct messages_list *q_curr = head;
  if (q_curr == NULL)
    return NULL;
  while (q_curr->next != NULL) {
    q_curr = q_curr->next;
  }
  pr_info("get_tail_msg print msg");
  if (q_curr->msg)
    print_msg_data(q_curr->msg);
  return q_curr->msg;
}

int clean_tail_msg(struct messages_list **head) {
  struct messages_list *q_prev = *head;
  struct messages_list *q_curr, *q_next;

  if (q_prev == NULL)
    return SUCCESS;

  q_curr = q_prev->next;
  if (q_curr == NULL) {
    msg_q_free(q_prev);
    *head = NULL;
    return SUCCESS;
  }

  q_next = q_curr->next;
  while (q_next != NULL) {
    q_prev = q_curr;
    q_curr = q_next;
    q_next = q_next->next;
  }

  msg_q_free(q_curr);
  q_prev->next = NULL;

  return SUCCESS;
}

// добавляет сообщение в хвост списка
static int add_msg(MSG_THEAD msg_id, void *data, size_t length,
                   MSG_THEAD sender_pid, MSG_THEAD receiver_pid,
                   MSG_THEAD response_msg_id) {
  struct pid_msg *cur_pid_msg = get_pid_msg_list(receiver_pid);
  struct messages_list *head = NULL;
  struct message *msg;
  mutex_lock(&cur_pid_msg->lock);
  if (cur_pid_msg->nmsgs >= MAXNMSG) {
    mutex_unlock(&cur_pid_msg->lock);
    return FULL;
  } else
    cur_pid_msg->nmsgs++;

  head = cur_pid_msg->head;
  cur_pid_msg->head =
      (struct messages_list *)kmalloc(sizeof(struct messages_list), GFP_KERNEL);
  if (!cur_pid_msg->head) {
    mutex_unlock(&cur_pid_msg->lock);
    return ERROR;
  }
  cur_pid_msg->head->next = head;
  head = cur_pid_msg->head;
  mutex_unlock(&cur_pid_msg->lock);
  if (msg_alloc(&(cur_pid_msg->head->msg), length) < 0)
    return ERROR;
  msg = cur_pid_msg->head->msg;
  memcpy(msg->body, data, length - sizeof(struct msg_head));
  msg->head->pid = sender_pid;
  msg->head->msg_id = response_msg_id;

  pr_info("add_msg print msg");
  if (cur_pid_msg->head->msg)
    print_msg_data(cur_pid_msg->head->msg);
  return SUCCESS;
}

static int init_msg(MSG_THEAD pid) {
  return add_msg(INIT_STATUS, (void *)pids,
                 sizeof(struct msg_head) + NPIDS * sizeof(MSG_THEAD), 0, pid,
                 INIT_STATUS);
}

static int send_msg(struct message *msg, MSG_THEAD sender_pid,
                    MSG_THEAD receiver_pid) {
  return add_msg(SEND, (void *)msg->body, msg->length, sender_pid, receiver_pid,
                 SEND);
}

int msg_matching(struct message *msg, MSG_THEAD pid) {
  pr_info("msg_matching print msg");
  if (msg)
    print_msg_data(msg);
  MSG_THEAD sender_pid = pid;
  MSG_THEAD receiver_pid = msg->head->pid;
  MSG_THEAD msg_id = msg->head->msg_id;
  int status = SUCCESS;
  switch (msg_id) {
  case INIT:
    status = init_msg(sender_pid);
    break;
  case SEND:
    status = send_msg(msg, sender_pid, receiver_pid);
    break;
  default:
    status = ERROR;
    printk("msg_matching: unknown message id");
    break;
  }
  return status;
}