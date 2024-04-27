#include <string.h>
#include "utils.h"

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
