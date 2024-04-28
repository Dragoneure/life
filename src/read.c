#include "linux/buffer_head.h"
#include "linux/types.h"
#include "ouichefs.h"

ssize_t ouichefs_read(struct file *file, char __user *buff, size_t size,
		      loff_t *pos)
{
	pr_info("size to read : %lu it is new\n", size);
	pr_info("I am in ouichefs_read\n");
	struct buffer_head *bh_index, *bh_data;
	struct ouichefs_file_index_block *index;
	/*
	 * block logical number of the first character to be read
	 * f_pos (cursor) /  4 KiB
	 */
	int block_logical_number = (*pos) >> 12;
	/*
   * the pos cursor inside a block
	 */
	int logical_pos = (*pos) % (1 << 12);

	pr_info("pos : %lld\n", *pos);
	struct inode *i = file->f_inode;
	struct ouichefs_inode_info *oi = OUICHEFS_INODE(i);
	sector_t index_block = oi->index_block;
	ssize_t total_read = 0;
	ssize_t remaining_read = size;
	int read = 0;
	int cpt_nb_blocs = 0;
	sector_t sc;
	/*
	 * Amount of data to be read inside one bloc
	 */
	int block_size_read;
	/*
	 * Read index block from disk
	 */
	bh_index = sb_bread(i->i_sb, index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;
	/*
	 * Concurency has been managed in upper layer (ksys_read), no need
	 * to manage it here, we are already  safe here
 	 */

	/*
 	 * Each loop will read max 1 bloc of data
 	 */
	if (!index) {
		pr_info("index null\n");
		return 0;
	}

	for (int j = 0; j < i->i_blocks; i++)
		pr_info("block : %d\n", index->blocks[j]);
	pr_info("about to enter into while\n");
	while ((total_read != size) && (cpt_nb_blocs <= i->i_blocks)) {
		pr_info("block logical number : %d\n", block_logical_number);
		sc = (sector_t)index->blocks[0];
		pr_info("sc : %lld\n", sc);
		bh_data = sb_bread(i->i_sb, sc);
		if (bh_data == NULL)
			break;
		pr_info("passed sb_read\n");
		if ((remaining_read + logical_pos) > bh_data->b_size)
			block_size_read = bh_data->b_size - logical_pos;
		else
			block_size_read = remaining_read;
		pr_info("about to copy data\n");
		read = copy_to_user(buff + (size - remaining_read - 1),
				    bh_data + logical_pos, block_size_read);
		pr_info("finish to read\n");
		pr_info("reading block : %d\n",
			index->blocks[block_logical_number]);
		remaining_read -= read;
		total_read += read;
		block_logical_number++;
		/*
		 * If we read a new bloc, for sure the cursor position in this bloc 
		 * will be at the begining
		 */
		logical_pos = 0;
		brelse(bh_data);
	}
	brelse(bh_index);
	*pos += total_read;
	return total_read;
}
