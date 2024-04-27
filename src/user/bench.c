#include "utils.h"
#include <string.h>

void bench_write_read()
{
	FILE *f = fopen(__func__, "w+");
	time_t w_time = 0;
	time_t r_time = 0;

	size_t len = 256;
	char wbuf[len];
	memset(wbuf, 1, len);

	for (int i = 0; i < MAX_FILESIZE; i += BLOCK_SIZE) {
		for (int j = 0; j < BLOCK_SIZE; j += BLOCK_SIZE / 10) {
			struct time_data t;
			TIME_START(t);
			fwrite(wbuf, sizeof(char), len, f);
			TIME_END(t);
			w_time += t.diff;
		}
	}

	fseek(f, 0, SEEK_SET);
	char rbuf[len];

	for (int i = 0; i < MAX_FILESIZE; i += len) {
		struct time_data t;
		TIME_START(t);
		fread(rbuf, sizeof(char), len, f);
		TIME_END(t);
		r_time += t.diff;
	}

	pr_test("Total write time: %ld ms\n", w_time / 1000);
	pr_test("Total read time: %ld ms\n", r_time / 1000);
}

int main(int argc, char **argv)
{
	RUN_BENCH(bench_write_read);
	return 0;
}
