#! /bin/sh

# Sets the scheduler to P-RES and shows the scheduler in the screen
#set -v

echo "Will change the scheduler to P-RES"
setsched P-RES

echo "Current scheduler is: "
showsched

# Creates a new reservation with id 1234 in core 0, with period 1000.
echo "Will create reservation..."
resctrl -n 1234 -t table-driven -m 1000 -c 0 [50,300]
echo "Reservation created"

# Creates three tasks to run in the reservation
echo "Will create tasks"
rtspin -p 0 -r 1234 10 50 10 &
#rtspin -p 0 -r 1234 10 50 10 &
#rtspin -p 0 -r 1234 10 50 10 &
echo "Tasks created"

# Call the feather-trace tools on core 0
echo "Will start tracing"
ftcat /dev/litmus!ft_cpu_trace0 CXS_START CXS_END > ./cpu0_trace.txt &
ftcat /dev/litmus!ft_msg_trace0 SEND_RESCHED_START SEND_RESCHED_END > ./msg0_trace.txt &
#st_trace sch_trace
echo "Tracing finished"

