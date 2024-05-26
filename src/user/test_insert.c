#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Automated tests
 */

int test_hello_world()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	char buf1[] = "Hello\n";
	char buf2[] = "World\n";
	size_t total_len = strlen(buf1) + strlen(buf2);

	write(fd, buf1, strlen(buf1));

	close(fd);
	fd = open(__func__, O_RDWR | O_CREAT | O_APPEND, 0644);

	write(fd, buf2, strlen(buf2));

	char rbuf[total_len];

	DEFRAG_FILE(fd);

	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, total_len);
	ASSERT_EQ_BUF(rbuf, buf1, strlen(buf1));
	ASSERT_EQ_BUF(rbuf + strlen(buf1), buf2, strlen(buf2));

	size_t file_size = lseek(fd, 0, SEEK_END);
	ASSERT_EQ(file_size, total_len);

	return TEST_SUCCESS;
}

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

	DEFRAG_FILE(fd);

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
	DEFRAG_FILE(fd);
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
int test_write_in_padding(int fd, int start_pos, int file_size,
			  int write_offset)
{
	char wbuf[] = "The disco-dancing banana slipped on a rainbow.";
	size_t len = strlen(wbuf);
	char empty_buf[len];
	int start = 0;
	init_seq_buff(empty_buf, len, &start);
	int write_empty = 0, size = 0, nb_blocks = 0;

	for (int i = start_pos; i < file_size; i += len + write_offset) {
		lseek(fd, i, SEEK_SET);
		if (write_empty) {
			write(fd, empty_buf, len);
		} else {
			write(fd, wbuf, len);
		}
		write_empty = !write_empty;
	}

	size = lseek(fd, 0, SEEK_END);
	nb_blocks = idiv_ceil(size, BLOCK_SIZE);
	ASSERT_FILE(fd, nb_blocks, (nb_blocks * BLOCK_SIZE) - size);

	DEFRAG_FILE(fd);
	ASSERT_FILE(fd, nb_blocks, (nb_blocks * BLOCK_SIZE) - size);

	char rbuf[len];
	write_empty = 0;

	for (int i = start_pos; i < file_size; i += len + write_offset) {
		lseek(fd, i, SEEK_SET);
		int readen = read(fd, rbuf, len);
		if (write_empty) {
			ASSERT_EQ_BUF(rbuf, empty_buf, readen);
		} else {
			ASSERT_EQ_BUF(rbuf, wbuf, readen);
		}
		write_empty = !write_empty;
	}

	return TEST_SUCCESS;
}

int test_write_continuous()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	return test_write_in_padding(fd, 0, BLOCK_SIZE * 10, 0);
}

int test_write_with_offset()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	return test_write_in_padding(fd, 0, BLOCK_SIZE * 10, 13);
}

int test_write_with_offset_far()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	return test_write_in_padding(fd, BLOCK_SIZE * 12 + 87,
				     BLOCK_SIZE * 12 + 287, 13);
}

int test_write_with_offset_end()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	int ret =
		test_write_in_padding(fd, MAX_FILESIZE - 70, MAX_FILESIZE, 22);
	size_t file_size = lseek(fd, 0, SEEK_END);
	ASSERT_EQ(file_size, (size_t)MAX_FILESIZE - 24);
	return ret;
}

/*
 * Visual tests
 */

int test_big_content()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	size_t len = 3 * BLOCK_SIZE + BLOCK_SIZE / 1.2;
	char wbuf[len];
	int start = 0;
	init_seq_buff(wbuf, len, &start);

	write(fd, wbuf, len);

	lseek(fd, BLOCK_SIZE * 7 + 7, SEEK_SET);
	write(fd, wbuf, len);

	printf("\nBefore defragmentation:\n");
	SHOW_FILE_INFO(fd);
	printf("\n");

	DEFRAG_FILE(fd);

	printf("After defragmentation:\n");
	SHOW_FILE_INFO(fd);
	printf("\n");

	size_t file_size = lseek(fd, 0, SEEK_END);
	pr_file(fd, 0, file_size);

	return TEST_SUCCESS;
}

int test_write_user_simple()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	char buf1[] = "first ";
	char buf2[] = "second ";
	char buf3[] = "third ";
	char buf4[] = "forth ";
	char buf5[] = "fifth ";
	char buf_middle[] = "|in the middle|";
	char buf_end[] = "at the end";
	char buf_bef_far[] = "|just before so far away..|";
	char buf_far[] = "so far away";
	size_t far_offset = BLOCK_SIZE * 4 - 3;

	write(fd, buf1, strlen(buf1));
	lseek(fd, 0, SEEK_SET);
	write(fd, buf2, strlen(buf2));
	lseek(fd, 0, SEEK_SET);
	write(fd, buf3, strlen(buf3));
	lseek(fd, 0, SEEK_SET);
	write(fd, buf4, strlen(buf4));
	lseek(fd, 0, SEEK_SET);
	write(fd, buf5, strlen(buf5));

	lseek(fd, 0, SEEK_END);
	write(fd, buf_end, strlen(buf_end));

	lseek(fd, 20, SEEK_SET);
	write(fd, buf_middle, strlen(buf_middle));

	lseek(fd, far_offset, SEEK_SET);
	write(fd, buf_far, strlen(buf_far));

	lseek(fd, far_offset, SEEK_SET);
	write(fd, buf_bef_far, strlen(buf_bef_far));

	printf("\nBefore defragmentation:\n");
	SHOW_FILE_INFO(fd);

	DEFRAG_FILE(fd);

	printf("After defragmentation:\n");
	SHOW_FILE_INFO(fd);
	printf("\n");

	int file_size = lseek(fd, 0, SEEK_END);
	printf("file size : %d\n", file_size);
	pr_file(fd, 0, file_size);

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

	/* automated tests */
	RUN_TEST(test_hello_world);
	RUN_TEST(test_write_insert_begin);
	RUN_TEST(test_write_insert);
	RUN_TEST(test_defrag);
	RUN_TEST(test_write_end);
	RUN_TEST(test_write_begin_block);
	RUN_TEST(test_write_far);
	RUN_TEST(test_write_continuous);
	RUN_TEST(test_write_with_offset);
	RUN_TEST(test_write_with_offset_far);
	RUN_TEST(test_write_with_offset_end);

	/* visual tests */
	RUN_TEST(test_big_content);
	RUN_TEST(test_write_user_simple);

	return 0;
}
