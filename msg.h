#ifndef __msg_h
#define __msg_h

#include <linux/kernel.h>
#include <linux/mutex.h>

// номера команд в протоколе сообщений
#define INIT 10
// инициализация, когда процесс хочет
// узнать номера процессов - соседей
#define INIT_STATUS 11
// ответ на собщение - инициализацию,
// содержит номра процессов - соседей
#define SEND 20
// непосредственно отправка сообщения

#define NPIDS 4
// число процессов, которые могут
// одновременно работать с драйвером
#define MSG_THEAD int
// тип данных в заголовке
#define MSG_TDATA char
// тип данных в теле сообщения

// статус завершения процедур
enum {
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
  struct messages_list *head;
};

// Инициализация списка процессов
void pids_init(void);
// Очистка списка процессов
void pids_uninit(void);
// Проверка,етсь ли место в списке процессов
int pids_full(void);
// Регистрация процесса
int pid_register(int pid);
// Отмена регистрации процесса
void pid_unregister(int pid);
// Получение структуры  со списком сообщений для конкретного процесса
struct pid_msg *get_pid_msg_list(int pid);
// отдает сообщение из хвоста списка (самое старое)
struct message *get_tail_msg(struct messages_list *head);
// очищает сообщение из хвоста списка
int clean_tail_msg(struct messages_list **head);
// алоцирует память для нового сообщения
int msg_alloc(struct message **msg, size_t length);
// сопоставляет сообщение и обработчик данного сообщения
int msg_matching(struct message *msg, int pid);

#endif
