obj-m += ouichefs.o
ouichefs-objs := src/fs.o src/super.o src/inode.o src/file.o src/dir.o

KERNELDIR := $(shell grep -Po '^KERNELDIR=\K.*' .env 2> /dev/null)
BUILDDIR := build

define move_files
	mkdir -p share
	mkdir -p build
	cp ouichefs.ko share/ouichefs.ko
	mv .*.cmd src/.*.cmd *.o src/*.o *.ko *.mod \
	*.mod.c *.symvers *.order $(BUILDDIR)
endef

all:
	make -C $(KERNELDIR) M=$(PWD) modules
	$(call move_files)

check:
	./check/checkpatch.pl -f -q --no-tree src/*.c

debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules
	$(call move_files)

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~

.PHONY: all clean check
