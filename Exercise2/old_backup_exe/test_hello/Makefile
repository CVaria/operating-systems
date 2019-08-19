CC = gcc

obj-m	:= crashl.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
USER_CFLAGS = -Wall -Werror 

PWD	:= $(shell pwd)

#all:	modules crashl

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c
crashl: crashl.c
	$(CC) $(USER_CFLAGS) -o $@ crashl.c
