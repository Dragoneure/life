// SPDX-License-Identifier: GPL-2.0

#include "linux/pagemap.h"

#include "linux/buffer_head.h"
#include "linux/uaccess.h"
#include "linux/minmax.h"
#include "ouichefs.h"
#include "bitmap.h"

ssize_t ouichefs_read_page(struct file *file, char __user *buff, size_t size,
			   loff_t *pos)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct page *page;
	struct readahead_control rc;
	pgoff_t pgi;
	int ret;
	char *data;
	int remaining_read = size, readen = 0;

	if (*pos + size > inode->i_size)
		return -EIO;

	/* Add other condition to the while*/
	while (remaining_read) {
		/* Get the index block through page cache? */
		/* Find the page index depending of the file cursor (need to be fixed)*/
		pgi = 2;
		page = find_get_page(file->f_mapping, pgi);

		/* 
	 * If the page is not in the page cache 
	 * Allocate a new page page and add it to the page cache
	 */
		if (!page) {
			page = page_cache_alloc(file->f_mapping);
			if (!page)
				return -EIO;

			ret = add_to_page_cache_lru(page, file->f_mapping, pgi,
						    GFP_KERNEL);
			if (ret)
				/* error adding page into page cache */
				return ret;

			rc.file = file;
			rc.mapping = file->f_mapping;
			rc.ra = NULL;
			rc._index = pgi;
			rc._nr_pages = 1;
			/* What is batch_count ?*/
			rc._batch_count = 0;
			file->f_mapping->a_ops->readahead(&rc);

			/* Mapped the page to virtual memory into the kernel : do we need to do 
	 	 * it everytime or only if the page is not in the page cache?
	   */
			data = kmap_local_page(page);
		}

		if (copy_to_user(buff, data, size))
			pr_err("%s: copy_to_user() failed\n", __func__);

		// remaining_read -= qqchose;
	}
}
