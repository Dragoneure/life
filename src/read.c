#include "linux/buffer_head.h"
#include "linux/compiler.h"
#include "linux/gfp_types.h"
#include "linux/minmax.h"
#include "linux/slab.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "ouichefs.h"

/*
	 * Concurency has been managed in upper layer (ksys_read), no need
	 * to manage it here, we are already  safe here
 	 */
ssize_t ouichefs_read(struct file *file, char __user *buff, size_t size,
		      loff_t *pos)
{
	struct buffer_head *bh_index, *bh_data;
	struct ouichefs_file_index_block *index;
	// pr_info("pos : %lld\n", *pos);
	// pr_info("size : %lu\n", size);
	/*
	 * block logical number of the first character to be read
	 * f_pos (cursor) /  4 KiB
	 */
	int block_logical_number = (*pos) >> 12;

	/*the pos cursor inside a block*/
	int logical_pos = (*pos) % OUICHEFS_BLOCK_SIZE;
	// pr_info("logical pos : %d\n", logical_pos);
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	sector_t index_block = ci->index_block;
	unsigned long remaining_read = size;
	sector_t sc;
	int res = 0;
	/*inode -> i_blocks, total number of the file, including the block of index*/
	int nb_blocks = inode->i_blocks - 1;
	char *buff_temp = kzalloc(size, GFP_KERNEL);

	size_t available_size = 0;
	int last_bloc_size;
	int total_read = 0;
	/* Amount of data to be read inside one block*/
	int to_read = 0;
	char *data;
	/* Read index block from disk*/
	bh_index = sb_bread(inode->i_sb, index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	last_bloc_size = inode->i_size % OUICHEFS_BLOCK_SIZE;
	/* if the last bock of the file is full
	 * It's possible that the modulo return 0 if the file is
	 * empty, so last_block_size_should be 0
	 */
	if ((last_bloc_size == 0) && (nb_blocks > 1))
		last_bloc_size = OUICHEFS_BLOCK_SIZE;
	// pr_info("last block size : %d\n", last_bloc_size);
	// pr_info("nr_block : %d\n", nb_blocks);
	while ((remaining_read) && (block_logical_number < nb_blocks)) {
		// pr_info("block logical number : %d\n", block_logical_number);
		sc = (sector_t)index->blocks[block_logical_number];
		// pr_info("sc : %llu\n", sc);
		bh_data = sb_bread(inode->i_sb, sc);
		/*If we are reading the last block of the file*/
		if (unlikely(block_logical_number == (nb_blocks - 1)))
			available_size = last_bloc_size - logical_pos;
		else
			available_size = OUICHEFS_BLOCK_SIZE - logical_pos;

		if (unlikely(available_size <= 0))
			break;
		/*Not read more than what we can read and what we want to read*/
		to_read = min(available_size, remaining_read);
		data = (char *)bh_data->b_data;
		// read = copy_to_user(buff + (total_read), (data + logical_pos),
		// 		    to_read);
		memcpy(buff_temp + total_read, data + logical_pos, to_read);
		remaining_read -= to_read;
		total_read += to_read;
		block_logical_number++;
		/*
		 * If we read a new block, for sure the cursor position in this block 
		 * will be at the begining
		 */
		logical_pos = 0;
		brelse(bh_data);
	}
	brelse(bh_index);
	if (total_read)
		res = copy_to_user(buff, buff_temp, total_read);
	if (res > 0) {
		total_read -= res;
	}
	kfree(buff_temp);
	// pr_info("total read : %d\n", total_read);
	*pos += total_read;
	return total_read;
}
