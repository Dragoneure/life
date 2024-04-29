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
/share/test.o
/share/bench.o
cd ~

# cleanup
umount $TESTDIR
rm -rf $TESTDIR
rmmod ouichefs
