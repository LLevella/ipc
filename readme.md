# ipcdev

`ipcdev` - простой Linux kernel module с character device `/dev/ipcdev`.
Модуль регистрирует процессы по PID при открытии устройства и позволяет им
обмениваться сообщениями через небольшой PID-addressed IPC-протокол.

В репозитории есть:

- `ipc.c`, `msg.c`, `ipc.h`, `msg.h` - kernel module.
- `ipc_protocol.py` - общий Python-код протокола.
- `server.py` - процесс, который отвечает значениями из `base.json`.
- `client.py` - процесс, который запрашивает значения у зарегистрированных
  соседних процессов.
- `tests/` - unit/smoke проверки.
- `.github/workflows/ci.yml` - CI/CD pipeline.

## Требования

Для сборки модуля нужны kernel headers для текущего или выбранного ядра.
На Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y build-essential linux-headers-$(uname -r)
```

Если headers для текущего ядра недоступны, можно указать путь вручную:

```bash
make KERNELDIR=/path/to/linux-headers
```

## Сборка

```bash
make
```

Строгая сборка с дополнительными предупреждениями kernel build system:

```bash
make W=1
```

Очистка build-артефактов:

```bash
make clean
```

## Тесты

Unit-тесты проверяют Python-протокол без загрузки kernel module:

```bash
make test
```

Smoke-проверка компилирует Python entrypoints, запускает unit-тесты, затем
собирает kernel module с `W=1` и очищает артефакты:

```bash
tests/smoke.sh
```

Если `KERNELDIR` не существует, smoke-скрипт пропускает kernel build:

```bash
KERNELDIR=/path/to/linux-headers tests/smoke.sh
```

## Запуск вручную

Соберите модуль:

```bash
make
```

Загрузите его:

```bash
sudo insmod ipcdev.ko
```

Проверьте, что устройство создано:

```bash
ls -l /dev/ipcdev
```

Если нужны права для обычного пользователя:

```bash
sudo chmod 666 /dev/ipcdev
```

Запустите сервер в одном терминале:

```bash
python3 server.py
```

Запустите клиент в другом терминале:

```bash
python3 client.py
```

Выгрузка модуля:

```bash
sudo rmmod ipcdev
```

## Протокол

Заголовок каждого сообщения:

```c
struct msg_head {
  int msg_id;
  int pid;
} __attribute__((packed));
```

Команды:

- `INIT = 10` - процесс запрашивает список зарегистрированных PID.
- `INIT_STATUS = 11` - ответ со snapshot списка PID.
- `SEND = 20` - отправка пользовательского payload конкретному PID.

Ограничения драйвера:

- `NPIDS = 4` - максимум зарегистрированных процессов.
- `MAXNMSG = 16` - максимум сообщений в очереди одного процесса.
- `MAXMSGSIZE = 4096` - максимум байт в одном сообщении.

## CI/CD

GitHub Actions workflow запускается на `push`, `pull_request` и вручную через
`workflow_dispatch`.

Pipeline состоит из двух job:

- `Python protocol tests` - запускает `make test`.
- `Kernel module build` - ставит build dependencies, находит generic kernel
  headers, собирает модуль через `make W=1`.

На `push` и tag `v*` workflow публикует собранный `ipcdev.ko` как GitHub
Actions artifact. На pull request артефакт не публикуется.

## Историческая среда

Первоначально проект собирался на:

```text
Linux vbox 5.15.0-58-generic #64-Ubuntu SMP Thu Jan 5 11:43:13 UTC 2023 x86_64
```

Текущая сборка также проверена через headers ядра `6.17.0-23-generic`.
