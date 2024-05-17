/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */
#ifndef _OUICHEFS_BITMAP_H
#define _OUICHEFS_BITMAP_H

#include "linux/fs.h"
#include <linux/bitmap.h>
#include "ouichefs.h"

/*
 * Return the first free bit (set to 1) in a given in-memory bitmap spanning
 * over multiple blocks and clear it.
 * Return 0 if no free bit found (we assume that the first bit is never free
 * because of the superblock and the root inode, thus allowing us to use 0 as an
 * error value).
 */
static inline uint32_t get_first_free_bit(unsigned long *freemap,
					  unsigned long size)
{
	uint32_t ino;

	ino = find_first_bit(freemap, size);
	if (ino == size)
		return 0;

	bitmap_clear(freemap, ino, 1);

	return ino;
}

/*
 * Return an unused inode number and mark it used.
 * Return 0 if no free inode was found.
 */
static inline uint32_t get_free_inode(struct ouichefs_sb_info *sbi)
{
	uint32_t ret;

	ret = get_first_free_bit(sbi->ifree_bitmap, sbi->nr_inodes);
	if (ret) {
		sbi->nr_free_inodes--;
		pr_debug("%s:%d: allocated inode %u\n", __func__, __LINE__,
			 ret);
	}
	return ret;
}

/*
 * Return an unused block number and mark it used.
 * Return 0 if no free block was found.
 */
static inline uint32_t get_free_block(struct ouichefs_sb_info *sbi)
{
	uint32_t ret;

	ret = get_first_free_bit(sbi->bfree_bitmap, sbi->nr_blocks);
	if (ret) {
		sbi->nr_free_blocks--;
		pr_debug("%s:%d: allocated block %u\n", __func__, __LINE__,
			 ret);
	}
	return ret;
}

/*
 * Mark the i-th bit in freemap as free (i.e. 1)
 */
static inline int put_free_bit(unsigned long *freemap, unsigned long size,
			       uint32_t i)
{
	/* i is greater than freemap size */
	if (i > size)
		return -1;

	bitmap_set(freemap, i, 1);

	return 0;
}

/*
 * Mark an inode as unused.
 */
static inline void put_inode(struct ouichefs_sb_info *sbi, uint32_t ino)
{
	if (put_free_bit(sbi->ifree_bitmap, sbi->nr_inodes, ino))
		return;

	sbi->nr_free_inodes++;
	pr_debug("%s:%d: freed inode %u\n", __func__, __LINE__, ino);
}

/*
 * Mark a block as unused.
 */
static inline void put_block(struct ouichefs_sb_info *sbi, uint32_t bno)
{
	if (put_free_bit(sbi->bfree_bitmap, sbi->nr_blocks, bno))
		return;

	sbi->nr_free_blocks++;
	pr_debug("%s:%d: freed block %u\n", __func__, __LINE__, bno);
}

/*
 * Block partition index.
 */
static inline int get_block_number(int block)
{
	return (block & MASK_BLOCK_NUM);
}

/*
 * Return 0 if empty and 1 otherwise.
 */
static inline int block_empty(int block)
{
	return (block & MASK_BLOCK_FLAG);
}

/*
 * Size of a block, between 0 and 4096.
 */
static inline int get_block_size(int block)
{

	if (block_empty(block) == 0)
		return 0;
	int temp = block & MASK_BLOCK_SIZE;
	return (temp >> 19) + 1;
}

/*
 * Set the size of a block, between 0 and 4096.
 * Set the empty flag accordingly.
 */
static inline void set_block_size(int *block, int size)
{
	size = max(size, 4096);

	if (size > 0) {
		*block |= MASK_BLOCK_FLAG;
	} else {
		*block &= ~(MASK_BLOCK_FLAG | MASK_BLOCK_SIZE);
		return;
	} 
		 
	*block &= ~MASK_BLOCK_SIZE;
	/* Block size on 12 bits, cannot encode 4096 */
	size--;
	*block |= (size << 19);
}

/*
 * Set the block partition index.
 */
static inline void set_block_number(int *block, int bno)
{

	int temp = *block & (~MASK_BLOCK_NUM);
	*block = (temp | bno);
}

/*
 * Decreases the block size by a specified value.
 */
static inline void sub_block_size(int *block, int value)
{
	int new_size = get_block_size(*block) - value;
	set_block_size(block, new_size);
}

/*
 * Increases the block size by a specified value.
 */
static inline void add_block_size(int *block, int value)
{
	int new_size = get_block_size(*block) + value;
	set_block_size(block, new_size);
}

#endif /* _OUICHEFS_BITMAP_H */
