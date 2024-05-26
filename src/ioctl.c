// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "linux/file.h"
#include "linux/buffer_head.h"
#include "ouichefs.h"
#include "ioctl.h"
#include "bitmap.h"

static int ouichefs_ioctl_file_info(struct file *file,
				    unsigned int __user *argp)
{
	int ret = 0, display = 1;
	struct file_info user_file_info;

	if (copy_from_user(&user_file_info, argp, sizeof(user_file_info))) {
		ret = -EFAULT;
		pr_err("copy_from_user() failed\n");
		goto end;
	}

	display = !user_file_info.hide_display;

	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index = NULL;
	struct buffer_head *bh_index = NULL;
	uint32_t block_size;
	uint32_t wasted = 0;
	uint32_t block;
	uint32_t nb_partial_block = 0;
	uint32_t total_wasted = 0;

	if (display)
		pr_info("File information:\n"
			"\tsize: %lld\n"
			"\tdata blocks number: %llu\n"
			"\tblocks: ",
			inode->i_size, inode->i_blocks - 1);

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index) {
		ret = -EFAULT;
		pr_err("could not read index block\n");
		goto end;
	}
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	if (display && inode->i_blocks > 10)
		pr_cont("\n");

	for (int i = 0; i < inode->i_blocks - 1; i++) {
		block = index->blocks[i];
		block_size = get_block_size(block);
		wasted = OUICHEFS_BLOCK_SIZE - block_size;
		total_wasted += wasted;
		if (wasted != 0)
			nb_partial_block++;

		if (display)
			pr_cont("%u:%u", get_block_number(block), block_size);
		if (display && (i < inode->i_blocks - 2))
			pr_cont(", ");
		if (display && (i % 10 == 9))
			pr_cont("\n");
	}

	if (display)
		pr_cont("\n\twasted: %d\n"
			"\tpartial block: %d\n",
			total_wasted, nb_partial_block);

	brelse(bh_index);

	user_file_info.wasted = total_wasted;
	user_file_info.nb_blocks = inode->i_blocks - 1;
	if (copy_to_user(argp, &user_file_info, sizeof(user_file_info))) {
		ret = -EFAULT;
		pr_err("copy_to_user() failed\n");
		goto end;
	}

end:
	return ret;
}

static int ouichefs_ioctl_file_block_print(struct file *file)
{
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index = NULL;
	struct buffer_head *bh_index = NULL, *bh_data = NULL;
	int ret = 0;

	pr_info("file information:\n\n"
		"\tsize: %lld\n"
		"\tdata blocks number: %llu\n\n",
		inode->i_size, inode->i_blocks - 1);

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index) {
		ret = -EFAULT;
		pr_err("could not read index block\n");
		goto end;
	}
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	for (int i = 0; i < inode->i_blocks - 1; i++) {
		uint32_t block_size, block;
		int nb_iter, size_last_iter;
		char buff[1025];

		block = index->blocks[i];
		block_size = get_block_size(block);

		pr_cont("\t%d: block with id %u:\n", i,
			get_block_number(block));

		block_size = get_block_size(block);
		bh_data = sb_bread(inode->i_sb, get_block_number(block));
		if (!bh_data)
			goto end;

		nb_iter = (block_size / 1024) + 1;
		size_last_iter = block_size % 1024;

		for (int i = 0; i < nb_iter; i++) {
			if (i != nb_iter - 1) {
				memcpy(buff, bh_data->b_data + 1024 * i, 1024);
				buff[1024] = 0;
			} else {
				memcpy(buff, bh_data->b_data + 1024 * i,
				       size_last_iter);
				buff[size_last_iter + 1] = 0;
			}
			pr_cont("\t\t%s\n", buff);
		}
	}

	brelse(bh_index);

end:
	return ret;
}

static int ouichefs_ioctl_defrag(struct file *file)
{
	return ouichefs_defrag(file);
}

long ouichefs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != OUICHEFS_IOCTL_MAGIC)
		return -EINVAL;

	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case OUICHEFS_IOC_FILE_INFO:
		return ouichefs_ioctl_file_info(file, argp);
	case OUICHEFS_IOC_DEFRAG:
		return ouichefs_ioctl_defrag(file);
	case OUICHEFS_IOC_FILE_BLOCK_PRINT:
		return ouichefs_ioctl_file_block_print(file);
	default:
		return -EINVAL;
	}

	return 0;
}
