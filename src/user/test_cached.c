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

	// set_read_fn(DEFAULT_READ);
	pr_file(fd, 0, len);

	flush_cache();

	set_read_fn(CACHED_READ);

	lseek(fd, 0, SEEK_SET);
	read(fd, rbuf, len);

	pr_file(fd, 0, lseek(fd, 0, SEEK_END));
	pr_buf(rbuf, len);
	printf("\n");

	ASSERT_EQ_BUF(rbuf, wbuf, len);

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
