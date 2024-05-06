// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/printk.h"
#include "linux/buffer_head.h"
#include "linux/minmax.h"
#include "linux/uaccess.h"
#include "ouichefs.h"
#include "bitmap.h"
#include "write.h"

void move_to(struct ouichefs_file_index_block *index, int to, int from, int nb)
{
	for (int i = 0; i < nb; i++) {
		index->blocks[to] = index->blocks[from];
		index->blocks[from] = 0;
		to--;
		from--;
	}
}
int reserve_blocks_wli(struct inode *inode, int bn_insertion,
		       int nb_to_allocate,
		       struct ouichefs_file_index_block *index)
{
	int id_last_block = inode->i_blocks - 2;
	move_to(index, id_last_block + nb_to_allocate, id_last_block,
		nb_to_allocate);
	for (int iblock = bn_insertion + 1;
	     iblock < bn_insertion + 1 + nb_to_allocate; iblock++) {
		/* Block already allocated */
		if (index->blocks[iblock] != 0)
			continue;
		int bno = get_free_block(OUICHEFS_SB(inode->i_sb));

		if (!bno) {
			pr_err("get_free_block() failed\n");
			return 1;
		}

		index->blocks[iblock] = get_block_number(bno);
		inode->i_blocks++;
	}

	return 0;
}

ssize_t ouichefs_write_li(struct file *file, const char __user *buff,
			  size_t size, loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(inode->i_sb);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index = NULL, *bh_data = NULL;
	size_t remaining_write = size;
	size_t writen = 0;
	/* Index of the block where the cursor is */
	int logical_block_index = (*pos) / OUICHEFS_BLOCK_SIZE;
	/* Cursor position inside the current block */
	int logical_pos = (*pos) % OUICHEFS_BLOCK_SIZE;

	/*
	 * Amount of data to be copied from the block of insertion
	 * to another block
	 */
	int to_copy = OUICHEFS_BLOCK_SIZE - logical_pos;

	/*Amount of insert data to be writen into a new block*/
	int to_write_nb = size - to_copy;

	/* Check if the write can be completed (enough space?) */
	if (*pos + size > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;

	/* Check if we can allocate needed blocks */
	size_t nr_allocs = ((to_write_nb) / OUICHEFS_BLOCK_SIZE) + 1;
	if ((to_write_nb % OUICHEFS_BLOCK_SIZE) != 0)
		nr_allocs++;
	if (nr_allocs > sbi->nr_free_blocks ||
	    ((nr_allocs + inode->i_blocks) > 1025))
		return -ENOSPC;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto write_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Allocate needed blocks */
	reserve_blocks_wli(inode, logical_block_index, nr_allocs, index);

	/*
	 * Get the size of the last block to write. Needed to manage file
	 * sizes that are not multiple of BLOCK_SIZE.
	 */
	int last_block_size = inode->i_size % OUICHEFS_BLOCK_SIZE;

	if (last_block_size == 0 && inode->i_size != 0)
		last_block_size = OUICHEFS_BLOCK_SIZE;

	/* Number of data blocks in the file (without the index block) */
	int nb_blocks = inode->i_blocks - 1;

	while (remaining_write && (logical_block_index < nb_blocks)) {
		uint32_t bno = index->blocks[logical_block_index];

		bh_data = sb_bread(inode->i_sb, bno);
		if (!bh_data)
			goto free_bh_index;

		/* Available size between the cursor and the end of the block */
		size_t available_size = OUICHEFS_BLOCK_SIZE - logical_pos;

		if (logical_block_index == nb_blocks - 1)
			available_size = last_block_size - logical_pos;
		if (available_size <= 0)
			goto free_bh_data;

		/* Do not write more than what's available and asked */
		size_t len = min(available_size, remaining_write);
		char *block = (char *)bh_data->b_data;

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
	writen = size - remaining_write;
	*pos += writen;

	/* Update file size */
	size_t new_file_size = *pos + writen;

	if (new_file_size > inode->i_size) {
		inode->i_size = new_file_size;
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);
	}
	return writen;
}
