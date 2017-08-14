#!/bin/sh

qemu-system-x86_64 -hda linux.img -m 1024 -enable-kvm -smp 2 -kernel bzImage -append "console=ttyS0,115220 root=/dev/sda1"


