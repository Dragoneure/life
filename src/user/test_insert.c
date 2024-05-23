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
	ASSERT_FILE(fd, 2, (BLOCK_SIZE - prev_len) + (BLOCK_SIZE - len));

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
	ASSERT_FILE(fd, 1, BLOCK_SIZE - (prev_len + next_len));

	lseek(fd, prev_len, SEEK_SET);
	write(fd, insert_write, insert_len);
	ASSERT_FILE(fd, 2,
		    (BLOCK_SIZE - (prev_len + insert_len)) +
			    (BLOCK_SIZE - next_len));

	DEFRAG_FILE(fd);
	SHOW_FILE_INFO(fd);
	ASSERT_FILE(fd, 1, (BLOCK_SIZE - (prev_len + next_len + insert_len)));

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

	ASSERT_FILE(fd, 3, BLOCK_SIZE * 3 - len * 3);
	DEFRAG_FILE(fd);
	ASSERT_FILE(fd, 1, BLOCK_SIZE - len * 3);

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
	ASSERT_FILE(fd, 1, (BLOCK_SIZE - (404 + len)));

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

/* Test the write in padding optimization */
int test_write_in_padding()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	char wbuf[] = "The disco-dancing banana slipped on a rainbow.";
	size_t len = strlen(wbuf);
	char empty_buf[len];
	int start = 0;
	init_seq_buff(empty_buf, len, &start);
	int write_empty = 0, size = (len + 5) * 500;

	for (int i = 0; i < size; i += len + 5) {
		lseek(fd, i, SEEK_SET);
		if (write_empty) {
			write(fd, empty_buf, len);
		} else {
			write(fd, wbuf, len);
		}
		write_empty = !write_empty;
	}

	SHOW_FILE_INFO(fd);

	char rbuf[len];
	write_empty = 0;

	for (int i = 0; i < size; i += len + 5) {
		lseek(fd, i, SEEK_SET);
		read(fd, rbuf, len);
		if (write_empty) {
			ASSERT_EQ_BUF(rbuf, empty_buf, len);
		} else {
			ASSERT_EQ_BUF(rbuf, wbuf, len);
		}
		write_empty = !write_empty;
	}

	return TEST_SUCCESS;
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
	RUN_TEST(test_write_in_padding);

	return 0;
}
