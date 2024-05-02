#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/printk.h"
#include "linux/buffer_head.h"
#include "linux/minmax.h"
#include "linux/uaccess.h"
#include "ouichefs.h"
#include "bitmap.h"
#include "write.h"

int check_begin(struct file *file, loff_t pos, size_t len)
{
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);
	uint32_t nr_allocs = 0;

	/* Check if the write can be completed (enough space?) */
	if (pos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs =
		max((uint32_t)(pos + len), (uint32_t)file->f_inode->i_size) /
		OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;
	return 0;
}

void check_end(struct file *file)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;

	/* Complete the write() */
	uint32_t nr_blocks_old = inode->i_blocks;

	/* Update inode metadata */
	inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);

	/* If file is smaller than before, free unused blocks */
	if (nr_blocks_old > inode->i_blocks) {
		int i;
		struct buffer_head *bh_index;
		struct ouichefs_file_index_block *index;

		/* Free unused blocks from page cache */
		truncate_pagecache(inode, inode->i_size);

		/* Read index block to remove unused blocks */
		bh_index = sb_bread(sb, ci->index_block);
		if (!bh_index) {
			pr_err("failed truncating '%s'. we just lost %llu blocks\n",
			       file->f_path.dentry->d_name.name,
			       nr_blocks_old - inode->i_blocks);
		}
		index = (struct ouichefs_file_index_block *)bh_index->b_data;

		for (i = inode->i_blocks - 1; i < nr_blocks_old - 1; i++) {
			put_block(OUICHEFS_SB(sb), index->blocks[i]);
			index->blocks[i] = 0;
		}
		mark_buffer_dirty(bh_index);
		brelse(bh_index);
	}
}

int reserve_blocks(struct inode *inode, size_t size, loff_t *pos,
		   struct ouichefs_file_index_block *index)
{
	size_t new_file_size = *pos + size;
	inode->i_size = new_file_size;

	int old_bln = inode->i_blocks - 1;
	int new_bln = new_file_size / OUICHEFS_BLOCK_SIZE;
	if ((new_file_size % OUICHEFS_BLOCK_SIZE) > 0)
		new_bln++;

	for (int bli = max(old_bln - 1, 0); bli < new_bln; bli++) {
		int bno = get_free_block(OUICHEFS_SB(inode->i_sb));
		if (!bno) {
			pr_err("%s: get_free_block() failed\n", __func__);
			return 1;
		}

		index->blocks[bli] = bno;
		inode->i_blocks++;

		struct buffer_head *bh_data;
		bh_data = sb_bread(inode->i_sb, bno);

		mark_buffer_dirty(bh_data);
		sync_dirty_buffer(bh_data);
		brelse(bh_data);
	}

	return 0;
}

ssize_t ouichefs_write(struct file *file, const char __user *buff, size_t size,
		       loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index, *bh_data;
	size_t remaining_write = size;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto write_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	reserve_blocks(inode, size, pos, index);

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

	while (remaining_write && (logical_block_index < nb_blocks)) {
		uint32_t bno = index->blocks[logical_block_index];
		bh_data = sb_bread(inode->i_sb, bno);
		if (!bh_data)
			goto write_end;

		/* Available size between the cursor and the end of the block */
		size_t available_size = OUICHEFS_BLOCK_SIZE - logical_pos;
		if (logical_block_index == nb_blocks - 1)
			available_size = last_block_size - logical_pos;
		if (available_size <= 0)
			goto free_bh_data;

		/* Do not read more than what's available and asked */
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

	goto write_end;

free_bh_data:
	brelse(bh_data);

write_end:
	brelse(bh_index);

	size_t readen = size - remaining_write;
	*pos += readen;

	return readen;
}

ssize_t ouichefs_write_test(struct file *file, const char __user *buff,
			    size_t size, loff_t *pos)
{
	if (!check_begin(file, *pos, size)) {
		return 0;
	}
	size_t block_logical_number = (*pos) >> 12;
	size_t size_fic = file->f_inode->i_size;
	size_t nb_block_file = size_fic / OUICHEFS_BLOCK_SIZE;
	size_t logical_pos = (*pos) % OUICHEFS_BLOCK_SIZE;
	size_t remaining_read = OUICHEFS_BLOCK_SIZE - logical_pos;
	if ((size_fic % OUICHEFS_BLOCK_SIZE) > 0)
		nb_block_file++;
	if (*pos > size_fic) {
		while (block_logical_number > (nb_block_file + 1)) {
			struct buffer_head *bh_result = NULL;
			int ret = ouichefs_file_get_block(file->f_inode,
							  nb_block_file + 1,
							  bh_result, true);
			memset(bh_result->b_data, 0, 1 << 12);
			mark_buffer_dirty(bh_result);
			sync_dirty_buffer(bh_result);
			brelse(bh_result);
			if (ret < 0) {
				return 0;
			}

			nb_block_file++;
		}
	}
	size_t total_write = 0;
	struct buffer_head *bh_result = NULL;
	int ret = ouichefs_file_get_block(file->f_inode, block_logical_number,
					  bh_result, true);
	if (ret < 0) {
		goto end;
	}
	if (nb_block_file > block_logical_number) {
		memset(bh_result->b_data, 0, 1 << 12);
	}
	size_t size_to_write = min(remaining_read, size);
	size_t size_fail_write = copy_from_user(bh_result->b_data + logical_pos,
						buff, size_to_write);
	mark_buffer_dirty(bh_result);
	sync_dirty_buffer(bh_result);
	brelse(bh_result);
	total_write += size_to_write - size_fail_write;
	if (size_fail_write > 0) {
		return total_write;
	}
	block_logical_number++;
	while (total_write < size) {
		int ret = ouichefs_file_get_block(
			file->f_inode, block_logical_number, bh_result, true);
		if (ret < 0) {
			goto end;
		}
		size_t size_to_write = min((size_t)OUICHEFS_BLOCK_SIZE, size);
		size_t size_fail_write =
			copy_from_user(bh_result->b_data, buff, size_to_write);
		mark_buffer_dirty(bh_result);
		sync_dirty_buffer(bh_result);
		brelse(bh_result);
		total_write += size_to_write - size_fail_write;
		if (size_fail_write > 0) {
			goto end;
		}
		block_logical_number++;
	}

end:
	check_end(file);
	return total_write;
}
