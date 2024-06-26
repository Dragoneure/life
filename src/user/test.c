#include "utils.h"
#include <stddef.h>
#include <stdio.h>

int test_simple_file_write()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	char wbuf[] = "Hello cruel world!\n";
	size_t len = strlen(wbuf) + 1;
	write(fd, wbuf, len);

	char rbuf[len];
	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, len);
	ASSERT_EQ_BUF(rbuf, wbuf, len);

	return TEST_SUCCESS;
}

int test_write_read()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	size_t pos = lseek(fd, BLOCK_SIZE - 5, SEEK_SET);
	char wbuf[] = "Hello cruel world!\n";
	size_t len = strlen(wbuf) + 1;
	write(fd, wbuf, len);

	SHOW_FILE_INFO(fd);

	char rbuf[len];
	lseek(fd, pos, SEEK_SET);
	read(fd, rbuf, len);
	ASSERT_EQ_BUF(rbuf, wbuf, len);

	return TEST_SUCCESS;
}

int test_rand_read()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	init_rand_file(fd);
	flush_cache();

	size_t len = BLOCK_SIZE * 10;
	int round = 100;
	char expect[2][len];
	size_t readen[2];

	for (int i = 0; i < round; i++) {
		size_t rand_pos = rand() % MAX_FILESIZE;
		size_t rand_len = rand() % len;

		set_read_fn(DEFAULT_READ);
		lseek(fd, rand_pos, SEEK_SET);
		readen[0] = read(fd, expect[0], rand_len);

		set_read_fn(SIMPLE_READ);
		lseek(fd, rand_pos, SEEK_SET);
		readen[1] = read(fd, expect[1], rand_len);

		ASSERT_EQ(readen[0], readen[1]);
		ASSERT_EQ_BUF(expect[0], expect[1], readen[0]);
	}

	return TEST_SUCCESS;
}

int test_simple_read_insert()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	init_rand_file(fd);

	char wbuf[] = "Hello cruel world!!!";
	size_t len = strlen(wbuf);

	char prev_rbuf[3][len];
	char rbuf[3][len];
	size_t pos = BLOCK_SIZE / 3;

	lseek(fd, pos - len, SEEK_SET);
	for (int i = 0; i < 3; i++)
		read(fd, prev_rbuf[i], len);

	lseek(fd, pos, SEEK_SET);
	write(fd, wbuf, len);

	lseek(fd, pos - len, SEEK_SET);
	for (int i = 0; i < 3; i++)
		read(fd, rbuf[i], len);

	ASSERT_EQ_BUF(rbuf[0], prev_rbuf[0], len);
	ASSERT_EQ_BUF(rbuf[1], wbuf, len);
	ASSERT_EQ_BUF(rbuf[2], prev_rbuf[2], len);

	return TEST_SUCCESS;
}

int check_write_end(int fd, size_t end_pos)
{
	char wbuf[] =
		"Riding on a pancake spaceship, the syrupy crew soared through galaxies.";
	size_t len = strlen(wbuf);

	lseek(fd, end_pos - len, SEEK_SET);
	ASSERT_EQ(write(fd, wbuf, len), len);

	char rbuf[len];
	size_t end_offset = 16;

	lseek(fd, end_pos - end_offset, SEEK_SET);
	ASSERT_EQ(read(fd, rbuf, len), end_offset);
	ASSERT_EQ_BUF(rbuf, &wbuf[len - end_offset], end_offset);

	size_t filesize = lseek(fd, 0, SEEK_END);
	ASSERT_EQ(filesize, end_pos);

	return TEST_SUCCESS;
}

int test_write_filesize_end()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	return check_write_end(fd, MAX_FILESIZE);
}

int test_write_block_end()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	return check_write_end(fd, MAX_FILESIZE - (BLOCK_SIZE * 4) - 42);
}

int test_empty_file()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	lseek(fd, BLOCK_SIZE * 4 + 32, SEEK_SET);

	size_t len = 32;
	char rbuf[len];
	ASSERT_EQ(read(fd, rbuf, len), 0L);

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

	RUN_TEST(test_simple_file_write);
	RUN_TEST(test_write_read);
	RUN_TEST(test_simple_read_insert);
	RUN_TEST(test_rand_read);
	RUN_TEST(test_write_filesize_end);
	RUN_TEST(test_write_block_end);
	RUN_TEST(test_empty_file);

	return 0;
}
