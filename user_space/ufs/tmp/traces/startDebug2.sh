#! /bin/sh

echo "Will change the scheduler to P-RES"
setsched P-RES
showsched

echo "Will create a new reservation"
resctrl -n 1234 -t table-driven -m 1000 -c 0 [50,300]

echo "Will create new tasks"
rtspin -p 0 -r 1234 10 50 10 &
rtspin -p 0 -r 1234 10 50 10 &
rtspin -p 0 -r 1234 10 50 10 &

