CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)
obj-m := ipc.o 
ipc-objs := msg.o ipc.o

ipc: clean
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
	@rm -f *.o .*.cmd .*.flags *.mod.c *.order *.symvers