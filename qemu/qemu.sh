#!/bin/sh

# Creates initramfs
#cd ../user_space/ufs
#./mk_eus.sh
#cd -
#cp -v ../user_space/ufs/initramfs.igz ../bin/

# Moves bzImage to bin folder
#cp -v ../kernel/arch/x86/boot/bzImage ../bin/

# Calls qemu
qemu-system-x86_64 -enable-kvm \
-smp 2 \
 -hda ../bin/litmus.img \
-name "SLOT_SHIFT" -nographic -kernel ../bin/bzImage -initrd ../bin/initramfs.igz\
 -append "console=ttyS0,115200"

