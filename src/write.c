#include "linux/buffer_head.h"
#include "linux/compiler.h"
#include "linux/gfp_types.h"
#include "linux/minmax.h"
#include "linux/slab.h"
#include "linux/uaccess.h"
#include "ouichefs.h"
#include "bitmap.h"
#include "write.h"

/*int shift(struct file *file, size_t size, loff_t *pos) {        
        size_t block_logical_number = (*pos) >> 12;
        size_t size_fic = file-> f_inode->i_size;
        size_t nb_block_file = size_fic / OUICHEFS_BLOCK_SIZE;
        size_t logical_pos = (*pos) % OUICHEFS_BLOCK_SIZE;
        size_t remaining_read = OUICHEFS_BLOCK_SIZE - logical_pos;
        if ((size_fic % OUICHEFS_BLOCK_SIZE) > 0)
                nb_block_file++;
        if (*pos >= size_fic) {
                return 0;
        }
        int remaining_shift = size-= remaining_read;
        while(remaining_shift > 0) {
                struct buffer_head *bh_result = NULL;
                int ret = ouichefs_file_get_block(file->f_inode, nb_block_file+1, bh_result, true);
                if (ret < 0) {
                        return 1;
                }
                nb_block_file++;
                remaining_shift -= 1>>12;

        }

}*/


int check_begin(struct file * file, loff_t pos, size_t len) {
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);
	uint32_t nr_allocs = 0;

	/* Check if the write can be completed (enough space?) */
	if (pos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max((uint32_t)(pos + len), (uint32_t)file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;
        return 0;
}

void check_end(struct file *file) {
        struct inode *inode = file->f_inode;
        struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
        struct super_block *sb = inode->i_sb;

        /* Complete the write() */
        uint32_t nr_blocks_old = inode->i_blocks;

        /* Update inode metadata */
        inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
        inode->i_mtime = inode->i_ctime = current_time(inode);
        mark_inode_dirty(inode);

        /* If file is smaller than before, free unused blocks */
        if (nr_blocks_old > inode->i_blocks) {
                int i;
                struct buffer_head *bh_index;
                struct ouichefs_file_index_block *index;

                /* Free unused blocks from page cache */
                truncate_pagecache(inode, inode->i_size);

                /* Read index block to remove unused blocks */
                bh_index = sb_bread(sb, ci->index_block);
                if (!bh_index) {
                        pr_err("failed truncating '%s'. we just lost %llu blocks\n",
                               file->f_path.dentry->d_name.name,
                               nr_blocks_old - inode->i_blocks);
                }
                index = (struct ouichefs_file_index_block *)
                bh_index->b_data;

                for (i = inode->i_blocks - 1; i < nr_blocks_old - 1;
                i++) {
                        put_block(OUICHEFS_SB(sb), index->blocks[i]);
                        index->blocks[i] = 0;
                }
                mark_buffer_dirty(bh_index);
                brelse(bh_index);
        }
}


ssize_t ouichefs_write(struct file *file, const char __user *buff, size_t size,
		      loff_t *pos)
{ 
        if (!check_begin(file, *pos, size)) {
                return 0;
        }
        size_t block_logical_number = (*pos) >> 12;
        size_t size_fic = file-> f_inode->i_size;
        size_t nb_block_file = size_fic / OUICHEFS_BLOCK_SIZE;
        size_t logical_pos = (*pos) % OUICHEFS_BLOCK_SIZE;
        size_t remaining_read = OUICHEFS_BLOCK_SIZE - logical_pos;
        if ((size_fic % OUICHEFS_BLOCK_SIZE) > 0)
                nb_block_file++;
        if (*pos > size_fic) {
                while(block_logical_number > (nb_block_file + 1)) {
                        struct buffer_head *bh_result = NULL;
                        int ret = ouichefs_file_get_block(file->f_inode, nb_block_file+1, bh_result, true);
                        memset(bh_result->b_data,0,1 << 12);
                        if (ret < 0) {
                                return 0;
                        }

                        nb_block_file++;
                }
        }
        size_t total_write = 0;
        struct buffer_head *bh_result = NULL;
        int ret = ouichefs_file_get_block(file->f_inode, block_logical_number, bh_result, true);
        if (ret < 0) {
                goto end;
        }
        memset(bh_result->b_data,0,1 << 12);
        size_t size_to_write = min(remaining_read,size);
        size_t size_fail_write = copy_from_user(bh_result->b_data+logical_pos, buff, size_to_write);
        total_write += size_to_write - size_fail_write;
        if (size_fail_write > 0) {
                return total_write;
        }
        block_logical_number++;
        while(total_write < size) {
                int ret = ouichefs_file_get_block(file->f_inode, block_logical_number, bh_result, true);
                if (ret < 0) {
                        goto end;
                }
                size_t size_to_write = min((size_t)OUICHEFS_BLOCK_SIZE,size);
                size_t size_fail_write = copy_from_user(bh_result->b_data, buff, size_to_write);
                total_write += size_to_write - size_fail_write;
                if (size_fail_write > 0) {
                        goto end;
                }
                block_logical_number++;
        }

end :
        check_end(file);
        return total_write;
}
