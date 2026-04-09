import os
import struct

from ipc_protocol import (INIT, INIT_STATUS, IPC_PATH, SEND, calc_i_size,
                          get_val, io_func, load_base, pack_msg, read_ipc,
                          write_ipc)


def main():
    pid = os.getpid()
    print("Server was started with pid", pid)
    ipc_fd = os.open(IPC_PATH, os.O_NONBLOCK | os.O_RDWR)
    try:
        base = load_base()
        func = {
            "read": read_ipc,
            "write": write_ipc
        }
        pids = []
        # init
        out_msg = pack_msg([INIT, 0], " ")
        in_msg = io_func(ipc_fd, func, out_msg)
        if in_msg and in_msg[0] == INIT_STATUS:
            msg_format = f'{calc_i_size(in_msg[2])}i'
            pids_tmp = struct.unpack(msg_format, in_msg[2])
            print("pids_tmp", pids_tmp)
            pids = [p for p in pids_tmp if p > 0 and p != pid]
            print("PIDs registered", pids)
        else:
            print("Unknown answer for INIT")
            raise SystemExit(1)

        # main cycle for server
        while True:
            in_msg = io_func(ipc_fd, {"read": func["read"]})
            if in_msg:
                if in_msg[0] == SEND:
                    key = in_msg[2].decode("UTF-8")
                    out_msg = pack_msg((SEND, in_msg[1]), get_val(base, key))
                    res = io_func(ipc_fd, {"write": func["write"]}, out_msg)
                    if res:
                        print("Success")
                else:
                    print("Unknown message")
    finally:
        os.close(ipc_fd)


if __name__ == "__main__":
    main()
