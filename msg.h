#ifndef __msg_h
#define __msg_h

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/wait.h>

// номера команд в протоколе сообщений
#define INIT 10
// инициализация, когда процесс хочет
// узнать номера процессов - соседей
#define INIT_STATUS 11
// ответ на собщение - инициализацию,
// содержит номра процессов - соседей
#define SEND 20
// непосредственно отправка сообщения

#define MAXNMSG 16
// максимальное колличество сообщений,
// которые могут храниться для каждого
// процесса
// здесь считаем, что все процессы умеют читать
// и писать, иначе буфер для не читающего процесса
// заполнится и 16 сообщений будут висеть до момента
// отмены регистрации этого процесса

#define MAXMSGSIZE 4096
// максимальный размер одного сообщения в байтах

// TODO в протоколе init ввести тип процесса и сразу
// отвергать запись в буфер для нечитающих процессов

// TODO подумать над разделением процессов на сервера
// и клиенты уже в протоколе

#define NPIDS 4
// число процессов, которые могут
// одновременно работать с драйвером
#define MSG_THEAD int
// тип данных в заголовке
#define MSG_TDATA char
// тип данных в теле сообщения

// статус завершения процедур
enum {
  INVALID = -3,
  NOT_FOUND = -2,
  ERROR = -1,
  SUCCESS = 0,
  FULL = 1,
};

struct msg_head {
  MSG_THEAD msg_id; //номер команды
  MSG_THEAD pid;    // номер процесса - получателя
} __attribute__((packed));

struct message {
  struct msg_head *head;
  MSG_TDATA *body; // тело сообщения
  size_t length; // длина всего сообщения (вместе с заголовком) в байтах
  MSG_TDATA *data; // представление всего сообщения в байтах
};

// цепочка сообщений
struct messages_list {
  struct message *msg;
  struct messages_list *next;
};

// Структура для работы с сообщением
struct pid_msg {
  struct mutex lock;
  wait_queue_head_t read_queue;
  MSG_THEAD pid;
  struct messages_list *head;
  struct messages_list *tail;
  size_t nmsgs;
  unsigned int users;
};

// Инициализация списка процессов
void pids_init(void);
// Очистка списка процессов
void pids_uninit(void);
// Проверка,етсь ли место в списке процессов
int pids_full(void);
// поиск pid в списке зарегистрированных
int pid_nfind(int pid);
// Регистрация процесса
int pid_register(int pid);
// Отмена регистрации процесса
void pid_unregister(int pid);
// Получение структуры  со списком сообщений для конкретного процесса
struct pid_msg *get_pid_msg_list(int pid);
// удаляет первое сообщение из очереди. Вызывать при удержанном pid_msg.lock
void msg_drop_head(struct pid_msg *pidp);
// алоцирует память для нового сообщения
int msg_alloc(struct message **msg, size_t length);
// освобождает сообщение
void msg_free(struct message *msg);
// сопоставляет сообщение и обработчик данного сообщения
int msg_matching(struct message *msg, int pid);

#endif
