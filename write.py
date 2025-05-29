import os
path = "/dev/ipcdev"
fd = os.open(path, os.O_NONBLOCK|os.O_RDWR)
s = "100"
line = str.encode(s)
numBytes = os.write(fd, line)
pid = os.getpid()
print("Number of bytes written:", numBytes, "pid", pid)
os.close(fd)