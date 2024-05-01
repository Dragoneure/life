#include "ouichefs.h"

static int ouichefs_ioctl_file_info(struct file *file, void __user *argp)
{
	struct ouichefs_file_info file_info = { 0 };
	int ret = 0;

	file_info.foo = 32;

	if (copy_to_user(argp, &file_info, sizeof(file_info)))
		ret = -EFAULT;

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

struct file_operations ouichefs_ioctl_ops = { .unlocked_ioctl =
						      ouichefs_ioctl };
