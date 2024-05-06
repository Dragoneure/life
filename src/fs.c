// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018  Redha Gouicem <redha.gouicem@lip6.fr>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>

#include "ouichefs.h"
#include "debug.h"

/*
 * Mount a ouiche_fs partition
 */
struct dentry *ouichefs_mount(struct file_system_type *fs_type, int flags,
			      const char *dev_name, void *data)
{
	struct dentry *dentry = NULL;

	dentry =
		mount_bdev(fs_type, flags, dev_name, data, ouichefs_fill_super);
	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);

	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	kill_block_super(sb);

	pr_info("unmounted disk\n");
}

static struct file_system_type ouichefs_file_system_type = {
	.owner = THIS_MODULE,
	.name = "ouichefs",
	.mount = ouichefs_mount,
	.kill_sb = ouichefs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
	.next = NULL,
};

static int chr_major;

static int __init ouichefs_init(void)
{
	int ret;

	ret = ouichefs_init_inode_cache();
	if (ret) {
		pr_err("inode cache creation failed\n");
		goto err;
	}

	ret = register_filesystem(&ouichefs_file_system_type);
	if (ret) {
		pr_err("register_filesystem() failed\n");
		goto err_inode;
	}

	kobj_sysfs = kobject_create_and_add("ouichefs", kernel_kobj);
	if (!kobj_sysfs) {
		pr_err("kobject_create_and_add() failed\n");
		goto err_inode;
	}

	ret = sysfs_create_file(kobj_sysfs, &read_fn_attr.attr);
	if (ret) {
		pr_err("sysfs_create_file() for read_fn_attr failed\n");
		goto free_kobj;
	}
	ret = sysfs_create_file(kobj_sysfs, &write_fn_attr.attr);
	if (ret) {
		pr_err("sysfs_create_file() for write_fn_attr failed\n");
		goto free_kobj;
	}

	ret = register_chrdev(0, "ouichefs-dev", &ouichefs_ioctl_ops);
	if (ret < 0) {
		pr_err("chrdev registration failed\n");
		goto free_kobj;
	}
	chr_major = ret;
	pr_info("chrdev registered with major %d\n", chr_major);

	pr_info("module loaded\n");
	return 0;

free_kobj:
	kobject_put(kobj_sysfs);
err_inode:
	ouichefs_destroy_inode_cache();
err:
	return ret;
}

static void __exit ouichefs_exit(void)
{
	int ret;

	kobject_put(kobj_sysfs);

	unregister_chrdev(chr_major, "ouichefs-dev");

	ret = unregister_filesystem(&ouichefs_file_system_type);
	if (ret)
		pr_err("unregister_filesystem() failed\n");

	ouichefs_destroy_inode_cache();

	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@rwth-aachen.de>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");
