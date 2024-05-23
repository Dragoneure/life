#include "utils.h"
#include <stdio.h>

#define LEN_INIT 2000000
#define LEN_INSERT 1000000
#define OFFSET 500000

char wbuf_init[LEN_INIT];
char wbuf_insert[LEN_INSERT];

void init_buffers()
{
	int start = 0;
	init_seq_buff(wbuf_init, LEN_INIT, &start);
	init_rand_buf(wbuf_insert, LEN_INSERT);
}

/*
 * Check write performance with our optimisation
 */

void bench_insert_user()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	time_t w_time = 0;

	set_write_fn(SIMPLE_WRITE);
	set_read_fn(SIMPLE_READ);
	init_buffers();

	/* Initial content */
	write(fd, wbuf_init, LEN_INIT);

	/* To insert in the user, get old content and write it at the end */
	size_t file_size = lseek(fd, 0, SEEK_END);
	size_t len_old = file_size - OFFSET;
	char rbuf_old[len_old];

	struct time_data t;
	TIME_START(t);

	/* Shift old content */
	lseek(fd, OFFSET, SEEK_SET);
	read(fd, rbuf_old, len_old);
	lseek(fd, OFFSET + LEN_INSERT, SEEK_SET);
	write(fd, rbuf_old, len_old);

	/* Write new content */
	lseek(fd, OFFSET, SEEK_SET);
	write(fd, wbuf_insert, LEN_INSERT);

	TIME_END(t);
	w_time = t.diff;

	pr_test("Total write time without optimization: %ld ms\n",
		w_time / 1000);
}

void bench_insert_lite_write()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	time_t w_time = 0;

	set_write_fn(SIMPLE_WRITE);
	init_buffers();

	set_write_fn(LIGHT_WRITE);
	set_read_fn(LIGHT_READ);

	/* Initial content */
	write(fd, wbuf_init, LEN_INIT);

	struct time_data t;
	TIME_START(t);

	/* Insert content in file */
	lseek(fd, OFFSET, SEEK_SET);
	write(fd, wbuf_insert, LEN_INSERT);

	TIME_END(t);
	w_time = t.diff;

	pr_test("Total write time with optimization: %ld ms\n", w_time / 1000);
}

/*
 * Check read performance after defragmentation
 */

void bench_insert_defrag()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	time_t r_time = 0;
	time_t r_time_defrag = 0;

	size_t len = 256;
	size_t total_len = MAX_FILESIZE;
	char wbuf[len];
	int start = 0;
	int written = 0;
	init_seq_buff(wbuf, len, &start);

	for (int i = 0; i < total_len; i += len) {
		lseek(fd, 0, SEEK_SET);
		written = write(fd, wbuf, len);
		if (written == 0)
			break;
	}

	char rbuf[total_len];

	struct time_data t;
	TIME_START(t);
	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, total_len);
	TIME_END(t);
	r_time = t.diff;

	SHOW_FILE_INFO(fd);
	DEFRAG_FILE(fd);
	SHOW_FILE_INFO(fd);

	TIME_START(t);
	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, total_len);
	TIME_END(t);
	r_time_defrag = t.diff;

	pr_test("Total read time before defrag: %ld us\n", r_time);
	pr_test("Total read time after defrag: %ld us\n", r_time_defrag);
}

int main(int argc, char **argv)
{
	// RUN_BENCH(bench_insert_user);
	// RUN_BENCH(bench_insert_lite_write);
	RUN_BENCH(bench_insert_defrag);
	return 0;
}
