#include "utils.h"
#include <fcntl.h>
int test_write_read()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	size_t pos = lseek(fd, BLOCK_SIZE - 5, SEEK_SET);
	char wbuf[] = "Hello cruel world!";
	size_t len = strlen(wbuf);
	write(fd, wbuf, len);
	char rbuf[len];
	lseek(fd, pos, SEEK_SET);
	read(fd, rbuf, len);
	ASSERT_EQ_BUF(rbuf, wbuf, len);

	return TEST_SUCCESS;
}

int test_rand_read(int read1, int read2)
{
	int read_fn[] = { read1, read2 };
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	/*add by Harena*/
	int fd2 = open(__func__, O_RDWR);
	init_rand_file(fd);

	size_t len = 256;
	int round = 100;
	char expect[2][len];
	size_t readen[2];

	for (int i = 0; i < round; i++) {
		size_t rand_pos = rand() % MAX_FILESIZE;
		// TODO: set first and second read fn
		lseek(fd, rand_pos, SEEK_SET);
		lseek(fd2, rand_pos, SEEK_SET);
		readen[0] = read(fd, expect[0], len);
		readen[1] = read(fd2, expect[1], len);
		ASSERT_EQ_BUF(expect[0], expect[1], len);
		ASSERT_EQ(readen[0], readen[1]);
	}

	return TEST_SUCCESS;
}

int test_insert()
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

	for (int i = 0; i < 3; i++) {
		pr_test("rbuf content: ");
		pr_buf(rbuf[i], len);
		printf("\n");
	}

	return TEST_SUCCESS;
}

int test_write_end()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	char wbuf[] =
		"Riding on a pancake spaceship, the syrupy crew soared through galaxies.";
	size_t len = strlen(wbuf);

	lseek(fd, MAX_FILESIZE - len, SEEK_SET);
	ASSERT_EQ(write(fd, wbuf, len), len);

	char rbuf[len];
	size_t end_offset = 16;

	lseek(fd, MAX_FILESIZE - end_offset, SEEK_SET);
	ASSERT_EQ(read(fd, rbuf, len), end_offset);
	ASSERT_EQ_BUF(rbuf, &wbuf[len - end_offset], end_offset)

	return TEST_SUCCESS;
}

int main(int argc, char **argv)
{
	srand(42);
	RUN_TEST(test_write_read);
	//RUN_TEST(test_insert);
	RUN_TEST(test_rand_read, 1, 2);
	//RUN_TEST(test_write_end);
	return 0;
}
