#include "msg.h"
#include <linux/slab.h>
#include <linux/types.h>

static MSG_THEAD pids[NPIDS];
static struct messages_queque *pids_msg_q[NPIDS];

static void msg_q_free(struct messages_queque *q) {
  struct messages_queque *q_next;
  while (q != NULL) {
    q_next = q->next;
    kfree(q->data);
    kfree(q);
    q = q_next;
  }
}
static int msg_q_alloc(struct messages_queque *q, int datalen) {
  if (datalen < sizeof(struct msg_head))
    return -1;
  q = (struct messages_queque *)kmalloc(sizeof(struct messages_queque),
                                        GFP_KERNEL);
  if (!q)
    return -1;
  q->data = (MSG_TDATA *)kmalloc(sizeof(MSG_TDATA) * datalen, GFP_KERNEL);
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

void pids_init(void) {
  int i = 0;
  for (i = 0; i < NPIDS; i++) {
    pids[i] = 0;
    pids_msg_q[i] = NULL;
  }
}

void pids_uninit(void) {
  int i = 0;
  for (i = 0; i < NPIDS; i++) {
    pids[i] = 0;
    msg_q_free(pids_msg_q[i]);
    pids_msg_q[i] = NULL;
  }
}