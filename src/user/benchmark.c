#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#define INSERT 200
#define SIZE_STRING 40
#define SIZE_INSERT 20

void test_write()
{
	clock_t start, end;
	double time_used;
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
	//FILE *fd1 = fopen("/root/test/file.txt", "r+");
	FILE *fd1 = fopen(
		"/home/harena/snail/M1/S2/LIFE_project/life/share/file1.txt",
		"r+b");
	if (fd1 == NULL) {
		printf("cannot open the file\n");
	}
	fseek(fd1, INSERT - SIZE_STRING, SEEK_SET);
	fread(prev, sizeof(char), SIZE_STRING, fd1);
	fread(next, sizeof(char), SIZE_STRING, fd1);
	prev[SIZE_STRING] = '\0';
	next[SIZE_STRING] = '\0';
	/*
	 * move the file pointer where we would like d-to insert our string 
	 */
	fseek(fd1, INSERT, SEEK_SET);
	char buff1[] = "Hello cruel world!!!";
	start = clock();
	fwrite(buff1, sizeof(char), strlen(buff1), fd1);
	end = clock();

	fseek(fd1, INSERT - SIZE_STRING, SEEK_SET);
	fread(prev_after, sizeof(char), SIZE_STRING, fd1);
	fread(actual, sizeof(char), SIZE_INSERT, fd1);
	fread(next_after, sizeof(char), SIZE_STRING, fd1);

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

	time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
	printf("Writing at 50 octetc took : %f\n", time_used);
err:
	fclose(fd1);
}
int main(int argc, char **argv)
{
	test_write();
	return 0;
}
