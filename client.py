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


def pack_msg(msg, val, respid=pid):
    if val:
        format = f'ii{len(val)}s'
        return struct.pack(format, msg[0], msg[1], val.encode(encoding='UTF-8'))
    else:
        return struct.pack("ii", msg[0], msg[1])


def write_ipc(msg):
    numBytes = os.write(ipc_fd, msg)
    print("Byte array write:", msg)
    print("Number of bytes written:", numBytes)


def calcISize(bytes):
    return int(len(bytes)/intSize)


def get_answer():
    poll = select.poll()
    poll.register(ipc_fd, select.POLLIN)
    try:
        timer_stop = 60
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
                        pids = [p for p in pids_tmp if p > 0 and p != pid]
                        print("PIDs registered", pids)
                        return pids
                    elif msg[0] == SEND:
                        return msg[2].decode('UTF-8')
                    else:
                        raise Exception('Unknown command')
            print("I'm waiting 4 seconds")
            time.sleep(4)
            timer_stop = timer_stop - 4
            if timer_stop < 0:
                raise Exception('Response timed out')
    except Exception as e:
        print(e)
    return ""


if __name__ == "__main__":

    msg_head = [INIT, 0]
    write_ipc(pack_msg(msg_head, "0"))
    pids = get_answer()
    if len(pids) > 0:
        print("ok", pids)
        for key in base:
            msg_head = [SEND, pids[0]]
            write_ipc(pack_msg(msg_head, key, pids[0]))
            new_val = get_answer()
            print(f"{key, base[key]} ?= {key, new_val}")

    os.close(ipc_fd)
