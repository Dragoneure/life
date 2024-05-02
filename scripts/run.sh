#! /bin/bash

# example usage
#
# using random seeds
# ./run.sh
#
# using user seed for all tests
# ./run.sh 42

# load ouichefs
insmod /share/ouichefs.ko

# mount a ouichefs test directory
TESTDIR=~/tests-ouichefs
if [ ! -d "$TESTDIR" ]; then
  mkdir $TESTDIR
fi
mount /share/test.img $TESTDIR

# run the user executables
cd $TESTDIR

# use the provided seed or generate a new one
seed=$1
if [ ! -n "$seed" ]; then
  seed=$(date +%s)
fi
  
echo -e "\n\033[1mUsing kernel default read/write:\033[0m\n"

echo -n '0' > /sys/kernel/ouichefs/read_fn
echo -n '0' > /sys/kernel/ouichefs/write_fn
/share/test.o "$seed"
/share/bench.o

rm $TESTDIR/*
echo -e "\n\033[1mUsing simple read/write:\033[0m\n"

echo -n '1' > /sys/kernel/ouichefs/read_fn
echo -n '1' > /sys/kernel/ouichefs/write_fn
if [ ! -n "$1" ]; then
  seed=$((seed + 1))
fi
/share/test.o "$seed"
/share/bench.o

cd ~

# cleanup
umount $TESTDIR
rm -rf $TESTDIR
rmmod ouichefs
