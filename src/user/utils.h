#ifndef _UTILS_H
#define _UTILS_H

#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <sys/ioctl.h>
#include "../ioctl.h"

#define BLOCK_SIZE (1 << 12) /* 4 KiB */
#define MAX_FILESIZE (1 << 22) /* 4 MiB */

static inline int idiv_ceil(int a, int b)
{
	int ret;

	ret = a / b;
	if (a % b != 0)
		return ret + 1;
	return ret;
}

/* Change ouichefs read and write function using sysfs */

#define DEFAULT_READ '0'
#define SIMPLE_READ '1'
#define LIGHT_READ '2'
#define CACHED_READ '3'

static inline void set_read_fn(char read_fn)
{
	int fd = open("/sys/kernel/ouichefs/read_fn", O_WRONLY);
	write(fd, &read_fn, 1);
	close(fd);
}

static inline char get_read_fn()
{
	int fd = open("/sys/kernel/ouichefs/read_fn", O_RDONLY);
	char read_fn;
	read(fd, &read_fn, 1);
	close(fd);
	return read_fn;
}

#define DEFAULT_WRITE '0'
#define SIMPLE_WRITE '1'
#define LIGHT_WRITE '2'

static inline void set_write_fn(char write_fn)
{
	int fd = open("/sys/kernel/ouichefs/write_fn", O_WRONLY);
	write(fd, &write_fn, 1);
	close(fd);
}

static inline char get_write_fn()
{
	int fd = open("/sys/kernel/ouichefs/write_fn", O_RDONLY);
	char write_fn;
	read(fd, &write_fn, 1);
	close(fd);
	return write_fn;
}

/* Test utilities */

#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_BLUE "\x1b[34m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RED "\x1b[31m"
#define ANSI_RESET "\x1b[0m"

#define TEST_SUCCESS 0
#define TEST_FAIL 1

static inline void flush_cache(void)
{
	sync();
	int fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
	write(fd, "3", 1);
	close(fd);
}

static inline void init_seq_buff(char *buf, size_t len, int *counter)
{
	int offset = 0;
	while (offset < len - 1) {
		int written =
			snprintf(buf + offset, len - offset, "%d ", *counter);
		if (written < 0)
			break;
		offset += written;
		*counter += 1;
	}
	buf[offset] = '\0';
}

static inline void init_seq_file(int fd)
{
	int counter = 0;
	size_t len = BLOCK_SIZE;
	char buf[len];
	for (int i = 0; i < MAX_FILESIZE; i += len) {
		init_seq_buff(buf, len, &counter);
		write(fd, buf, len);
	}
}

static inline void init_rand_buf(char *buf, size_t len)
{
	for (int i = 0; i < len; i++)
		buf[i] = (char)(rand() % CHAR_MAX);
}

static inline void init_rand_file(int fd)
{
	size_t len = BLOCK_SIZE;
	char buf[len];
	for (int i = 0; i < MAX_FILESIZE; i += len) {
		init_rand_buf(buf, len);
		write(fd, buf, len);
	}
}

static inline void pr_buf(const char *buf, size_t len)
{
	printf("\"");
	int new_line = 0;
	for (size_t i = 0; i < len; i++) {
		if (new_line > 150) {
			printf("\n");
			new_line = 0;
		}
		if (isprint(buf[i])) {
			printf("%c", buf[i]);
			new_line++;
			continue;
		} else if (buf[i] == '\0') {
			printf("\\0");
			new_line += 2;
		} else if (buf[i] == '\n') {
			printf("\\n");
			new_line += 2;
		} else {
			printf("?");
			new_line++;
		}
	}
	printf("\"");
}

static inline void pr_file(int fd, int offset, int file_size)
{
	int len = file_size - offset;
	char rbuf[len];
	lseek(fd, offset, SEEK_SET);
	read(fd, rbuf, len);
	pr_buf(rbuf, len);
	printf("\n");
}

