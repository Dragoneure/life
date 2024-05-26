#ifndef _IOCTL_H
#define _IOCTL_H

/* Ioctl commands / structures, shared with user programs */

struct file_info {
	int wasted;
	int nb_blocks;
	int hide_display;
};

#define OUICHEFS_IOCTL_MAGIC 'N'
#define OUICHEFS_IOC_FILE_INFO _IOWR(OUICHEFS_IOCTL_MAGIC, 1, struct file_info)
#define OUICHEFS_IOC_DEFRAG _IO(OUICHEFS_IOCTL_MAGIC, 2)
#define OUICHEFS_IOC_FILE_BLOCK_PRINT _IO(OUICHEFS_IOCTL_MAGIC, 3)

#endif /* IOCTL_H */
