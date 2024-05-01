#ifndef _IOCTL_H
#define _IOCTL_H

/* Ioctl commands / structures, shared with user programs */

#define OUICHEFS_IOCTL_MAGIC 'N'

struct ouichefs_file_info {
	int foo;
};

#define OUICHEFS_IOC_FILE_INFO \
	_IOR(OUICHEFS_IOCTL_MAGIC, 1, struct ouichefs_file_info)

#endif /* IOCTL_H */
