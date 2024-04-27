#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE (1 << 12) /* 4 KiB */
#define MAX_FILESIZE (1 << 22) /* 4 MiB */

#define ANSI_GREEN "\x1b[32m"
#define ANSI_RED "\x1b[31m"
#define ANSI_RESET "\x1b[0m"

#define TEST_SUCCESS 0
#define TEST_FAIL 1

#define pr_test(fmt, ...) \
	printf("%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)

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

void init_rand_file(FILE *f)
{
	for (int i = 0; i < MAX_FILESIZE; i++)
		fputc(rand() % CHAR_MAX, f);
}

void pr_buf(const char *buf, size_t len)
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

#define ASSERT_BUF_EQ(buf_actual, buf_expected, len)                         \
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

#define TIME_END(time)                                                   \
	clock_gettime(CLOCK_MONOTONIC, &time.end);                       \
	time.diff = (time.end.tv_sec - time.start.tv_sec) * 1000000000 + \
		    (time.end.tv_nsec - time.start.tv_nsec) / 1000;

int test_write_read()
{
	FILE *f = fopen(__func__, "w+");
	fseek(f, BLOCK_SIZE - 5, SEEK_SET);

	long pos = ftell(f);
	char wbuf[] = "Hello cruel world!";
	size_t len = strlen(wbuf);

	struct time_data t;
	TIME_START(t);
	fwrite(wbuf, sizeof(char), len, f);
	TIME_END(t);
	pr_test("Time taken for write: %ld us\n", t.diff);

	char rbuf[len];
	fseek(f, pos, SEEK_SET);
	fread(rbuf, sizeof(char), len, f);

	ASSERT_BUF_EQ(rbuf, wbuf, len);
	return TEST_SUCCESS;
}

int test_rand_read(int read1, int read2)
{
	int read_fn[] = { read1, read2 };
	FILE *f = fopen(__func__, "w+");
	init_rand_file(f);

	size_t len = 256;
	int round = 100;
	char expect[2][len];

	for (int i = 0; i < round; i++) {
		long rand_pos = rand() % MAX_FILESIZE;
		// TODO: set first and second read fn
		fseek(f, rand_pos, SEEK_SET);
		fread(expect[0], sizeof(char), len, f);
		fseek(f, rand_pos, SEEK_SET);
		fread(expect[1], sizeof(char), len, f);
		ASSERT_BUF_EQ(expect[0], expect[1], len);
	}

	return TEST_SUCCESS;
}

int test_insert()
{
	FILE *f = fopen(__func__, "w+");
	init_rand_file(f);

	char wbuf[] = "Hello cruel world!!!";
	size_t len = strlen(wbuf);

	char prev_rbuf[3][len];
	char rbuf[3][len];
	long pos = BLOCK_SIZE / 3;

	fseek(f, pos - len, SEEK_SET);
	for (int i = 0; i < 3; i++)
		fread(prev_rbuf[i], sizeof(char), len, f);

	fseek(f, pos, SEEK_SET);
	fwrite(wbuf, sizeof(char), len, f);

	fseek(f, pos - len, SEEK_SET);
	for (int i = 0; i < 3; i++)
		fread(rbuf[i], sizeof(char), len, f);

	ASSERT_BUF_EQ(rbuf[0], prev_rbuf[0], len);
	ASSERT_BUF_EQ(rbuf[1], wbuf, len);
	ASSERT_BUF_EQ(rbuf[2], prev_rbuf[2], len);

	for (int i = 0; i < 3; i++) {
		pr_test("rbuf content: ");
		pr_buf(rbuf[i], len);
		printf("\n");
	}

	return TEST_SUCCESS;
}

int main(int argc, char **argv)
{
	srand(42);
	RUN_TEST(test_write_read);
	RUN_TEST(test_insert);
	RUN_TEST(test_rand_read, 1, 2);
	return 0;
}
