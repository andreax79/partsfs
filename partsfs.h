/**
 * Partitions Filesystem
 *
 * Copyright (c) 2012 Andrea Bonomi (andrea.bonomi@gmail.com)
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/fs.h>


#define PARTSFS_MAGIC                 0x1979
#define PARTSFS_ROOT_DIR_INODE             1
#define PARTSFS_FIRST_PARTITION_INODE    100
#define PARTSFS_MAX_NAME_LENGTH           16
#define PARTSFS_DEFAULT_DIR_MODE        0555
#define PARTSFS_DEFAULT_FILE_MODE       0600

extern struct parsed_partitions *check_partition(struct gendisk *hd,
                        struct block_device *bdev);

static struct inode *partsfs_get_inode(struct super_block *sb, ino_t s_ino);

static int partsfs_readdir(struct file *filp, void *dirent, filldir_t filldir);

static struct dentry *partsfs_lookup(struct inode *dir, struct dentry *dentry,
                        struct nameidata *nd);

static int partsfs_writepage(struct page *page, struct writeback_control *wbc);

static int partsfs_readpage(struct file *file, struct page *page);

static int partsfs_write_begin(struct file *file, struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned flags,
                        struct page **pagep, void **fsdata);

static sector_t partsfs_bmap(struct address_space *mapping, sector_t block);

static int partsfs_statfs(struct dentry *dentry, struct kstatfs *buf);

static struct dentry *partsfs_mount(struct file_system_type *fs_type,
                        int flags, const char *dev_name, void *data);

static void partsfs_kill_sb(struct super_block *sb);

static int partsfs_show_options(struct seq_file *seq, struct vfsmount *mnt);


static const struct file_operations partsfs_dir_operations = {
        .read             = generic_read_dir,
        .readdir          = partsfs_readdir,
        .fsync            = generic_file_fsync,
        .llseek           = generic_file_llseek,
};

static const struct inode_operations partsfs_dir_inode_operations = {
        .lookup           = partsfs_lookup,
};

static const struct file_operations partsfs_file_operations = {
        .llseek           = generic_file_llseek,
        .read             = do_sync_read,
        .write            = do_sync_write,
        .aio_read         = generic_file_aio_read,
        .aio_write        = generic_file_aio_write,
        .mmap             = generic_file_mmap,
        .splice_read      = generic_file_splice_read
};

static const struct address_space_operations partsfs_file_aops = {
        .readpage         = partsfs_readpage,
        .writepage        = partsfs_writepage,
        .write_begin      = partsfs_write_begin,
        .write_end        = generic_write_end,
        .bmap             = partsfs_bmap,
};

static const struct super_operations partsfs_super_ops = {
        .statfs           = partsfs_statfs,
        .show_options     = partsfs_show_options,
};

static struct file_system_type partsfs_fs_type = {
        .owner           = THIS_MODULE,
        .name            = "partsfs",
        .mount           = partsfs_mount,
        .kill_sb         = partsfs_kill_sb,
        .fs_flags        = FS_REQUIRES_DEV, /* can only be mounted on a block device */
};

/*
 * Partitions Filesystem Info
 */
struct partsfs_state {
        struct {
                sector_t from;    /* Partition starting position */
                sector_t size;    /* Partition size, in sectors */
        } parts[DISK_MAX_PARTS];
        int number_of_partitions; /* Number of partitions */
        int last_partition;       /* Last partition */
        sector_t sector_size;     /* Sector size */
        sector_t capacity;        /* The capacity of this drive, in 512-byte sectors */
        /* Mount options */
        uid_t option_uid;         /* The uid of all files */
        gid_t option_gid;         /* The gid of all files */
        umode_t option_mode;      /* The mode of all files */
};

static int parse_options(char *options, struct partsfs_state *state);
