#! /bin/bash

# load ouichefs
insmod /share/ouichefs.ko

# mount a ouichefs test directory
TESTDIR=~/tests-ouichefs
if [ ! -d "$TESTDIR" ]; then
  mkdir $TESTDIR
  mount /share/test.img $TESTDIR
fi

# run the user executables
cd $TESTDIR

echo -e "\n\033[1mUsing kernel default read:\033[0m\n"

echo -n '0' > /sys/kernel/ouichefs/read_fn
/share/test.o
/share/bench.o

echo -e "\n\033[1mUsing simple read:\033[0m\n"

echo -n '1' > /sys/kernel/ouichefs/read_fn
/share/test.o
/share/bench.o

cd ~

# cleanup
umount $TESTDIR
rm -rf $TESTDIR
rmmod ouichefs
