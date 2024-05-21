// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/buffer_head.h"
#include "linux/uaccess.h"
#include "linux/minmax.h"
#include "ouichefs.h"
#include "bitmap.h"

/* Returns ceil(a/b) */
static inline uint32_t idiv_ceil(uint32_t a, uint32_t b)
{
	uint32_t ret;

	ret = a / b;
	if (a % b != 0)
		return ret + 1;
	return ret;
}

/*
 * Allocate nb_blocks from a block index, update inode blocks number.
 */
int reserve_empty_blocks(struct inode *inode,
			 struct ouichefs_file_index_block *index,
			 int block_index, int nb_blocks)
{
	uint32_t bli, bno;

	for (bli = block_index; bli < block_index + nb_blocks; bli++) {
		/* Block already allocated */
		if (index->blocks[bli] != 0)
			continue;

		/* Allocate a block by removing one from the free list */
		bno = get_free_block(OUICHEFS_SB(inode->i_sb));
		if (!bno) {
			pr_err("get_free_block() failed\n");
			return 1;
		}

		set_block_number(&index->blocks[bli], bno);
		set_block_size(&index->blocks[bli], 0);

		inode->i_blocks++;
	}

	return 0;
}

/*
 * Normal write.
 */

/*
 * Pre-allocate blocks before writing.
 * Return 1 in case of error.
 */
int reserve_write_blocks(struct inode *inode,
			 struct ouichefs_file_index_block *index, int nr_allocs)
{
	int alloc_start = max((int)inode->i_blocks - 2, 0);
	return reserve_empty_blocks(inode, index, alloc_start, nr_allocs);
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
	int nb_blocks, logical_block_index, logical_pos;

	/* Check if the write can be completed (enough space?) */
	if (*pos + size > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;

	/* Check if we can allocate needed blocks */
	nr_allocs = idiv_ceil(max(*pos + (uint32_t)size, inode->i_size),
			      OUICHEFS_BLOCK_SIZE);
	if (nr_allocs > inode->i_blocks - 1)
		nr_allocs -= inode->i_blocks - 1;
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
	if (reserve_write_blocks(inode, index, nr_allocs))
		goto write_end;

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
	}
	if (written > 0) {
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);
	}

	*pos += written;

	return written;
}

/*
 * Write with insertion.
 */

/*
 * Shift all blocks from block_index by nb_blocks to the right.
 */
void shift_blocks(struct ouichefs_file_index_block *index, int block_index,
		  int nb_blocks)
{
	for (int bli = nb_blocks; bli >= block_index; bli--) {
		index->blocks[bli + nb_blocks] = index->blocks[bli];
		index->blocks[bli] = 0;
	}
}

/*
 * Copy old content from a block (block_index_from), 
 * from logical_pos to its block size, 
 * to another block (block_index_to), which is empty.
 */
int move_old_content_to(struct ouichefs_file_index_block *index,
			struct super_block *sb, int block_index_from,
			int block_index_to, int logical_pos)
{
	struct buffer_head *bh_data_from, *bh_data_to;
	int to_copy =
		get_block_size(index->blocks[block_index_from]) - logical_pos;

	bh_data_from =
		sb_bread(sb, get_block_number(index->blocks[block_index_from]));
	bh_data_to =
		sb_bread(sb, get_block_number(index->blocks[block_index_to]));
	if (!bh_data_from || !bh_data_to)
		return -EIO;

	/*
	 * Move old content from logical pos in a new empty block.
	 * Update old and new block size.
	 */
	memcpy(bh_data_to->b_data, bh_data_from->b_data + logical_pos, to_copy);
	sub_block_size(&index->blocks[block_index_from], to_copy);
	set_block_size(&index->blocks[block_index_to], to_copy);

	mark_buffer_dirty(bh_data_to);
	sync_dirty_buffer(bh_data_to);
	brelse(bh_data_from);
	brelse(bh_data_to);

	return 0;
}

ssize_t ouichefs_light_write(struct file *file, const char __user *buff,
			     size_t size, loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(inode->i_sb);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index = NULL, *bh_data = NULL;
	size_t remaining_write = size, written = 0, nr_allocs = 0, to_copy = 0;
	int logical_block_index, logical_pos, alloc_index_start, diff_old_new;
	bool move_old_content = 0;
	ssize_t ret = 0;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index) {
		ret = -EIO;
		goto write_end;
	}
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Find logical block index and position in the block based on pos. */
	if (*pos > inode->i_size) {
		/* TODO: support write after end of file. */
		pr_err("Write after the end not supported yet\n");
		goto free_bh_index;
	} else {
		find_block_pos(*pos, index, inode->i_blocks - 1,
			       &logical_block_index, &logical_pos);
		to_copy = get_block_size(index->blocks[logical_block_index]) -
			  logical_pos;
		/* Should we move old content to a new block. */
		move_old_content = to_copy > 0;
	}

	/* Compute number of blocks needed and check if we can pre-allocate. */
	diff_old_new = size - to_copy;
	diff_old_new = (size > to_copy) ? diff_old_new : 0;
	nr_allocs =
		idiv_ceil(diff_old_new, OUICHEFS_BLOCK_SIZE) + move_old_content;
	if (nr_allocs + inode->i_blocks - 1 > OUICHEFS_BLOCK_SIZE >> 2) {
		ret = -ENOSPC;
		goto free_bh_index;
	}
	if (nr_allocs > sbi->nr_free_blocks) {
		ret = -ENOSPC;
		goto free_bh_index;
	}

	/* Pre-allocate memory after block index that we insert to. */
	alloc_index_start = 0;
	if (inode->i_blocks - 1 > 0) {
		alloc_index_start = logical_block_index + 1;
		shift_blocks(index, logical_block_index + 1, nr_allocs);
	}
	reserve_empty_blocks(inode, index, alloc_index_start, nr_allocs);

	/*
	 * Move old content in logical block index to the last
	 * pre-allocated block if needed.
	 */
	if (move_old_content)
		ret = move_old_content_to(index, inode->i_sb,
					  logical_block_index,
					  logical_block_index + nr_allocs,
					  logical_pos);
	if (ret < 0)
		goto free_bh_index;

	while (remaining_write && (logical_block_index < nr_allocs)) {
		uint32_t bno;
		size_t available_size, len;
		char *block;

		/* Read data block from disk */
		bno = index->blocks[logical_block_index];
		bh_data = sb_bread(inode->i_sb, get_block_number(bno));
		if (!bh_data) {
			ret = -EIO;
			goto free_bh_index;
		}

		/* Available size between the cursor and the end of the block */
		available_size = OUICHEFS_BLOCK_SIZE - logical_pos;
		if (available_size <= 0) {
			ret = -ENODATA;
			goto free_bh_data;
		}
		/* Do not write more than what's available and asked */
		len = min(available_size, remaining_write);
		block = (char *)bh_data->b_data;

		/* Copy user data in the block and update its size. */
		if (copy_from_user(block + logical_pos,
				   (buff + (size - remaining_write)), len)) {
			pr_err("%s: copy_from_user() failed\n", __func__);
			ret = -EIO;
			goto free_bh_data;
		}
		set_block_size(&index->blocks[logical_block_index],
			       logical_pos + len);
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
	*pos += written;

	/* Update file size based on what we could write */
	if (written > 0) {
		inode->i_size = inode->i_size + written;
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);
	}

	if (ret == 0) {
		ret = written;
	}

	return ret;
}
