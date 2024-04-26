#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#define INSERT 200
#define SIZE_STRING 40
#define SIZE_INSERT 20
#define SIZE_READ 50
#define SIZE_WRITE 50

/*
 * Test if the function read of our filesystem works 
 * file1 should be a file located on working filesystem
 * file2 should be a file located on our filesystem
 */

void test_read(int round, char *file1, char *file2)
{
	FILE *f1 = fopen(file1, "r+");
	FILE *f2 = fopen(file2, "r+");
	fseek(f1, 0, SEEK_END);
	fseek(f2, 0, SEEK_END);
	int size_file1 = ftell(f1);
	int size_file2 = ftell(f2);
	int read1 = 0;
	int read2 = 0;
	if (size_file1 != size_file2) {
		printf("file : %s and %s are not the same\n", file1, file2);
		printf("size 1 : %d and size2 : %d\n", size_file1, size_file2);
		goto close;
		return;
	}

	char buff1[SIZE_READ + 1];
	char buff2[SIZE_READ + 1];
	int rand_seek = 0;
	for (int i = 0; i < round; i++) {
		fseek(f1, rand_seek, SEEK_SET);
		fseek(f2, rand_seek, SEEK_SET);
		read1 = fread(buff1, sizeof(char), SIZE_READ, f1);
		read2 = fread(buff2, sizeof(char), SIZE_READ, f2);
		buff1[SIZE_READ] = '\0';
		buff2[SIZE_READ] = '\0';
		rand_seek = rand() % (size_file1 - SIZE_READ);
		if (read1 != read2) {
			printf("filesystem [read] : number of read\n");
			printf("read1 : %d\nread2 : %d\n", read1, read2);
			goto close;
		}
		if (strncmp(buff1, buff2, SIZE_READ)) {
			printf("filesystem [read] : not the good data\n");
			printf("buff1 : %s\nbuff2 : %s", buff1, buff2);
			goto close;
		}
	}
	printf("filesystem [read] : ok\n");
close:
	fclose(f1);
	fclose(f2);
}

void test_insertion(char *file)
{
	clock_t start, end, start2, end2;
	double time_used_write, time_used_read;
	int size1 = 0;
	int write;
	/*
	 * prev buffer : previous string  where we inserted our string 
	 * next buffer : next string  where we inserted our string
	 * prev_after  buffer : previous string  where we inserted our string after we inserted the string
	 * next_after  buffer : next string  where we inserted our string after we inserted the string
	 * actual buffer : te string we insterted 
	 */

	char prev[SIZE_STRING + 1];
	char prev_after[SIZE_STRING + 1];

	char next[SIZE_STRING + 1];
	char next_after[SIZE_STRING + 1];
	char actual[SIZE_INSERT + 1];
	/* we should create an another option to insert into a file */
	FILE *fd1 = fopen(file, "r+");
	if (fd1 == NULL) {
		printf("cannot open the file\n");
	}
	fseek(fd1, INSERT - SIZE_STRING, SEEK_SET);
	fread(prev, sizeof(char), SIZE_STRING, fd1);
	fread(next, sizeof(char), SIZE_STRING, fd1);
	prev[SIZE_STRING] = '\0';
	next[SIZE_STRING] = '\0';
	/*
	 * move the file pointer where we would like to insert our string 
	 */
	fseek(fd1, INSERT, SEEK_SET);
	char buff1[] = "Hello cruel world!!!";
	start = clock();
	fwrite(buff1, sizeof(char), strlen(buff1), fd1);
	end = clock();

	fseek(fd1, INSERT - SIZE_STRING, SEEK_SET);

	start2 = clock();
	fread(prev_after, sizeof(char), SIZE_STRING, fd1);
	fread(actual, sizeof(char), SIZE_INSERT, fd1);
	fread(next_after, sizeof(char), SIZE_STRING, fd1);
	end2 = clock();
	prev_after[SIZE_STRING] = '\0';
	next_after[SIZE_STRING] = '\0';
	actual[SIZE_INSERT] = '\0';
	if (strncmp(prev, prev_after, SIZE_STRING)) {
		printf("problem with previous string : \nbefore : %s\nafter  : %s\n",
		       prev, prev_after);
		goto err;
	}
	if (strncmp(next, next_after, SIZE_STRING)) {
		printf("problem with next string : \nbefore : %s\nafter  : %s\n",
		       next, next_after);
		goto err;
	}
	if (strncmp(buff1, actual, SIZE_INSERT)) {
		printf("problem with inserted  string : \nbefore : %s\nafter  : %s\n",
		       buff1, actual);
		goto err;
	}

	printf("previous string : \nbefore : %s\nafter  : %s\n", prev,
	       prev_after);
	printf("next string : \nbefore : %s\nafter  : %s\n", next, next_after);
	printf("inserted  string : \nbefore : %s\nafter  : %s\n", buff1,
	       actual);

	time_used_write = ((double)(end - start)) / CLOCKS_PER_SEC;
	time_used_read = ((double)(end2 - start2)) / CLOCKS_PER_SEC;
	printf("Writing at %d octets took : %f\n", INSERT, time_used_write);
	printf("Reading : %d after instertion took : %f\n",
	       SIZE_STRING * 2 + SIZE_INSERT, time_used_read);
err:
	fclose(fd1);
}

/*
 * Number of argument of main should always be : 3
 * 1 : executable
 * 2 : file1 or the file of insertion if test_insertion
 * 3 : file2 or 0 if test_insertion
 */
int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("Bad arguments\n");
		return 0;
	}
	srand(time(NULL));
	//test_insertion(argv[1]);
	test_read(10, argv[1], argv[2]);
	return 0;
}
