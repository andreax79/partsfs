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

#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/parser.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/statfs.h>
#include <linux/pagemap.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>

#include "partitions/check.h"
#include "partsfs.h"


/**
 * Check if the partition number is the the range
 */
static inline int check_partition_range(int partition_number, struct super_block *sb) {
        struct partsfs_state *state = (struct partsfs_state *) sb->s_fs_info;
        return (partition_number > 0) && (partition_number <= state->number_of_partitions);
}

/**
 * Convert a inode number into a partition number
 * Returns -1 in the inode_number does not correspond to a partition
 */
static inline int inode_number_to_partition(ino_t inode_number, struct super_block *sb) {
        int partition_number = inode_number - PARTSFS_FIRST_PARTITION_INODE;
        if (check_partition_range(partition_number, sb))
                return partition_number;
        else
                return -1;
}

/**
 * Convert a partition number into an inode number
 */
static inline int partition_to_inode_number(int partition_number, struct super_block *sb) {
        return partition_number + PARTSFS_FIRST_PARTITION_INODE;
}

/*
 * Returns the root directory entries
 */
static int partsfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
        struct dentry *dentry = filp->f_path.dentry;
        struct super_block *sb = dentry->d_sb;
        struct partsfs_state *state = (struct partsfs_state *) sb->s_fs_info;
        struct inode *inode = dentry->d_inode;
        char name[PARTSFS_MAX_NAME_LENGTH];
        int i;

        switch (filp->f_pos) {
                case 0:
                        if (filldir(dirent, ".", 1, filp->f_pos,
                                    inode->i_ino, DT_DIR) < 0)
                                return 0;
                        filp->f_pos++;
                case 1:
                        if (filldir(dirent, "..", 2, filp->f_pos,
                                    parent_ino(dentry), DT_DIR) < 0)
                                return 0;
                        filp->f_pos++;
                default:
                        for (i=filp->f_pos-1; i<=state->number_of_partitions; i++) {
                                snprintf(name, sizeof(name), "%d", i);
                                if (filldir(dirent, name, strlen(name), filp->f_pos,
                                            i+PARTSFS_FIRST_PARTITION_INODE, DT_REG) < 0)
                                        return 0;
                                filp->f_pos++;
                        }
        }
        return 0;
}

/*
 * Look up an entry in the root directory
 */
static struct dentry *partsfs_lookup(struct inode *dir, struct dentry *dentry,
                                 struct nameidata *nd)
{
        struct super_block *sb = dentry->d_sb;
        int partition_number = simple_strtol(dentry->d_name.name, NULL, 0);

        if (check_partition_range(partition_number, sb)) {
            int inode_number = partition_to_inode_number(partition_number, sb);
                struct inode *inode = partsfs_get_inode(sb, inode_number);
                d_add(dentry, inode);
                return ERR_PTR(0);
        } else {
                return ERR_PTR(-ENOENT);
        }
}

/*
 * Map a file position (iblock) to a disk offset (passed back in bh_result)
 */
static int get_block(struct inode *inode, sector_t iblock,
                     struct buffer_head *bh_result, int create)
{
        struct partsfs_state *state = (struct partsfs_state *) inode->i_sb->s_fs_info;
        loff_t disk_offset;

        /* Get the partition number */
        int partition_number = inode_number_to_partition(inode->i_ino, inode->i_sb);
        if (partition_number < 0)
                return -ENOENT;

        /* Check the offset */
        if (create && (iblock >= state->parts[partition_number].size))
                return -ENOSPC; /* No space left on device */

        if ((iblock < 0) || (iblock >= state->parts[partition_number].size))
                return -ESPIPE; /* Illegal seek */

        /* Get the disk offset */
        disk_offset = state->parts[partition_number].from + iblock;
        map_bh(bh_result, inode->i_sb, disk_offset);
        return 0;
}

static int partsfs_writepage(struct page *page, struct writeback_control *wbc)
{
        return block_write_full_page(page, get_block, wbc);
}

static int partsfs_readpage(struct file *file, struct page *page)
{
        return block_read_full_page(page, get_block);
}

static sector_t partsfs_bmap(struct address_space *mapping, sector_t block)
{
        return generic_block_bmap(mapping, block, get_block);
}

static int partsfs_write_begin(struct file *file, struct address_space *mapping,
                           loff_t pos, unsigned len, unsigned flags,
                           struct page **pagep, void **fsdata)
{
        int ret = block_write_begin(mapping, pos, len, flags, pagep, get_block);
        if (unlikely(ret)) {
                loff_t isize = mapping->host->i_size;
                if (pos + len > isize)
                        vmtruncate(mapping->host, isize);
        }
        return ret;
}

static void partsfs_init_inode(struct inode *inode, struct super_block *sb, ino_t inode_number)
{
        struct partsfs_state *state = (struct partsfs_state *) sb->s_fs_info;
        inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
        inode->i_uid = 0;
        inode->i_gid = 0;

        if (inode_number == PARTSFS_ROOT_DIR_INODE) { /* root directory */
                /* set_nlink(inode, state->number_of_partitions + 1); */
                inode->i_nlink = state->number_of_partitions + 1;
                inode->i_size = 0;
                inode->i_mode = S_IFDIR | 0700;
                inode->i_op = &partsfs_dir_inode_operations;
                inode->i_fop = &partsfs_dir_operations;
        } else { /* partition file */
                int partition_number = inode_number_to_partition(inode_number, sb);
                /* set_nlink(inode, 1); */
                inode->i_nlink = 1;
                inode->i_size = state->parts[partition_number].size * state->sector_size;
                inode->i_mode = S_IFREG | 0600;
                inode->i_fop = &partsfs_file_operations;
                inode->i_data.a_ops = &partsfs_file_aops;
        }

        unlock_new_inode(inode);
}

