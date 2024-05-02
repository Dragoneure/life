#ifndef _DEBUG_H
#define _DEBUG_H

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "ouichefs.h"

#define DEFAULT_READ '0'
#define SIMPLE_READ '1'

#define DEFAULT_WRITE '0'
#define SIMPLE_WRITE '1'

/* sysfs */

static char read_fn = DEFAULT_READ;
static uint8_t write_fn = DEFAULT_WRITE;

static ssize_t read_fn_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	switch (buf[0]) {
	case DEFAULT_READ:
		ouichefs_file_ops.read = NULL;
		break;
	case SIMPLE_READ:
		ouichefs_file_ops.read = ouichefs_read;
		break;
	default:
		pr_err("Invalid read fn code\n");
		return count;
	}

	read_fn = buf[0];
	return count;
}

static ssize_t read_fn_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%c\n", read_fn);
}

static ssize_t write_fn_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	switch (buf[0]) {
	case DEFAULT_WRITE:
		ouichefs_file_ops.write = NULL;
		break;
	case SIMPLE_WRITE:
		ouichefs_file_ops.write = ouichefs_write;
		break;
	default:
		pr_err("Invalid write fn code\n");
		return count;
	}

	write_fn = buf[0];
	return count;
}

static ssize_t write_fn_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%c\n", write_fn);
}

extern struct kobject *kernel_kobj;
static struct kobj_attribute read_fn_attr = __ATTR_RW(read_fn);
static struct kobj_attribute write_fn_attr = __ATTR_RW(write_fn);
static struct kobject *kobj_sysfs;

#endif /* _DEBUG_H */
