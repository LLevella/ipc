#include "msg.h"
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>

static int msgalloc(struct messages_queque *q, int datalen) {
  if (datalen < sizeof(struct msg_head))
    return -1;

  q = (struct messages_queque *)kmalloc(sizeof(struct messages_queque),
                                        GFP_KERNEL);
  if (!q)
    return -1;

  q->data = (MSG_BODY *)kmalloc(sizeof(MSG_BODY) * datalen, GFP_KERNEL);
  if (!q->data)
    return -1;

  q->msg = (struct message *)q->data;
  q->msg->head = (struct msg_head *)q->data;
  q->msg->body = NULL;
  if (datalen > sizeof(struct msg_head))
    q->msg->body = q->data + sizeof(struct msg_head);
  q->next = NULL;
  return 0;
}

void erase_msg_qs(struct messages_queque *q) {
  struct messages_queque *q_next;
  while (q != NULL) {
    q_next = q->next;
    kfree(q->data);
    kfree(q);
    q = q_next;
  }
}

struct messages_queque *add_msg_qs(struct messages_queque *q, int datalen) {
  struct messages_queque *q_curr = q;
  struct messages_queque *q_prev;

  if (q == NULL) {
    if (msgalloc(q, datalen))
      return NULL;
    return q;
  }

  do {
    q_prev = q_curr;
    q_curr = q_curr->next;
  } while (q_curr != NULL);

  if (msgalloc(q_prev->next, datalen))
    return NULL;
  return q_prev->next;
}

struct messages_queque *get_msg_qs(struct messages_queque *q) {
  struct messages_queque *q_curr = q;
  struct messages_queque *q_prev;
  do {
    q_prev = q_curr;
    q_curr = q_curr->next;
  } while (q_curr != NULL);
  return q_prev;
}

MODULE_LICENSE("GPL");