static struct inode *partsfs_get_inode(struct super_block *sb, ino_t inode_number)
{
        struct inode *inode = iget_locked(sb, inode_number);
        if (inode && (inode->i_state & I_NEW))
                partsfs_init_inode(inode, sb, inode_number);
        return inode;
}

/*
 * Get filesystem statistics
 */
static int partsfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
        struct super_block *sb = dentry->d_sb;
        struct partsfs_state *state = (struct partsfs_state *) sb->s_fs_info;
        u64 id = huge_encode_dev(sb->s_bdev->bd_dev);

        buf->f_type = PARTSFS_MAGIC;
        buf->f_namelen = PARTSFS_MAX_NAME_LENGTH;
        buf->f_bsize = state->sector_size;
        buf->f_bfree = buf->f_bavail = buf->f_ffree = 0;
        buf->f_blocks = state->capacity;
        buf->f_files = state->number_of_partitions + 1;
        buf->f_fsid.val[0] = (u32)id;
        buf->f_fsid.val[1] = (u32)(id >> 32);
        return 0;
}

/*
 * Get partitioning information
 */
static struct partsfs_state *get_partitions_info(struct super_block *sb, int silent) {
        struct parsed_partitions *partitions;
        struct gendisk *disk;
        struct partsfs_state *state;
        int partno;
        int p;

        disk = get_gendisk(sb->s_bdev->bd_dev, &partno);
        if (!disk) {
                if (!silent)
                        printk(KERN_WARNING "PARTSFS: Error getting partition information (get_gendisk failed)\n");
                return NULL;
        }

        state = kzalloc(sizeof(struct partsfs_state), GFP_KERNEL);
        partitions = check_partition(disk, sb->s_bdev);
        if (IS_ERR(partitions) || partitions == NULL) {
                if (!silent)
                        printk(KERN_WARNING "PARTSFS: Error getting partition information (check_partition failed)\n");
                return NULL;
        }

        state->sector_size = bdev_logical_block_size(sb->s_bdev);
        state->capacity = get_capacity(disk);
        sb_set_blocksize(sb, state->sector_size);
        put_disk(disk);

        /* Count the partitions */
        state->number_of_partitions = 0;
        for (p = 1; p < partitions->limit; p++) {
                if (partitions->parts[p].from != 0 || partitions->parts[p].size != 0) {
                        if (!silent)
                                printk(KERN_WARNING "PARTSFS: Partition %d start: %llu size: %llu\n",
                                p,
                                (unsigned long long)partitions->parts[p].from,
                                (unsigned long long)partitions->parts[p].size * state->sector_size);
                        state->parts[p].from = partitions->parts[p].from;
                        state->parts[p].size = partitions->parts[p].size;
                        state->number_of_partitions = p; /* number of partitions */
                }
        }
        kfree(partitions);
        return state;
}

/*
 * Fills in the superblock
 */
static int partsfs_fill_super(struct super_block *sb, void *data, int silent)
{
        struct inode *root;
        struct partsfs_state *state;

        /* Get partitioning information */
        state = get_partitions_info(sb, silent);
        if (state == NULL)
            return -EINVAL;

        /* Check the partitions number */
        if (state->number_of_partitions == 0) {
                if (!silent)
                        printk(KERN_WARNING "PARTSFS: Can't find partitions\n");
                kfree(state);
                return -EINVAL;
        }

        /* Fill the superblock */
        sb->s_fs_info = state;
        sb->s_maxbytes = 0xFFFFFFFF;
        sb->s_magic = PARTSFS_MAGIC;
        sb->s_flags |= MS_NOATIME; /* Do not update access times */
        sb->s_op = &partsfs_super_ops;

        root = partsfs_get_inode(sb, PARTSFS_ROOT_DIR_INODE);
        if (IS_ERR(root)) {
                kfree(state);
                return -EINVAL;
        }

        sb->s_root = d_alloc_root(root);
        if (!sb->s_root) {
                iput(root);
                kfree(state);
                return -EINVAL;
        }

        return 0;
}

/*
 * Get a superblock for mounting
 */
static struct dentry *partsfs_mount(struct file_system_type *fs_type,
                                int flags, const char *dev_name, void *data)
{
        return mount_bdev(fs_type, flags, dev_name, data, partsfs_fill_super);
}

/*
 * Destroy a superblock
 */
static void partsfs_kill_sb(struct super_block *sb)
{
        kfree(sb->s_fs_info);
        kill_block_super(sb);
}

/*
 * Register the filesystem
 */
static int __init init_partsfs_fs(void)
{
        int ret = register_filesystem(&partsfs_fs_type);
        if (ret) {
                printk(KERN_ERR "PARTSFS: Cannot register file system (error %d)\n", ret);
                return ret;
        }
        return 0;
}

/*
 * Unregister the filesystem
 */
static void __exit exit_partsfs_fs(void)
{
        unregister_filesystem(&partsfs_fs_type);
}

module_init(init_partsfs_fs);
module_exit(exit_partsfs_fs);

MODULE_DESCRIPTION("Partitions Filesystem");
MODULE_AUTHOR("Andrea Bonomi <andrea.bonomi@gmail.com>");
MODULE_LICENSE("GPL");
