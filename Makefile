obj-m += ouichefs.o
ouichefs-objs := fs.o super.o inode.o file.o dir.o

KERNELDIR := $(shell grep -Po '^KERNELDIR=\K.*' .env)

all:
	make -C $(KERNELDIR) M=$(PWD) modules
	mkdir -p share
	cp ouichefs.ko share/ouichefs.ko

check:
	./check/checkpatch.pl -f -q --no-tree *.c

debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules
	mkdir share
	cp ouichefs.ko share/ouichefs.ko

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~

.PHONY: all clean check
