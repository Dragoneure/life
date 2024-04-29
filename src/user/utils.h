#ifndef _UTILS_H
#define _UTILS_H

#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>

#define BLOCK_SIZE (1 << 12) /* 4 KiB */
#define MAX_FILESIZE (1 << 22) /* 4 MiB */

#define ANSI_GREEN "\x1b[32m"
#define ANSI_RED "\x1b[31m"
#define ANSI_RESET "\x1b[0m"

#define TEST_SUCCESS 0
#define TEST_FAIL 1

static inline void init_rand_file(FILE *f)
{
	for (int i = 0; i < MAX_FILESIZE; i++)
		fputc(rand() % CHAR_MAX, f);
}

static inline void pr_buf(const char *buf, size_t len)
{
	printf("\"");
	for (size_t i = 0; i < len; ++i) {
		if (isprint(buf[i])) {
			printf("%c", buf[i]);
		} else {
			printf("?");
		}
	}
	printf("\"");
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
		if (test_name(__VA_ARGS__) == 0) {                        \
			printf("%s ... " ANSI_GREEN "OK" ANSI_RESET "\n", \
			       #test_name);                               \
		} else {                                                  \
			printf("%s ... " ANSI_RED "FAIL" ANSI_RESET "\n", \
			       __func__);                                 \
		}                                                         \
	} while (0)

#define ASSERT_EQ(actual, expected)                                           \
	if (actual != expected) {                                             \
		pr_test(ANSI_RED "Comparison failed: \n" ANSI_RESET);         \
		printf("\tActual: %zu\n\tExpected: %zu\n", actual, expected); \
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

#endif /* UTILS_H */
