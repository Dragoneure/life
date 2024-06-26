// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/buffer_head.h"
#include "linux/pagemap.h"
#include "linux/mpage.h"
#include "ouichefs.h"
#include "bitmap.h"

/*
 * Check file flags.
 * Return -1 in case of error.
 */
int read_flags(struct file *file)
{
	if (file->f_flags & O_WRONLY)
		return -1;
	return 0;
}

/*
 * Normal write.
 */

ssize_t ouichefs_read(struct file *file, char __user *buff, size_t size,
		      loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index, *bh_data;
	size_t remaining_read = size, readen = 0;
	int last_block_size, nb_blocks, logical_block_index, logical_pos;

	/* Check if we can read */
	if (read_flags(file) < 0)
		return -EINVAL;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto read_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/*
	 * Get the size of the last block to read. Needed to manage file
	 * sizes that are not multiple of BLOCK_SIZE.
	 */
	last_block_size = inode->i_size % OUICHEFS_BLOCK_SIZE;
	if (last_block_size == 0 && inode->i_size != 0)
		last_block_size = OUICHEFS_BLOCK_SIZE;

	/* Number of data blocks in the file (without the index block) */
	nb_blocks = inode->i_blocks - 1;
	/* Index of the block where the cursor is */
	logical_block_index = (*pos) / OUICHEFS_BLOCK_SIZE;
	/* Cursor position inside the current block */
	logical_pos = (*pos) % OUICHEFS_BLOCK_SIZE;

	while (remaining_read && (logical_block_index < nb_blocks)) {
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
		if (available_size == 0)
			goto free_bh_data;

		/* Do not read more than what's available and asked */
		len = min(available_size, remaining_read);
		block = (char *)bh_data->b_data;

		if (copy_to_user(buff + (size - remaining_read),
				 block + logical_pos, len)) {
			pr_err("copy_to_user() failed\n");
			goto free_bh_data;
		}

		remaining_read -= len;
		logical_block_index += 1;
		/* The cursor position will start at 0 in subsequent blocks */
		logical_pos = 0;

		brelse(bh_data);
	}

	goto free_bh_index;

free_bh_data:
	brelse(bh_data);

free_bh_index:
	brelse(bh_index);

read_end:
	readen = size - remaining_read;
	*pos += readen;

	return readen;
}

/*
 * Read that support with insertion.
 */

ssize_t ouichefs_light_read(struct file *file, char __user *buff, size_t size,
			    loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index, *bh_data;
	size_t remaining_read = size;
	int nb_blocks, logical_block_index, logical_pos;

	/* Check if we can read */
	if (read_flags(file) < 0)
		return -EINVAL;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto read_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Number of data blocks in the file (without the index block). */
	nb_blocks = inode->i_blocks - 1;

	/* Find the index of the block where the cursor is. */
	if (find_block_pos(*pos, index, nb_blocks, &logical_block_index,
			   &logical_pos))
		goto read_end;

	while (remaining_read && (logical_block_index < nb_blocks)) {
		uint32_t bno;
		size_t available_size, len;
		char *block;

		/* Read data block from disk */
		bno = index->blocks[logical_block_index];
		bh_data = sb_bread(inode->i_sb, get_block_number(bno));
		if (!bh_data)
			goto read_end;

		/* Available size between the cursor and the end of the block */
		available_size = get_block_size(bno) - logical_pos;
		if (available_size <= 0)
			goto free_bh_data;

		/* Do not read more than what's available and asked */
		len = min(available_size, remaining_read);
		block = (char *)bh_data->b_data;

		if (copy_to_user(buff + (size - remaining_read),
				 block + logical_pos, len)) {
			pr_err("copy_to_user() failed\n");
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

/*
 * Read that support with insertion using page cache.
 */

ssize_t ouichefs_read_cached(struct file *file, char __user *buff, size_t size,
			     loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct address_space *mapping = inode->i_mapping;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	struct page *page;
	int remaining_read = size, logical_pos, nb_blocks, block_size,
	    available_size, len, logical_block_index;

	/* Check if we can read */
	if (read_flags(file) < 0)
		return -EINVAL;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto read_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/* Number of data blocks in the file (without the index block). */
	nb_blocks = inode->i_blocks - 1;

	/* Find the index of the block where the cursor is. */
	if (find_block_pos(*pos, index, nb_blocks, &logical_block_index,
			   &logical_pos))
		goto read_end;

	while (remaining_read && (logical_block_index < nb_blocks)) {
		page = read_mapping_page(mapping, logical_block_index, NULL);
		if (IS_ERR(page))
			goto read_end;

		/* Available size between the cursor and the end of the block */
		block_size = get_block_size(index->blocks[logical_block_index]);
		available_size = block_size - logical_pos;
		if (available_size <= 0)
			goto free_page;

		/* Do not read more than what's available and asked */
		len = min(available_size, remaining_read);

		if (copy_to_user(buff, page_address(page) + logical_pos, len)) {
			pr_err("copy_to_user() failed\n");
			goto free_page;
		}

		remaining_read -= len;
		logical_block_index++;
		/* The cursor position will start at 0 in subsequent blocks */
		logical_pos = 0;

		unlock_page(page);
		put_page(page);
	}

	goto read_end;

free_page:
	unlock_page(page);
	put_page(page);

read_end:
	brelse(bh_index);

	size_t readen = size - remaining_read;
	*pos += readen;

	return readen;
}
