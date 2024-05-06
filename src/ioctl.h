#ifndef _IOCTL_H
#define _IOCTL_H

/* Ioctl commands / structures, shared with user programs */

#define OUICHEFS_IOCTL_MAGIC 'N'
#define OUICHEFS_IOC_FILE_INFO _IOW(OUICHEFS_IOCTL_MAGIC, 1, unsigned int)

#endif /* IOCTL_H */
