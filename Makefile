obj-m += ouichefs.o
ouichefs-objs := src/fs.o src/super.o src/inode.o src/file.o src/dir.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
ENV_KERNELDIR := $(shell grep -Po '^KERNELDIR=\K.*' .env 2> /dev/null)
ifdef ENV_KERNELDIR	
	KERNELDIR := $(ENV_KERNELDIR)
endif


BUILDDIR := build
SHAREDIR := share

define move_files
	mkdir -p $(SHAREDIR)
	mkdir -p $(BUILDDIR)
	cp ouichefs.ko $(SHAREDIR)/ouichefs.ko
	mv .*.cmd src/.*.cmd *.o src/*.o *.ko *.mod \
	*.mod.c *.symvers *.order $(BUILDDIR)
endef

all: module user scripts

module:
	# module
	make -C $(KERNELDIR) M=$(PWD) modules
	$(call move_files)

user:
	# user files
	$(CC) -static src/user/benchmark.c -o $(SHAREDIR)/benchmark.o 

scripts:
	# copy scripts
	cp scripts/run.sh $(SHAREDIR)

check:
	./check/checkpatch.pl -f -q --no-tree src/*.c

debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules
	$(call move_files)

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~

.PHONY: all clean check scripts
