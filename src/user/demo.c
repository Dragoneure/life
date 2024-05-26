#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "../ioctl.h"
#include "utils.h"

int main()
{
	int ret = 0;
	int fd = 0;
	struct file_info info;

	fd = open("demo", O_RDWR | O_CREAT, 0644);

	char wbuf[] = "Ceci est le debut de la demo!\n";
	size_t len = strlen(wbuf);
	write(fd, wbuf, len);

	printf(ANSI_MAGENTA "Début de la démo !\n" ANSI_RESET);

	printf(ANSI_MAGENTA
	       "On commence par insérer un texte dans un fichier vide :\n" ANSI_RESET
	       "\n");
	if (ioctl(fd, OUICHEFS_IOC_FILE_BLOCK_PRINT) < 0) {
		printf("error in ioctl\n");
		return -1;
	}

	char wbuf2[] = "|Intrusion!\n|";
	size_t len2 = strlen(wbuf2);
	size_t pos2 = lseek(fd, 9, SEEK_SET);
	write(fd, wbuf2, len2);

	printf(ANSI_MAGENTA
	       "\nOn insère ensuite un texte au milieu de celui-ci : \n" ANSI_RESET
	       "\n");
	if (ioctl(fd, OUICHEFS_IOC_FILE_BLOCK_PRINT) < 0) {
		printf("error in ioctl\n");
		return -1;
	}

	char wbuf3[] = "Apres la fin du fichier!\n";
	size_t len3 = strlen(wbuf3);
	size_t end = lseek(fd, 0, SEEK_END);
	size_t pos3 = lseek(fd, end + 31, SEEK_SET);
	write(fd, wbuf3, len3);

	printf(ANSI_MAGENTA
	       "\nOn ajoute apres la fin du fichier : \n" ANSI_RESET "\n");
	if (ioctl(fd, OUICHEFS_IOC_FILE_BLOCK_PRINT) < 0) {
		printf("error in ioctl\n");
		return -1;
	}

	DEFRAG_FILE(fd);

	printf(ANSI_MAGENTA "\nOn run la defragmentation : \n" ANSI_RESET "\n");
	if (ioctl(fd, OUICHEFS_IOC_FILE_BLOCK_PRINT) < 0) {
		printf("error in ioctl\n");
		return -1;
	}

	close(fd);
	return ret;
}
