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
print("Client was started with pid", pid)
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

even_flags = {
    "read": select.POLLIN | select.POLLRDNORM,
    "write":
        select.POLLOUT | select.POLLWRNORM}


def read_ipc(fd, buffer_len):
    inBytes = os.read(fd, buffer_len)
    print("Byte array read:", inBytes)
    if len(inBytes) == headSize:
        format = 'ii'
    elif len(inBytes) > headSize:
        format = f'ii{len(inBytes)-headSize}s'
    else:
        raise Exception('Unknown message format')
    return struct.unpack(format, inBytes)


def write_ipc(fd, msg):
    numBytes = os.write(fd, msg)
    print("Byte array write:", msg)
    print("Number of bytes was written:", numBytes)
    return numBytes


def get_val(key):
    if key in base:
        return str(base[key])
    return "ERROR"


def pack_msg(msg, val):
    if val:
        format = f'ii{len(val)}s'
        return struct.pack(format, msg[0], msg[1], val.encode(encoding='UTF-8'))
    else:
        return struct.pack("ii", msg[0], msg[1])


def calcISize(bytes):
    return int(len(bytes)/intSize)


def io_func(func_arg, msg):
    def polling_cycle(fd, s, func, poll_event):
        poll = select.poll()
        poll.register(fd, poll_event)
        try:
            timer_stop = 10
            while True:
                events = poll.poll(2)
                for _, event in events:
                    if event == poll_event:
                        res = func(fd, s)
                        return res
                print("I'm waiting 2 seconds")
                time.sleep(2)
                timer_stop = timer_stop - 1
                if timer_stop == 0:
                    raise Exception('Response timed out')
        except Exception as e:
            print(e)
        return 0

    res = polling_cycle(ipc_fd, msg, func_arg["write"], even_flags["write"])
    if res:
        res = polling_cycle(ipc_fd, bufN, func_arg["read"], even_flags["read"])
        return res
    return None


if __name__ == "__main__":

    func = {
        "read": read_ipc,
        "write": write_ipc
    }
    pids = []
    # init
    out_msg = pack_msg([INIT, 0], " ")
    in_msg = io_func(func, out_msg)
    if in_msg and in_msg[0] == INIT_STATUS:
        format = f'{calcISize(in_msg[2])}i'
        pids_tmp = struct.unpack(format, in_msg[2])
        print("pids_tmp", pids_tmp)
        pids = [p for p in pids_tmp if p > 0 and p != pid]
        print("PIDs registered", pids)
    else:
        print("Unknown answer for INIT")
        exit

    if len(pids) > 0:
        for key in base:
            secure_random = random.SystemRandom()
            cur_pid = secure_random.choice(pids)
            print("cur pid serv ", cur_pid)
            out_msg = pack_msg([SEND, cur_pid], key)
            in_msg = io_func(func, out_msg)
            new_val = "?"
            if in_msg and in_msg[0] == SEND:
                new_val = in_msg[2].decode('UTF-8')
            else:
                print("Unknown answer for SEND")
            print(f"{key, base[key]} ?= {key, new_val}")
    else:
        print("There is not another pids for answers")

    os.close(ipc_fd)
