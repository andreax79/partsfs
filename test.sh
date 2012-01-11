#!/bin/sh -
DISK_IMAGE=50M.dsk
NEW_DISK_IMAGE=0

# Make sure only root can run this script
if [ "$(id -u)" != "0" ]; then
   echo "Please run this script as root" 1>&2
   exit 1
fi

if [ ! -f $DISK_IMAGE ]; then
  echo "Creating the disk image..."
  dd if=/dev/zero of=$DISK_IMAGE bs=1024 count=50000
  parted $DISK_IMAGE mklabel msdos
  parted $DISK_IMAGE mkpart primary ext2 1 10
  parted $DISK_IMAGE mkpart primary fat16 10 20
  parted $DISK_IMAGE mkpart primary ext2 20 50
  parted $DISK_IMAGE print
  NEW_DISK_IMAGE=1
fi

mkdir t
insmod pfs.ko
mount -o loop -t partsfs $DISK_IMAGE t
ls -laih t

if [ "$NEW_DISK_IMAGE" == "1" ]; then
  echo "Creating filesystems..."
  echo y | mkfs.ext2 t/1
  echo y | mkfs.msdos t/2
  echo y | mkfs.ext2 t/3
fi

mkdir t1
mkdir t2
mkdir t3

echo "Mounting partitions"
mount -o loop t/1 t1
mount -o loop t/2 t2
mount -o loop t/3 t3

echo "Copy some files on mounted partitions"
cp /bin/bash t1
cp /bin/bash t2
cp /bin/bash t3

echo "Partition 1:"
ls -la t1

echo "Partition 2:"
ls -la t2

echo "Partition 3:"
ls -la t3

echo "Umounting partitions"
umount t1
umount t2
umount t3
rmdir t1
rmdir t2
rmdir t3

umount t
rmdir t

rmmod pfs.ko
