// SPDX-License-Identifier: GPL-2.0

#include "bitmap.h"
#include "linux/buffer_head.h"
#include "ouichefs.h"
#include <string.h>

void update(struct inode *inode, struct ouichefs_file_index_block *index,
	    int block_number, int size_start)
{
	int new_size = get_block_size(index->blocks[block_number]) - size_start;
	struct buffer_head *bh_data1 = sb_bread(
		inode->i_sb, get_block_number(index->blocks[block_number]));
	char buff[new_size];

	memcpy(buff, bh_data1 + size_start, new_size);
	memcpy(bh_data1, buff, new_size);

	set_block_size(index->blocks[block_number], new_size);
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
			if (next_block == inode->i_blocks) {
				break;
			}
			index->blocks[current_block] =
				index->blocks[next_block];
			index->blocks[next_block] = 0;
		}
		current_block++;
		next_block++;
	}
}

int defrag(struct file *file)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index, *bh_data;
	int current_block = 0;

	while (current_block < inode->i_blocks - 1) {
		int taille_block = get_block_size(index->blocks[current_block]);
		if (taille_block == OUICHEFS_BLOCK_SIZE) {
			continue;
		}

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

			if (to_copy == taille_next_block) {
				put_block(OUICHEFS_SB(inode->i_sb),
					  index->blocks[next_block]);
				index->blocks[next_block] = 0;
			}

			update(inode, index, next_block, to_copy);
			next_block++;
		}
		set_block_size(index->blocks[current_block], taille_block);
		current_block++;
	}

	defrag_block(inode, index);

	return 0;
}
