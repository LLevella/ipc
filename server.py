import json
import os
import select
import struct
import time
import random

ipc_path = "/dev/ipcdev"
bufN = 256
headSize = struct.calcsize('ii')
intSize = struct.calcsize('i')
pid = os.getpid()
print("Server was started with pid", pid)
pids = []
# номера команд в протоколе сообщений
INIT = int(10)
# инициализация, когда процесс хочет
# узнать номера процессов - соседей
INIT_STATUS = int(11)
# ответ на собщение - инициализацию,
# содержит номра процессов - соседей
SEND = int(20)
# непосредственно отправка сообщения
ipc_fd = os.open(ipc_path, os.O_NONBLOCK | os.O_RDWR)

with open("base.json", "r") as read_file:
    base = json.load(read_file)


def read_ipc(fd):
    inBytes = os.read(fd, bufN)
    print("Byte array read:", inBytes)
    if len(inBytes) == headSize:
        return struct.unpack("ii", inBytes)
    elif len(inBytes) > headSize:
        format = f'ii{len(inBytes)-headSize}s'
        return struct.unpack(format, inBytes)
    else:
        raise Exception('Unknown message format')


def get_val(key):
    print("key", key)
    if key in base:
        return str(base[key])
    return "ERROR"


def pack_msg(msg, val):
    if val:
        format = f'ii{len(val)}s'
        return struct.pack(format, msg[0], pid, val.encode(encoding='utf-8'))
    else:
        return struct.pack("ii", msg[0], msg[1])


def write_ipc(msg):
    numBytes = os.write(ipc_fd, msg)
    print("Byte array write:", msg)
    print("Number of bytes written:", numBytes)


def calcISize(bytes):
    return int(len(bytes)/intSize)


if __name__ == "__main__":

    msg_init = [INIT, 0]
    write_ipc(pack_msg(msg_init, "key-1"))

    poll = select.poll()
    poll.register(ipc_fd, select.POLLIN)

    try:
        while True:
            events = poll.poll(4)
            for fd, event in events:
                print("fd, event = ", fd, event)
                if event == select.POLLIN or event == select.POLLRDNORM:
                    msg = read_ipc(ipc_fd)
                    print("msg", msg)
                    if msg[0] == INIT_STATUS:
                        format = f'{calcISize(msg[2])}i'
                        pids_tmp = struct.unpack(format, msg[2])
                        print("pids_tmp", pids_tmp)
                        pids = [pid for pid in pids_tmp if pid > 0]
                        print("PIDs registered", pids)
                    elif msg[0] == SEND:
                        nbytes = write_ipc(
                            pack_msg((SEND, msg[1]), get_val(msg[2].decode())))
                    else:
                        raise Exception('Unknown command')
            print("I'm waiting 4 seconds")
            time.sleep(4)
    except Exception as e:
        print(e)

    os.close(ipc_fd)
