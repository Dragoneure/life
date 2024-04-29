BIN ?= mkfs.ouichefs
IMG ?= test.img
IMGSIZE ?= 12500

all: ${BIN}

${BIN}: mkfs-ouichefs.c
	gcc -Wall -o $@ $<

img: ${BIN}
	rm -rf ${IMG}
	dd if=/dev/zero of=${IMG} bs=4KiB  count=${IMGSIZE}
	./${BIN} ${IMG}
	sync

clean:
	rm -rf *~

mrproper: clean
	rm -rf ${BIN}

.PHONY: all clean mrproper img
