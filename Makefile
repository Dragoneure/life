obj-m += ouichefs.o
ouichefs-objs := src/fs.o src/super.o src/inode.o src/file.o \
	src/dir.o src/read.o src/write.o src/defrag.o src/ioctl.o

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

user: test test_insert bench

test:
	$(CC) -static src/user/test.c -o $(SHAREDIR)/test.o 
test_insert:
	$(CC) -static src/user/test_insert.c -o $(SHAREDIR)/test_insert.o 
bench:
	$(CC) -static src/user/bench.c -o $(SHAREDIR)/bench.o 

scripts:
	# copy scripts
	cp scripts/run.sh $(SHAREDIR)

img:
	make -C mkfs img

check:
	./check/checkpatch.pl -f -q --no-tree src/*.c

debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules
	$(call move_files)

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~

.PHONY: all clean check scripts
