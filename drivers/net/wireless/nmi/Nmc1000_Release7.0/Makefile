#ARCH = arm
ARCH = x86
MAKE = make
#CC	=  arm-linux-gcc
#AR	=  arm-linux-ar
RM	= rm
MV 	= mv
ECHO	= echo
CP 	= cp
PWD 	= $(shell pwd)


KERNELDIR ?= /lib/modules/$(shell uname -r)/build

all:	
	$(MAKE) -C ./driver/linux


clean:
	$(MAKE) -C ./driver/linux clean
