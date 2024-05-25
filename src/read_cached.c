// SPDX-License-Identifier: GPL-2.0

#include "asm/pgtable_64_types.h"
#include "linux/gfp_types.h"
#include "linux/mm.h"
#include "linux/pagemap.h"

#include "linux/buffer_head.h"
#include "linux/uaccess.h"
#include "linux/minmax.h"
#include "ouichefs.h"
#include "bitmap.h"

ssize_t ouichefs_read_cached(struct file *file, char __user *buff, size_t size,
			     loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	struct page *page;
	int remaining_read = size, logical_block_index, logical_pos, nb_blocks,
	    nb_blocks_in_page = PAGE_SIZE / OUICHEFS_BLOCK_SIZE, bno,
	    available_size, len;
	pgoff_t pgi;

	if (*pos + size > inode->i_size)
		return -EIO;

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index)
		goto read_end;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	nb_blocks = inode->i_blocks - 1;

	/* Find the index of the block where the cursor is. */
	if (find_block_pos(*pos, index, nb_blocks, &logical_block_index,
			   &logical_pos))
		goto read_end;

	pgi = logical_block_index / nb_blocks_in_page;

	/* Add other condition to the while*/
	while (remaining_read && (logical_block_index < nb_blocks)) {
		pr_info(" pgi : %lu, logical block index : %d logical pos : %d\n",
			pgi, logical_block_index, logical_pos);

		page = grab_cache_page(file->f_mapping, pgi);
		bno = index->blocks[logical_block_index];
		available_size = get_block_size(bno) - logical_pos;
		if (available_size <= 0)
			goto free_page;

		/* Do not read more than what's available and asked */
		len = min(available_size, remaining_read);
		if (copy_to_user(buff, page_address(page) + logical_pos, len))
			pr_err("%s: copy_to_user() failed\n", __func__);
		pr_info(":%s\n", page_address(page));
		unlock_page(page);
		put_page(page);

		remaining_read -= len;
		logical_block_index++;
		logical_pos = 0;
		/* Could be optimized */
		pgi = logical_block_index / nb_blocks_in_page;
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
