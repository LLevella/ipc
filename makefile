LDDINC=$(PWD)/../include
EXTRA_CFLAGS += -O2
EXTRA_CFLAGS += -I$(LDDINC)

ifneq ($(KERNELRELEASE),)
# call from kernel build system

ipcdev-objs := msg.o ipc.o

obj-m	:= ipcdev.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

ipcdev: clean
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif



clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif