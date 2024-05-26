#! /bin/bash

# example usage
#
# using random seeds
# ./run.sh
#
# using user seed for all tests
# ./run.sh 42

TEST_DEFAULT=0
TEST_SIMPLE=0
TEST_INSERT=0
TEST_CACHED=0

BENCH_SIMPLE=0
BENCH_INSERT=0

RUN_DEMO=1

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

rm $TESTDIR/*
if [ "$RUN_DEMO" -eq 1 ]; then
  /share/demo.o
fi

# some scripts
echo -e "\n\033[1mVisual tests using scripts with lite read/write:\033[0m"
touch bloup
echo -n "Hello " > bloup
echo "World" >> bloup
cat bloup
rm bloup

echo -e "\n\033[1mUsing lite read with page cache:\033[0m\n"
echo -n '3' > /sys/kernel/ouichefs/read_fn
if [ ! -n "$1" ]; then
  seed=$((seed + 1))
fi
if [ $TEST_CACHED -eq 1 ]; then
  /share/test_cached.o "$seed"
fi

cd ~

# cleanup
rm $TESTDIR/*
umount $TESTDIR
rmdir $TESTDIR
rmmod ouichefs
