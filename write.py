import os
path = "/dev/ipcdev"
fd = os.open(path, os.O_WRONLY)
s = "hello world"
line = str.encode(s)
numBytes = os.write(fd, line)
pid = os.getpid()
print("Number of bytes written:", numBytes, "pid", pid)
os.close(fd)