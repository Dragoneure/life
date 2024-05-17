// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/buffer_head.h"
#include "linux/uaccess.h"
#include "linux/minmax.h"
#include "ouichefs.h"
#include "bitmap.h"

/*
 * Normal write.
 */

/*
 * Pre-allocate blocks before writing.
 */
int write_reserve_blocks(struct inode *inode,
			 struct ouichefs_file_index_block *index,
			 size_t new_file_size)
{
	int new_bln, iblock, bno;
	int old_bln = inode->i_blocks - 1;

	/* Find the new number of blocks based on new file size. */
	new_bln = new_file_size / OUICHEFS_BLOCK_SIZE;
	if ((new_file_size % OUICHEFS_BLOCK_SIZE) != 0)
		new_bln++;

	for (iblock = max(old_bln - 1, 0); iblock < new_bln; iblock++) {
		/* Block already allocated */
		if (index->blocks[iblock] != 0)
			continue;

		/* Allocate a block by removing one from the free list */
		bno = get_free_block(OUICHEFS_SB(inode->i_sb));
		if (!bno) {
			pr_err("get_free_block() failed\n");
			return 1;
		}

		index->blocks[iblock] = bno;
		inode->i_blocks++;
	}

	return 0;
}

ssize_t ouichefs_write(struct file *file, const char __user *buff, size_t size,
		       loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(inode->i_sb);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index = NULL, *bh_data = NULL;
	size_t remaining_write = size, written = 0, nr_allocs = 0,
	       new_file_size = 0;
	int last_block_size, nb_blocks, logical_block_index, logical_pos;

	/* Check if the write can be completed (enough space?) */
	if (*pos + size > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;

	/* Check if we can allocate needed blocks */
	nr_allocs = max(*pos + (uint32_t)size, file->f_inode->i_size) /
		    OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto write_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Allocate needed blocks */
	new_file_size = *pos + size;
	write_reserve_blocks(inode, index, new_file_size);

	/*
	 * Get the size of the last block to write. Needed to manage file
	 * sizes that are not multiple of BLOCK_SIZE.
	 */
	last_block_size = new_file_size % OUICHEFS_BLOCK_SIZE;
	if (last_block_size == 0 && new_file_size != 0)
		last_block_size = OUICHEFS_BLOCK_SIZE;

	/* Number of data blocks in the file (without the index block) */
	nb_blocks = inode->i_blocks - 1;
	/* Index of the block where the cursor is */
	logical_block_index = (*pos) / OUICHEFS_BLOCK_SIZE;
	/* Cursor position inside the current block */
	logical_pos = (*pos) % OUICHEFS_BLOCK_SIZE;

	while (remaining_write && (logical_block_index < nb_blocks)) {
		uint32_t bno;
		size_t available_size, len;
		char *block;

		/* Read data block from disk */
		bno = index->blocks[logical_block_index];
		bh_data = sb_bread(inode->i_sb, bno);
		if (!bh_data)
			goto free_bh_index;

		/* Available size between the cursor and the end of the block */
		available_size = OUICHEFS_BLOCK_SIZE - logical_pos;
		if (logical_block_index == nb_blocks - 1)
			available_size = last_block_size - logical_pos;
		if (available_size <= 0)
			goto free_bh_data;

		/* Do not write more than what's available and asked */
		len = min(available_size, remaining_write);
		block = (char *)bh_data->b_data;

		if (copy_from_user(block + logical_pos,
				   (buff + (size - remaining_write)), len)) {
			pr_err("%s: copy_from_user() failed\n", __func__);
			goto free_bh_data;
		}

		remaining_write -= len;
		logical_block_index += 1;
		/* The cursor position will start at 0 in subsequent blocks */
		logical_pos = 0;

		mark_buffer_dirty(bh_data);
		sync_dirty_buffer(bh_data);
		brelse(bh_data);
	}

	goto free_bh_index;

free_bh_data:
	brelse(bh_data);

free_bh_index:
	mark_buffer_dirty(bh_index);
	sync_dirty_buffer(bh_index);
	brelse(bh_index);

write_end:
	written = size - remaining_write;

	/* Update file size based on what we could write */
	new_file_size = *pos + written;
	if (new_file_size > inode->i_size) {
		inode->i_size = new_file_size;
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);
	}

	*pos += written;

	return written;
}

/*
 * Write with insertion.
 */

ssize_t ouichefs_write_insert(struct file *file, const char __user *buff,
			      size_t size, loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(inode->i_sb);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index = NULL, *bh_data = NULL;
	size_t remaining_write = size, written = 0, nr_allocs = 0,
	       new_file_size = 0;
	int last_block_size, nb_blocks, logical_block_index, logical_pos;
	find_block_pos(*pos, index, nb_blocks, &logical_block_index,
		       &logical_pos);
	reserve_blocks_write_insert(inode, index, logical_pos,
				    logical_block_index, size);
}
