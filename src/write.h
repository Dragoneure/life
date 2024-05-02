#ifndef _WRITE_H
#define _WRITE_H
#include "linux/types.h"
#include "linux/buffer_head.h"
ssize_t ouichefs_write(struct file *file, const char __user *buff, size_t size,
		      loff_t *pos);
#endif // !write.10:59:32
