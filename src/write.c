// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/buffer_head.h"
#include "linux/uaccess.h"
#include "linux/minmax.h"
#include "ouichefs.h"
#include "bitmap.h"

/*
 * Check file flags and update pos if needed.
 * Return -1 in case of error.
 */
int write_flags(struct file *file, loff_t *pos)
{
	if (file->f_flags & O_APPEND)
		*pos = file->f_inode->i_size;
	if (file->f_flags & O_RDONLY)
		return -1;
	return 0;
}

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
 * Check if we can allocate nb_allocs blocks.
 * Return an error if there is not enough space.
 */
int space_available(struct inode *inode, struct ouichefs_sb_info *sbi,
		    int nb_allocs)
{
	if (nb_allocs + inode->i_blocks - 1 > OUICHEFS_BLOCK_SIZE >> 2)
		return -ENOSPC;
	if (nb_allocs > sbi->nr_free_blocks)
		return -ENOSPC;
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
			 struct ouichefs_file_index_block *index, int nb_allocs)
{
	/* We start to allocate after the last block */
	int alloc_start = max((int)inode->i_blocks - 1, 0);

	return reserve_empty_blocks(inode, index, alloc_start, nb_allocs);
}

ssize_t ouichefs_write(struct file *file, const char __user *buff, size_t size,
		       loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(inode->i_sb);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index = NULL, *bh_data = NULL;
	size_t remaining_write = size, written = 0, nb_allocs = 0,
	       new_file_size = 0;
	int nb_blocks, logical_block_index, logical_pos;

	/* Update the pos based on the flags (e.g APPEND) */
	if (write_flags(file, pos) < 0)
		return -EINVAL;

	/* Check if the write can be completed (enough space?) */
	if (*pos + size > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;

	/* Check if we can allocate needed blocks */
	nb_allocs = idiv_ceil(max(*pos + (uint32_t)size, inode->i_size),
			      OUICHEFS_BLOCK_SIZE);
	nb_allocs = max((int)nb_allocs - ((int)inode->i_blocks - 1), 0);
	if (space_available(inode, sbi, nb_allocs) < 0)
		return -ENOSPC;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto write_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Allocate needed blocks */
	if (reserve_write_blocks(inode, index, nb_allocs))
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
			pr_err("copy_from_user() failed\n");
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
	if (new_file_size > inode->i_size)
		inode->i_size = new_file_size;

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
		  int nb_shift, int last_bli)
{
	for (int bli = last_bli; bli >= block_index; bli--) {
		index->blocks[bli + nb_shift] = index->blocks[bli];
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
	memset(bh_data_from->b_data + logical_pos, 0, to_copy);
	sub_block_size(&index->blocks[block_index_from], to_copy);
	set_block_size(&index->blocks[block_index_to], to_copy);

	mark_buffer_dirty(bh_data_to);
	sync_dirty_buffer(bh_data_to);
	brelse(bh_data_from);
	brelse(bh_data_to);

	return 0;
}

/*
 * Allocate and fill blocks to reach desired cursor position.
 * Find the final logical block number and logical position inside the block.
 * Return an error if there is not enough space.
 */
int fill_to_reach_pos(struct inode *inode,
		      struct ouichefs_file_index_block *index,
		      struct ouichefs_sb_info *sbi, loff_t pos,
		      int *logical_block_index, int *logical_pos)
{
	int bli, last_bli, last_block_size, block_size, nb_blocks_to_fill,
		to_fill, alloc_start, available_size, remaining;
	bool last_block_full = 0;
	int filled = 0;

	/* Find if there is available size in the last block to avoid allocating */
	last_bli = max((int)inode->i_blocks - 2, 0);
	last_block_size = get_block_size(index->blocks[last_bli]);
	available_size = OUICHEFS_BLOCK_SIZE - last_block_size;
	if (index->blocks[last_bli] == 0)
		available_size = 0;

	/* Find how many blocks we need to allocate to fill the gap. */
	to_fill = pos - inode->i_size;
	nb_blocks_to_fill = idiv_ceil(max(to_fill - available_size, 0),
				      OUICHEFS_BLOCK_SIZE);
	if (space_available(inode, sbi, nb_blocks_to_fill) < 0)
		return -ENOSPC;

	/* Allocate and fill blocks to reach file cursor, start after last block. */
	alloc_start = max((int)inode->i_blocks - 1, 0);
	reserve_empty_blocks(inode, index, alloc_start, nb_blocks_to_fill);

	bli = (available_size > 0) ? max(alloc_start - 1, 0) : alloc_start;
	while (filled < to_fill) {
		remaining = OUICHEFS_BLOCK_SIZE -
			    get_block_size(index->blocks[bli]);
		block_size = min(to_fill - filled, remaining);
		add_block_size(&index->blocks[bli], block_size);
		filled += block_size;
		bli++;
	}
	bli--;

	inode->i_size += filled;
	last_block_size = get_block_size(index->blocks[bli]);
	last_block_full = last_block_size == OUICHEFS_BLOCK_SIZE;
	*logical_block_index = last_block_full ? bli + 1 : bli;
	*logical_pos = last_block_full ? 0 : last_block_size;

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
	size_t remaining_write = size, written = 0, nb_allocs = 0, to_copy = 0;
	int logical_block_index, logical_pos, alloc_index_start = 0, nb_blocks,
					      remaining_size, available_size,
					      last_bli;
	bool move_old_content = 0, shift_old_content = 0;
	ssize_t ret = 0;

	/* Update the pos based on the flags (e.g APPEND) */
	if (write_flags(file, pos) < 0)
		return -EINVAL;

	/* Check if the write can be completed (enough space?) */
	if (*pos + size > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index) {
		ret = -EIO;
		goto write_end;
	}
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	if (*pos > inode->i_size) {
		/* We insert after the end of the file, fill to reach the cursor */
		ret = fill_to_reach_pos(inode, index, sbi, *pos,
					&logical_block_index, &logical_pos);
		if (ret < 0)
			goto free_bh_index;
	} else {
		/* Find logical block index and position in the block based on pos */
		find_block_pos(*pos, index, inode->i_blocks - 1,
			       &logical_block_index, &logical_pos);
		to_copy = get_block_size(index->blocks[logical_block_index]) -
			  logical_pos;
		/* Should we move old content to a new block */
		move_old_content = to_copy > 0;
		/* Shift only if we are not in the last block */
		last_bli = max((int)inode->i_blocks - 2, 0);
		shift_old_content = logical_block_index != last_bli;
	}

	/*
	 * Compute number of blocks needed and check if we can pre-allocate.
	 * No block is allocated if there is enough space in the block we insert to.
	 */
	available_size = OUICHEFS_BLOCK_SIZE - logical_pos;
	remaining_size = max((int)size - available_size, 0);
	nb_allocs = idiv_ceil(remaining_size, OUICHEFS_BLOCK_SIZE) +
		    move_old_content;
	/* Cursor in new unallocated block */
	if (index->blocks[logical_block_index] == 0)
		nb_allocs++;
	ret = space_available(inode, sbi, nb_allocs);
	if (ret < 0)
		goto free_bh_index;

	/*
	 * Pre-allocate memory after block that we insert to.
	 * If the current block was not allocated before, start from it.
	 */
	alloc_index_start = logical_block_index + 1;
	if (index->blocks[logical_block_index] == 0)
		alloc_index_start--;
	if (shift_old_content && nb_allocs > 0)
		shift_blocks(index, alloc_index_start, nb_allocs, last_bli);
	reserve_empty_blocks(inode, index, alloc_index_start, nb_allocs);

	/*
	 * Move old content in logical block index to the last
	 * pre-allocated block if needed.
	 */
	if (move_old_content)
		ret = move_old_content_to(index, inode->i_sb,
					  logical_block_index,
					  logical_block_index + nb_allocs,
					  logical_pos);
	if (ret < 0)
		goto free_bh_index;

	nb_blocks = inode->i_blocks - 1;
	while (remaining_write && (logical_block_index < nb_blocks)) {
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
		if (available_size == 0) {
			ret = -ENODATA;
			goto free_bh_data;
		}
		/* Do not write more than what's available and asked */
		len = min(available_size, remaining_write);
		block = (char *)bh_data->b_data;
		/* Copy user data in the block and update its size. */
		if (copy_from_user(block + logical_pos,
				   (buff + (size - remaining_write)), len)) {
			pr_err("copy_from_user() failed\n");
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

	if (ret == 0)
		ret = written;

	return ret;
}
