import json
import os
import select
import struct
from pathlib import Path

IPC_PATH = "/dev/ipcdev"
BUFFER_SIZE = 4096
HEAD_FORMAT = "ii"
HEAD_SIZE = struct.calcsize(HEAD_FORMAT)
INT_SIZE = struct.calcsize("i")
BASE_PATH = Path(__file__).with_name("base.json")

INIT = 10
INIT_STATUS = 11
SEND = 20

EVENT_FLAGS = {
    "read": select.POLLIN | select.POLLRDNORM,
    "write": select.POLLOUT | select.POLLWRNORM,
}


def load_base(path=BASE_PATH):
    with open(path, "r", encoding="utf-8") as read_file:
        return json.load(read_file)


def read_ipc(fd, buffer_len=BUFFER_SIZE):
    in_bytes = os.read(fd, buffer_len)
    print("Byte array read:", in_bytes)
    if len(in_bytes) == HEAD_SIZE:
        msg_format = HEAD_FORMAT
    elif len(in_bytes) > HEAD_SIZE:
        msg_format = f"{HEAD_FORMAT}{len(in_bytes) - HEAD_SIZE}s"
    else:
        raise ValueError("Unknown message format")
    return struct.unpack(msg_format, in_bytes)


def write_ipc(fd, msg):
    num_bytes = os.write(fd, msg)
    print("Byte array write:", msg)
    print("Number of bytes was written:", num_bytes)
    return num_bytes


def get_val(base, key):
    if key in base:
        return str(base[key])
    return "ERROR"


def pack_msg(msg, val):
    if val:
        data = val.encode(encoding="UTF-8")
        msg_format = f"{HEAD_FORMAT}{len(data)}s"
        return struct.pack(msg_format, msg[0], msg[1], data)
    return struct.pack(HEAD_FORMAT, msg[0], msg[1])


def calc_i_size(data):
    if len(data) % INT_SIZE != 0:
        raise ValueError("Integer payload has invalid size")
    return len(data) // INT_SIZE


def polling_cycle(fd, arg, func, poll_event, timeout_ms=2000, attempts=10):
    poller = select.poll()
    poller.register(fd, poll_event)

    for _ in range(attempts):
        events = poller.poll(timeout_ms)
        for _, event in events:
            if event & poll_event:
                return func(fd, arg)
        print("I'm waiting 2 seconds")

    raise TimeoutError("Response timed out")


def io_func(fd, func_arg, msg=b""):
    try:
        if "write" in func_arg and "read" in func_arg:
            res = polling_cycle(fd, msg, func_arg["write"], EVENT_FLAGS["write"])
            if res:
                return polling_cycle(fd, BUFFER_SIZE, func_arg["read"],
                                     EVENT_FLAGS["read"])
        elif "write" in func_arg:
            return polling_cycle(fd, msg, func_arg["write"], EVENT_FLAGS["write"])
        elif "read" in func_arg:
            return polling_cycle(fd, BUFFER_SIZE, func_arg["read"],
                                 EVENT_FLAGS["read"])
    except Exception as exc:
        print(exc)
        return 0

    return None
