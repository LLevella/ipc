#ifndef __msg_h
#define __msg_h

#define INIT 0x100       // init
#define SEND 0x200       // send message
#define UNINIT_CLN 0x300 // uninit

#define NPIDS 128
#define MSG_THEAD int
#define MSG_TDATA char

struct msg_head {
  MSG_THEAD msg_id;
  MSG_THEAD receiver_pid;
} __attribute__((packed));

struct message {
  struct msg_head *head;
  MSG_TDATA *body;
};

struct messages_queque {
  MSG_TDATA *data;
  struct message *msg;
  struct messages_queque *next;
};

void pids_init(void);
void pids_uninit(void);
// void erase_msg_qs(struct messages_queque *q);

// struct messages_queque *add_msg_qs(struct messages_queque *q, int datalen);
// struct messages_queque *get_msg_qs(struct messages_queque *q);

#endif