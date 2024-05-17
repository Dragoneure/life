// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include "asm-generic/errno-base.h"
#include "ouichefs.h"
#include "ioctl.h"
#include "bitmap.h"
#include "linux/file.h"
#include "linux/buffer_head.h"

static int ouichefs_ioctl_file_info(struct file *file,
				    unsigned int __user *argp)
{
	int ret = 0;
	unsigned int fd;

	if (get_user(fd, argp)) {
		ret = -EFAULT;
		pr_err("get_user() failed\n");
		goto end;
	}

	struct file *user_file = fget(fd);

	if (!user_file) {
		ret = -EINVAL;
		pr_err("invalid fd %d, file not found\n", fd);
		goto end;
	}

	struct inode *inode = user_file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index = NULL;
	struct buffer_head *bh_index = NULL;
	uint32_t size_block;
	uint32_t wasted = 0;
	uint32_t block;
	uint32_t nb_partial_block = 0;
	uint32_t total_waste = 0;

	pr_info("File information:\n"
		"\tfd: %u\n"
		"\tsize: %lld\n"
		"\tblocks number: %llu\n"
		"\tblocks:\n",
		fd, inode->i_size, inode->i_blocks);

	/* Read index block from disk */
	bh_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_index) {
		ret = -EFAULT;
		pr_err("could not read index block\n");
		goto free_file;
	}
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	for (int i = 0; i < inode->i_blocks - 1; i++) {
		block = index->blocks[i];
		size_block = get_block_size(block);
		wasted = OUICHEFS_BLOCK_SIZE - size_block;
		total_waste += wasted;
		if (wasted != 0)
			nb_partial_block++;

		pr_cont("%u:%u", get_block_number(block), size_block);
		if (i < inode->i_blocks - 2)
			pr_cont(", ");
		if (i % 10 == 9)
			pr_cont("\n");
	}

	pr_cont("\n\twasted: %d\n"
		"\tpartial block: %d\n",
		total_waste, nb_partial_block);

	brelse(bh_index);

free_file:
	fput(user_file);

end:
	return ret;
}

long ouichefs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != OUICHEFS_IOCTL_MAGIC)
		return -EINVAL;

	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case OUICHEFS_IOC_FILE_INFO:
		return ouichefs_ioctl_file_info(file, argp);
	default:
		return -EINVAL;
	}

	return 0;
}

const struct file_operations ouichefs_ioctl_ops = { .unlocked_ioctl =
							    ouichefs_ioctl };
