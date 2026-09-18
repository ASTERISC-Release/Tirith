obj-m += kmod.o
KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	sudo rmmod kmod || true
	make -C $(KDIR) M=$(PWD) clean