#define pr_test(fmt, ...) \
	printf("%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define RUN_BENCH(bench_name, ...)                                \
	do {                                                      \
		bench_name(__VA_ARGS__);                          \
		printf("%s ... " ANSI_GREEN "OK" ANSI_RESET "\n", \
		       #bench_name);                              \
	} while (0)

#define RUN_TEST(test_name, ...)                                          \
	do {                                                              \
		int old_read_fn = get_read_fn();                          \
		int old_write_fn = get_write_fn();                        \
		printf("RUNNING %s\n", #test_name);                       \
		if (test_name(__VA_ARGS__) == TEST_SUCCESS) {             \
			printf("%s ... " ANSI_GREEN "OK" ANSI_RESET "\n", \
			       #test_name);                               \
		} else {                                                  \
			printf("%s ... " ANSI_RED "FAIL" ANSI_RESET "\n", \
			       __func__);                                 \
		}                                                         \
		set_read_fn(old_read_fn);                                 \
		set_write_fn(old_write_fn);                               \
	} while (0)

#define ASSERT_EQ(actual, expected)                                           \
	if (actual != expected) {                                             \
		pr_test(ANSI_RED "Comparison failed: \n" ANSI_RESET);         \
		printf("\tActual: %zi\n\tExpected: %zi\n", actual, expected); \
		return TEST_FAIL;                                             \
	}

#define ASSERT_EQ_BUF(buf_actual, buf_expected, len)                         \
	if (memcmp(buf_actual, buf_expected, len) != 0) {                    \
		pr_test(ANSI_RED "Buffer comparison failed: \n" ANSI_RESET); \
		printf("\tActual: ");                                        \
		pr_buf(buf_actual, len);                                     \
		printf("\n\tExpected: ");                                    \
		pr_buf(buf_expected, len);                                   \
		printf("\n");                                                \
		return TEST_FAIL;                                            \
	}

static inline int assert_file_info(int fd, int nb_blocks_expected,
				   int wasted_expected)
{
	int ret = 0;

	struct file_info info = { .hide_display = 1 };
	ioctl(fd, OUICHEFS_IOC_FILE_INFO, &info);

	if (nb_blocks_expected != info.nb_blocks) {
		printf(ANSI_RED "Comparison nb_blocks failed: \n" ANSI_RESET);
		printf("\tActual: %d\n\tExpected: %d\n", info.nb_blocks,
		       nb_blocks_expected);
		ret = 1;
	}

	if (wasted_expected != info.wasted) {
		printf(ANSI_RED "Comparison wasted failed: \n" ANSI_RESET);
		printf("\tActual: %d\n\tExpected: %d\n", info.wasted,
		       wasted_expected);
		ret = 1;
	}

	return ret;
}

#define ASSERT_FILE(fd, nb_blocks_expected, nb_wasted_expected)             \
	if (assert_file_info(fd, nb_blocks_expected, nb_wasted_expected)) { \
		return TEST_FAIL;                                           \
	}

/* Time utilities for benchmarking */

struct time_data {
	struct timespec start;
	struct timespec end;
	time_t diff; // in us
};

#define TIME_START(time) clock_gettime(CLOCK_MONOTONIC, &time.start);

#define TIME_END(time)                                                  \
	do {                                                            \
		clock_gettime(CLOCK_MONOTONIC, &time.end);              \
		time.diff = ((time.end.tv_sec - time.start.tv_sec) *    \
				     1000000000UL +                     \
			     (time.end.tv_nsec - time.start.tv_nsec)) / \
			    1000;                                       \
	} while (0)

/* Ioctl commands */

#define SHOW_FILE_INFO(fd)                                \
	do {                                              \
		struct file_info info;                    \
		info.hide_display = 0;                    \
		ioctl(fd, OUICHEFS_IOC_FILE_INFO, &info); \
	} while (0)

#define DEFRAG_FILE(fd)                         \
	do {                                    \
		ioctl(fd, OUICHEFS_IOC_DEFRAG); \
	} while (0)

#endif /* UTILS_H */
