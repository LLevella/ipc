import os
import struct
path = "/dev/ipcdev"
fd = os.open(path, os.O_NONBLOCK|os.O_RDWR)
numBytes = os.write(fd, struct.pack("ii", 10, 0))
pid = os.getpid()
print("Number of bytes written:", numBytes, "calsize", struct.calcsize('ii') , "pid", pid)
os.close(fd)