// SPDX-License-Identifier: GPL-2.0
#include "linux/buffer_head.h"
#include "linux/uaccess.h"
#include "ouichefs.h"

ssize_t ouichefs_read(struct file *file, char __user *buff, size_t size,
		      loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index, *bh_data;
	size_t remaining_read = size;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto read_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/*
	 * Get the size of the last block to read. Needed to manage file
	 * sizes that are not multiple of BLOCK_SIZE.
	 */
	int last_block_size = inode->i_size % OUICHEFS_BLOCK_SIZE;

	if (last_block_size == 0 && inode->i_size != 0)
		last_block_size = OUICHEFS_BLOCK_SIZE;

	/* Number of data blocks in the file (without the index block) */
	int nb_blocks = inode->i_blocks - 1;
	/* Index of the block where the cursor is */
	int logical_block_index = (*pos) / OUICHEFS_BLOCK_SIZE;
	/* Cursor position inside the current block */
	int logical_pos = (*pos) % OUICHEFS_BLOCK_SIZE;

	while (remaining_read && (logical_block_index < nb_blocks)) {
		uint32_t bno = index->blocks[logical_block_index];

		bh_data = sb_bread(inode->i_sb, bno);
		if (!bh_data)
			goto read_end;

		/* Available size between the cursor and the end of the block */
		size_t available_size = OUICHEFS_BLOCK_SIZE - logical_pos;

		if (logical_block_index == nb_blocks - 1)
			available_size = last_block_size - logical_pos;
		if (available_size <= 0)
			goto free_bh_data;

		/* Do not read more than what's available and asked */
		size_t len = min(available_size, remaining_read);
		char *block = (char *)bh_data->b_data;

		if (copy_to_user(buff + (size - remaining_read),
				 block + logical_pos, len)) {
			pr_err("%s: copy_to_user() failed\n", __func__);
			goto free_bh_data;
		}

		remaining_read -= len;
		logical_block_index += 1;
		/* The cursor position will start at 0 in subsequent blocks */
		logical_pos = 0;

		brelse(bh_data);
	}

	goto read_end;

free_bh_data:
	brelse(bh_data);

read_end:
	brelse(bh_index);

	size_t readen = size - remaining_read;
	*pos += readen;

	return readen;
}
