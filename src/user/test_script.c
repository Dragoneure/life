#include "utils.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define buff_len 128

void test_write_print()
{
	int fd = open(__func__, O_RDWR | O_CREAT, 0644);
	char buff1[] = "Bonjour ";
	char buff2[] = "je  ";
	char buff3[] = "suis ";
	char buff4[] = "la ";
	char buff5[] = "mort ";
	char buff6[] = "c'est la fin ";
	char buff7[] = " coucou ";
	char buff8[] = " so far away";

	set_write_fn(LIGHT_WRITE);
	set_read_fn(LIGHT_READ);

	write(fd, buff1, strlen(buff1));
	lseek(fd, 0, SEEK_SET);
	write(fd, buff2, strlen(buff2));
	lseek(fd, 0, SEEK_SET);
	write(fd, buff3, strlen(buff3));
	lseek(fd, 0, SEEK_SET);
	write(fd, buff4, strlen(buff4));
	lseek(fd, 0, SEEK_SET);
	write(fd, buff5, strlen(buff5));

	lseek(fd, 0, SEEK_END);
	write(fd, buff6, strlen(buff6));

	lseek(fd, 20, SEEK_SET);
	write(fd, buff7, strlen(buff7));

	printf("Max filesize : %d\n", MAX_FILESIZE);
	lseek(fd, MAX_FILESIZE - 100, SEEK_SET);
	// lseek(fd, 100000, SEEK_SET);
	write(fd, buff8, strlen(buff8));

	// int file_size = lseek(fd, 0, SEEK_END);
	// printf("file size : %d\n", file_size);
	// lseek(fd, 0, SEEK_SET);
	// pr_file(fd, 0, file_size);
	SHOW_FILE_INFO(fd);
}
// int init_file()
// {
// 	int fd_write_kernel =
// 		open("file_default_write.txt", O_RDWR | O_CREAT, 0644);

// 	char wbuf[buff_len];
// 	int start = 0;
// 	init_seq_buff(wbuf, buff_len, &start);
// 	set_write_fn(DEFAULT_WRITE);

// 	write(fd_write_kernel, wbuf, buff_len);

// 	pr_buf(wbuf, buff_len);

// 	int fd_write_light =
// 		open("file_light_write.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
// 	set_write_fn(SIMPLE_WRITE);
// 	write(fd_write_light, wbuf, buff_len);

// 	char rbuf_dft[buff_len];
// 	char rbuf_simple[buff_len];

// 	printf("\n\ndefault write\n");
// 	set_read_fn(DEFAULT_READ);

// 	lseek(fd_write_kernel, 0, SEEK_SET);
// 	read(fd_write_kernel, rbuf_dft, buff_len + 1);
// 	pr_file(fd_write_kernel, 0, buff_len + 1);

// 	printf("\n\n light write\n");
// 	set_read_fn(SIMPLE_READ);
// 	pr_file(fd_write_light, 0, buff_len + 1);
// 	lseek(fd_write_light, 0, SEEK_SET);
// 	read(fd_write_light, rbuf_simple, buff_len + 1);

// 	ASSERT_EQ_BUF(rbuf_dft, rbuf_simple, buff_len + 1);

// 	return TEST_SUCCESS;
// }

int main()
{
	test_write_print();
	return 0;
}
