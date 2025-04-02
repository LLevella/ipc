#ifndef __msg_h
#define __msg_h

#include <linux/kernel.h>
#include <linux/mutex.h>

#define INIT 10        // init
#define INIT_STATUS 11 // init
#define SEND 20        // send message

#define NPIDS 4
#define MSG_THEAD int
#define MSG_TDATA char

struct mutex;
struct msg_head {
  MSG_THEAD msg_id;
  MSG_THEAD pid;
} __attribute__((packed));

struct message {
  struct msg_head *head;
  MSG_TDATA *body;
  size_t length;
  MSG_TDATA *data;
};

struct messages_list {
  struct message *msg;
  struct messages_list *next;
};

struct pid_msg {
  struct mutex lock;
  struct messages_list *head;
};

void pids_init(void);
void pids_uninit(void);
int pids_full(void);
int pid_register(int pid);
void pid_unregister(int pid);
struct pid_msg *get_pid_msg(int pid);
struct message *get_tail_msg(struct messages_list *head);
int clean_tail_msg(struct messages_list **head);
int msg_alloc(struct message **msg, size_t length);
int msg_matching(struct message *msg, int pid);
#endif
