ccflags-y += -O2

ifneq ($(KERNELRELEASE),)

ipcdev-objs := msg.o ipc.o
obj-m := ipcdev.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)

.PHONY: all ipcdev clean depend test

all: ipcdev

ipcdev:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	$(RM) -r *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers

test:
	python3 -m py_compile client.py server.py ipc_protocol.py
	python3 -m unittest discover -s tests -p 'test_*.py'

depend .depend dep:
	$(CC) $(ccflags-y) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

endif
