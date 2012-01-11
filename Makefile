#
# Partitions Filesystem
#
# Copyright (c) 2012 Andrea Bonomi (andrea.bonomi@gmail.com)
#
# This program/include file is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program/include file is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program (in the main directory of the Linux-NTFS
# distribution in the file COPYING); if not, write to the Free Software
# Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

obj-m += pfs.o
pfs-objs += partsfs.o
pfs-objs += partitions/check.o
pfs-objs += partitions/acorn.o
pfs-objs += partitions/amiga.o
pfs-objs += partitions/atari.o
pfs-objs += partitions/mac.o
pfs-objs += partitions/ldm.o
pfs-objs += partitions/msdos.o
pfs-objs += partitions/osf.o
pfs-objs += partitions/sgi.o
pfs-objs += partitions/sun.o
pfs-objs += partitions/ultrix.o
pfs-objs += partitions/efi.o
pfs-objs += partitions/karma.o
pfs-objs += partitions/sysv68.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

