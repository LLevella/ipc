import os
path = "/dev/ipcdev"
fd = os.open(path, os.O_RDONLY)
n = 128
readBytes = os.read(fd, n)
pid = os.getpid()
print("bytes read:", readBytes, "pid", pid)
s = readBytes.decode('UTF-8')
print("I read:", s, "pid", pid)
os.close(fd)