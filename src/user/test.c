#include <string.h>
#include "utils.h"

int test_write_read()
{
	FILE *f = fopen(__func__, "w+");
	fseek(f, BLOCK_SIZE - 5, SEEK_SET);

	size_t pos = ftell(f);
	char wbuf[] = "Hello cruel world!";
	size_t len = strlen(wbuf);
	fwrite(wbuf, sizeof(char), len, f);

	char rbuf[len];
	fseek(f, pos, SEEK_SET);
	fread(rbuf, sizeof(char), len, f);
	ASSERT_EQ_BUF(rbuf, wbuf, len);

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
	size_t readen[2];

	for (int i = 0; i < round; i++) {
		size_t rand_pos = rand() % MAX_FILESIZE;
		// TODO: set first and second read fn
		fseek(f, rand_pos, SEEK_SET);
		readen[0] = fread(expect[0], sizeof(char), len, f);
		fseek(f, rand_pos, SEEK_SET);
		readen[1] = fread(expect[1], sizeof(char), len, f);
		ASSERT_EQ_BUF(expect[0], expect[1], len);
		ASSERT_EQ(readen[0], readen[1]);
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
	size_t pos = BLOCK_SIZE / 3;

	fseek(f, pos - len, SEEK_SET);
	for (int i = 0; i < 3; i++)
		fread(prev_rbuf[i], sizeof(char), len, f);

	fseek(f, pos, SEEK_SET);
	fwrite(wbuf, sizeof(char), len, f);

	fseek(f, pos - len, SEEK_SET);
	for (int i = 0; i < 3; i++)
		fread(rbuf[i], sizeof(char), len, f);

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
	FILE *f = fopen(__func__, "w+");

	char wbuf[] =
		"Riding on a pancake spaceship, the syrupy crew soared through galaxies.";
	size_t len = strlen(wbuf);

	fseek(f, MAX_FILESIZE - len, SEEK_SET);
	ASSERT_EQ(fwrite(wbuf, sizeof(char), len, f), len);

	char rbuf[len];
	size_t end_offset = 16;

	fseek(f, MAX_FILESIZE - end_offset, SEEK_SET);
	ASSERT_EQ(fread(rbuf, sizeof(char), len, f), end_offset);
	ASSERT_EQ_BUF(rbuf, &wbuf[len - end_offset], end_offset)

	return TEST_SUCCESS;
}

int main(int argc, char **argv)
{
	srand(42);
	RUN_TEST(test_write_read);
	RUN_TEST(test_insert);
	RUN_TEST(test_rand_read, 1, 2);
	RUN_TEST(test_write_end);
	return 0;
}
