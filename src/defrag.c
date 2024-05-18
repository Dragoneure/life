// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/buffer_head.h"
#include "bitmap.h"
#include "ouichefs.h"

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

	return 0;
}

void defrag_block(struct inode *inode, struct ouichefs_file_index_block *index)
{
	int current_block = 0;
	int next_block = 1;

	while (current_block < inode->i_blocks) {
		if (index->blocks[current_block] == 0) {
			while (next_block < inode->i_blocks &&
			       index->blocks[next_block] == 0) {
				next_block++;
			}
			if (next_block == inode->i_blocks)
				break;
			index->blocks[current_block] =
				index->blocks[next_block];
			index->blocks[next_block] = 0;
		}
		current_block++;
		next_block++;
	}
	inode->i_blocks = current_block + 1;
	mark_inode_dirty(inode);
}

int defrag(struct file *file)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_file_index_block *index;
	int current_block = 0;

	while (current_block < inode->i_blocks - 1) {
		int taille_block = get_block_size(index->blocks[current_block]);

		if (taille_block == OUICHEFS_BLOCK_SIZE)
			continue;

		int remaining_size = OUICHEFS_BLOCK_SIZE - taille_block;
		int next_block = current_block + 1;

		while ((remaining_size > 0) && (next_block < inode->i_blocks)) {
			int taille_next_block =
				get_block_size(index->blocks[next_block]);
			struct buffer_head *bh_data1 = sb_bread(
				inode->i_sb,
				get_block_number(index->blocks[current_block]));
			struct buffer_head *bh_data2 = sb_bread(
				inode->i_sb,
				get_block_number(index->blocks[next_block]));
			int to_copy = min(remaining_size, taille_next_block);

			memcpy(bh_data1->b_data + taille_block,
			       bh_data2->b_data, to_copy);
			taille_block += to_copy;
			remaining_size -= to_copy;

			mark_buffer_dirty(bh_data1);
			sync_dirty_buffer(bh_data1);
			brelse(bh_data1);
			mark_buffer_dirty(bh_data2);
			sync_dirty_buffer(bh_data2);
			brelse(bh_data2);

			if (to_copy == taille_next_block) {
				put_block(OUICHEFS_SB(inode->i_sb),
					  index->blocks[next_block]);
				index->blocks[next_block] = 0;
			} else {
				update(inode, index, next_block, to_copy);
			}
			next_block++;
		}
		set_block_size(index->blocks[current_block], taille_block);
		current_block++;
	}

	defrag_block(inode, index);

	return 0;
}
