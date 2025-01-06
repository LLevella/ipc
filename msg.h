#ifndef __msg_h
#define __msg_h

#define INIT 0x100       // init
#define SEND 0x200       // send message
#define UNINIT_CLN 0x300 // uninit

#define NPIDS 128
#define MSG_HEAD int
#define MSG_BODY char

struct msg_head {
  MSG_HEAD msg_id;
  MSG_HEAD receiver_pid;
};

struct message {
  struct msg_head *head;
  MSG_BODY *body;
};

struct messages_queque {
  MSG_BODY *data;
  struct message *msg;
  struct messages_queque *next;
};

void erase_msg_qs(struct messages_queque *q);

struct messages_queque *add_msg_qs(struct messages_queque *q, int datalen);
struct messages_queque *get_msg_qs(struct messages_queque *q);

#endif