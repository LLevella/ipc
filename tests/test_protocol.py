import json
import os
import struct
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from unittest.mock import call, patch

from ipc_protocol import (BUFFER_SIZE, EVENT_FLAGS, HEAD_FORMAT, INIT, SEND,
                          calc_i_size, get_val, io_func, load_base, pack_msg,
                          polling_cycle, read_ipc)


class FakePoll:
    def __init__(self, events):
        self.events = list(events)
        self.registered = None
        self.timeouts = []

    def register(self, fd, event):
        self.registered = (fd, event)

    def poll(self, timeout_ms):
        self.timeouts.append(timeout_ms)
        if self.events:
            return self.events.pop(0)
        return []


class ProtocolTest(unittest.TestCase):
    def test_pack_msg_without_body(self):
        self.assertEqual(pack_msg((INIT, 0), ""), struct.pack(HEAD_FORMAT, INIT, 0))

    def test_pack_msg_uses_utf8_byte_length(self):
        payload = "ключ"
        packed = pack_msg((SEND, 42), payload)
        msg_id, pid, body = struct.unpack(f"{HEAD_FORMAT}{len(payload.encode())}s",
                                          packed)

        self.assertEqual(msg_id, SEND)
        self.assertEqual(pid, 42)
        self.assertEqual(body.decode("utf-8"), payload)

    def test_read_ipc_unpacks_header_only_message(self):
        read_fd, write_fd = os.pipe()
        try:
            os.write(write_fd, pack_msg((INIT, 0), ""))
            os.close(write_fd)
            write_fd = None

            with redirect_stdout(StringIO()):
                self.assertEqual(read_ipc(read_fd, 4096), (INIT, 0))
        finally:
            os.close(read_fd)
            if write_fd is not None:
                os.close(write_fd)

    def test_read_ipc_rejects_short_message(self):
        read_fd, write_fd = os.pipe()
        try:
            os.write(write_fd, b"\x01\x02")
            os.close(write_fd)
            write_fd = None

            with redirect_stdout(StringIO()):
                with self.assertRaises(ValueError):
                    read_ipc(read_fd, 4096)
        finally:
            os.close(read_fd)
            if write_fd is not None:
                os.close(write_fd)

    def test_calc_i_size_requires_int_aligned_payload(self):
        self.assertEqual(calc_i_size(struct.pack("4i", 1, 2, 3, 4)), 4)
        with self.assertRaises(ValueError):
            calc_i_size(b"abc")

    def test_load_base_and_get_val(self):
        with tempfile.NamedTemporaryFile("w", encoding="utf-8") as base_file:
            json.dump({"key": 7}, base_file)
            base_file.flush()

            base = load_base(base_file.name)

        self.assertEqual(get_val(base, "key"), "7")
        self.assertEqual(get_val(base, "missing"), "ERROR")

    def test_entrypoints_are_importable_without_device(self):
        __import__("client")
        __import__("server")

    def test_polling_cycle_returns_on_matching_event(self):
        poller = FakePoll([
            [],
            [(123, EVENT_FLAGS["read"])],
        ])

        def reader(fd, arg):
            return fd, arg

        with patch("ipc_protocol.select.poll", return_value=poller):
            with redirect_stdout(StringIO()):
                result = polling_cycle(123, BUFFER_SIZE, reader,
                                       EVENT_FLAGS["read"], timeout_ms=1,
                                       attempts=2)

        self.assertEqual(result, (123, BUFFER_SIZE))
        self.assertEqual(poller.registered, (123, EVENT_FLAGS["read"]))
        self.assertEqual(poller.timeouts, [1, 1])

    def test_polling_cycle_times_out_without_matching_event(self):
        poller = FakePoll([
            [(123, EVENT_FLAGS["write"])],
            [],
        ])

        with patch("ipc_protocol.select.poll", return_value=poller):
            with redirect_stdout(StringIO()):
                with self.assertRaises(TimeoutError):
                    polling_cycle(123, BUFFER_SIZE, lambda *_: None,
                                  EVENT_FLAGS["read"], timeout_ms=1,
                                  attempts=2)

    def test_io_func_writes_then_reads(self):
        def writer(_fd, _msg):
            return 8

        def reader(_fd, _size):
            return (SEND, 42, b"value")

        with patch("ipc_protocol.polling_cycle",
                   side_effect=[8, (SEND, 42, b"value")]) as poll_mock:
            result = io_func(123, {"write": writer, "read": reader}, b"payload")

        self.assertEqual(result, (SEND, 42, b"value"))
        self.assertEqual(
            poll_mock.call_args_list,
            [
                call(123, b"payload", writer, EVENT_FLAGS["write"]),
                call(123, BUFFER_SIZE, reader, EVENT_FLAGS["read"]),
            ],
        )

    def test_io_func_returns_zero_on_polling_error(self):
        with patch("ipc_protocol.polling_cycle",
                   side_effect=TimeoutError("Response timed out")):
            with redirect_stdout(StringIO()):
                result = io_func(123, {"read": lambda *_: None})

        self.assertEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
