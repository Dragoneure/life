#! /bin/bash

# example usage
#
# using random seeds
# ./run.sh
#
# using user seed for all tests
# ./run.sh 42

TEST_DEFAULT=1
TEST_SIMPLE=1
TEST_INSERT=1

BENCH_SIMPLE=1
BENCH_INSERT=1

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
if [ $TEST_DEFAULT -eq 1 ]; then
  /share/test.o "$seed"
fi
if [ $BENCH_SIMPLE -eq 1 ]; then
  /share/bench.o
fi

rm $TESTDIR/*
echo -e "\n\033[1mUsing simple read/write:\033[0m\n"

echo -n '1' > /sys/kernel/ouichefs/read_fn
echo -n '1' > /sys/kernel/ouichefs/write_fn
if [ ! -n "$1" ]; then
  seed=$((seed + 1))
fi
if [ $TEST_SIMPLE -eq 1 ]; then
  /share/test.o "$seed"
fi
if [ $BENCH_SIMPLE -eq 1 ]; then
  /share/bench.o
fi

rm $TESTDIR/*
echo -e "\n\033[1mUsing lite read/write:\033[0m\n"

echo -n '2' > /sys/kernel/ouichefs/read_fn
echo -n '2' > /sys/kernel/ouichefs/write_fn
if [ ! -n "$1" ]; then
  seed=$((seed + 1))
fi
if [ $TEST_INSERT -eq 1 ]; then
  /share/test_insert.o "$seed"
fi
if [ $BENCH_INSERT -eq 1 ]; then
  /share/bench_insert.o
fi

cd ~

# cleanup
rm $TESTDIR/*
umount $TESTDIR
rmdir $TESTDIR
rmmod ouichefs
