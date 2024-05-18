// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/buffer_head.h"
#include "ouichefs.h"
#include "bitmap.h"

/*
 * Move content from the beginning of a block (block_index_from),
 * up to a length fitting in a target block (block_index_to), starting at
 * logical_pos. Also, shift up any remaining data in the source block.
 */
int move_shift_block_content_to(struct ouichefs_file_index_block *index,
				struct super_block *sb, int block_index_from,
				int block_index_to, int logical_pos)
{
	struct buffer_head *bh_data_from, *bh_data_to;
	int from_len, to_available_len, to_copy;

	/* Get the size to copy, should fit in destination block. */
	from_len = get_block_size(index->blocks[block_index_from]);
	to_available_len =
		get_block_size(index->blocks[block_index_to]) - logical_pos;
	to_copy = min(from_len, to_available_len);

	bh_data_from =
		sb_bread(sb, get_block_number(index->blocks[block_index_from]));
	bh_data_to =
		sb_bread(sb, get_block_number(index->blocks[block_index_to]));
	if (!bh_data_from || !bh_data_to)
		return -EIO;

	/* Move data from source block to destination. */
	memcpy(bh_data_to->b_data + logical_pos, bh_data_from->b_data, to_copy);
	sub_block_size(&index->blocks[block_index_from], to_copy);
	add_block_size(&index->blocks[block_index_to], to_copy);

	/* Shift up any remaining data in source block. */
	memcpy(bh_data_from->b_data, bh_data_from->b_data + to_copy,
	       from_len - to_copy);

	mark_buffer_dirty(bh_data_from);
	sync_dirty_buffer(bh_data_from);
	mark_buffer_dirty(bh_data_to);
	sync_dirty_buffer(bh_data_to);
	brelse(bh_data_from);
	brelse(bh_data_to);

	return to_copy;
}

/*
 * Bubble up a block at block_index to the end of the index list.
 */
void bubble_up_block(struct ouichefs_file_index_block *index, int block_index,
		     int nb_blocks)
{
	for (int bli = block_index; bli < nb_blocks - 1; bli++) {
		int temp = index->blocks[bli];
		index->blocks[bli] = index->blocks[bli + 1];
		index->blocks[bli + 1] = temp;
	}
}

int ouichefs_defrag(struct file *file)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index = NULL;
	int bli = 0, data_moved = 0, logical_pos = 0, block_removed = 0,
	    ret = 0;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index) {
		ret = -EIO;
		goto defrag_end;
	}
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* 
	 * First pass: copy data between blocks to fill the padding
	 * going left to right.
	 */

	data_moved = get_block_size(index->blocks[bli]);
	logical_pos = data_moved;

	while (data_moved < inode->i_size && bli < inode->i_blocks - 2) {
		int block_moved;

		/* Bubble up empty blocks at the end of the index array */
		if (block_empty(index->blocks[bli + 1])) {
			bubble_up_block(index, bli + 1, inode->i_blocks - 1);
			continue;
		}

		/* Move data in the next block to the current block. */
		block_moved = move_shift_block_content_to(
			index, inode->i_sb, bli + 1, bli, logical_pos);
		if (block_moved < 0) {
			ret = -EIO;
			goto free_bh_index;
		}
		data_moved += block_moved;
		logical_pos += block_moved;

		/* If the block is full, go to the block afterward. */
		if (logical_pos == OUICHEFS_BLOCK_SIZE) {
			bli++;
			logical_pos = get_block_size(index->blocks[bli]);
		}
	}

	/*
	 * Second pass: de-allocate all empty blocks that we bubbled at the end.
	 */

	if (!block_empty(index->blocks[bli]))
		bli++;

	for (; bli < inode->i_blocks - 1; bli++) {
		put_block(OUICHEFS_SB(inode->i_sb),
			  get_block_number(index->blocks[bli]));
		index->blocks[bli] = 0;
		block_removed++;
	}

	/* Update inode information */
	inode->i_blocks -= block_removed;
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

free_bh_index:
	mark_buffer_dirty(bh_index);
	sync_dirty_buffer(bh_index);
	brelse(bh_index);

defrag_end:
	return ret;
}
