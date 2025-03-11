#!/usr/bin/bash
for i in tests/*.c;
do
	cc $i lib.c -I. -o $i.test
	if [ $? -ne 0 ]; then
		echo "Failed to compile $i"
		exit 1
	fi
done

for i in tests/*.test;
do
	$i
	if [ $? -ne 0 ]; then
		echo "Failed to run test $i"
		exit 1
	fi
done
