#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int test_write_insert_begin()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	char prev_write[] = "I love to eat chocolate\n";
	char wbuf[] = "Hello World\n";
	size_t prev_len = strlen(prev_write);
	size_t len = strlen(wbuf);

	write(fd, prev_write, prev_len);
	lseek(fd, 0, SEEK_SET);
	write(fd, wbuf, len);

	char rbuf[prev_len + len];

	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, prev_len + len);

	ASSERT_EQ_BUF(rbuf, wbuf, len);
	ASSERT_EQ_BUF(rbuf + len, prev_write, prev_len);

	return TEST_SUCCESS;
}

int test_write_insert()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	char prev_write[] = "I love to eat chocolate\n";
	char next_write[] = "I don't like to eat chocolate\n";
	char insert_write[] = "I love banana\n";
	size_t prev_len = strlen(prev_write);
	size_t next_len = strlen(next_write);
	size_t insert_len = strlen(insert_write);

	char rbuf[prev_len + next_len + insert_len];
	char first_write[prev_len + next_len];
	memcpy(first_write, prev_write, prev_len);
	memcpy(first_write + prev_len, next_write, next_len);

	write(fd, first_write, prev_len + next_len);

	lseek(fd, prev_len, SEEK_SET);
	write(fd, insert_write, insert_len);

	DEFRAG_FILE(fd);

	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, prev_len + next_len + insert_len);

	ASSERT_EQ_BUF(rbuf, prev_write, prev_len);
	ASSERT_EQ_BUF(rbuf + prev_len, insert_write, insert_len);
	ASSERT_EQ_BUF(rbuf + prev_len + insert_len, next_write, next_len);

	return TEST_SUCCESS;
}

int test_defrag()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	char wbuf[] = "The disco-dancing banana slipped on a rainbow.";
	size_t len = strlen(wbuf);
	int iterations = 3;

	for (int i = 0; i < iterations; i++) {
		lseek(fd, 0, SEEK_SET);
		write(fd, wbuf, len);
	}

	char rbuf[len];

	lseek(fd, 0, SEEK_SET);
	for (int i = 0; i < iterations; i++) {
		read(fd, rbuf, len);
		ASSERT_EQ_BUF(rbuf, wbuf, len);
	}

	DEFRAG_FILE(fd);

	return TEST_SUCCESS;
}

int test_write_pos(int fd, int SEEK, int offset)
{
	write(fd, "HAHA", 4);
	char wbuf[] = "The disco-dancing banana slipped on a rainbow.";
	size_t len = strlen(wbuf);
	char rbuf[100];

	ASSERT_FILE(fd, 1, BLOCK_SIZE - 4);
	int cur = lseek(fd, 400, SEEK_END);
	write(fd, wbuf, len);
	ASSERT_FILE(fd, 2, (BLOCK_SIZE - 4) + (BLOCK_SIZE - len - 400));

	lseek(fd, cur, SEEK_SET);
	read(fd, rbuf, len);

	ASSERT_EQ_BUF(rbuf, wbuf, len);

	return TEST_SUCCESS;
}

int test_write_end()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	return test_write_pos(fd, SEEK_END, 400);
}

int test_write_begin_block()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	return test_write_pos(fd, SEEK_SET, 4096);
}

int test_write_far()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	return test_write_pos(fd, SEEK_SET, 18075);
}

int main(int argc, char **argv)
{
	int seed = 42;
	/* Use the provided seed */
	if (argc > 1)
		seed = atoi(argv[1]);

	srand(seed);
	pr_test("Seed used: %d\n", seed);

	RUN_TEST(test_write_insert_begin);
	RUN_TEST(test_write_insert);
	RUN_TEST(test_defrag);
	RUN_TEST(test_write_end);
	RUN_TEST(test_write_begin_block);
	RUN_TEST(test_write_far);
	return 0;
}
