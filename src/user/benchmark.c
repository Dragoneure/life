#include <stdio.h>
#include <fcntl.h>
#include <string.h>

void test_write()
{
}
int main(int argc, char **argv)
{
	int size1 = 0;
	int size2 = 1024;
	int size3 = 1024 * 1024 * 4;
	FILE *fd1 = fopen("file1.txt", "wb+");
	FILE *fd2 = fopen("file2.txt", "wb+");
	FILE *fd3 = fopen("file3.txt", "wb+");
	fseek(fd1, 50, SEEK_SET);
	char buff1[] = "Hello cruel world";
	fwrite(fd1, sizeof(char), strlen(buff1), fd1);

	return 0;
}
