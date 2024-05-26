#include "utils.h"
#include <stddef.h>
#include <stdio.h>

int test_read_cached()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);

	char wbuf[] = "Hello cruel world!\n";
	size_t len = strlen(wbuf) + 1;
	char rbuf[len];

	write(fd, wbuf, len);

	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, len);

	ASSERT_EQ_BUF(rbuf, wbuf, len);

	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, len);

	ASSERT_EQ_BUF(rbuf, wbuf, len);

	char wbuf2[] = "This is a different text\n";
	size_t len2 = strlen(wbuf2) + 1;

	lseek(fd, 0, SEEK_SET);
	write(fd, wbuf2, len2);

	// not updated because we did not change the write function
	// so it's not detected as dirty, and still think that it's up-to-date
	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, len);
	ASSERT_EQ_BUF(rbuf, wbuf, len);

	// after cache flush, this should be updated
	flush_cache();
	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, len);
	ASSERT_EQ_BUF(rbuf, wbuf2, len);

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

	RUN_TEST(test_read_cached);

	return 0;
}
