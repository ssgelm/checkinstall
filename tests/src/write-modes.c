/* Opens that modify a file without asking for write access, and an fopen
 * mode whose '+' is not the second character. All of these were treated as
 * read-only and went to the real filesystem. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if(argc<3) return 2;

	if(!strcmp(argv[1],"wronly-creat")) {
		/* the ordinary case, for tests that are about something else */
		int fd=open(argv[2],O_WRONLY|O_CREAT|O_TRUNC,0644);
		if(fd<0) return 1;
		close(fd);
	} else if(!strcmp(argv[1],"rdonly-creat")) {
		int fd=open(argv[2],O_RDONLY|O_CREAT,0644);
		if(fd<0) return 1;
		close(fd);
	} else if(!strcmp(argv[1],"rdonly-trunc")) {
		int fd=open(argv[2],O_RDONLY|O_TRUNC);
		if(fd<0) return 1;
		close(fd);
	} else if(!strcmp(argv[1],"fopen-rbplus")) {
		FILE *f=fopen(argv[2],"rb+");
		if(!f) return 1;
		fputs("clobbered\n",f);
		fclose(f);
	} else if(!strcmp(argv[1],"plain-read")) {
		int fd=open(argv[2],O_RDONLY);
		if(fd<0) return 1;
		close(fd);
	} else {
		return 2;
	}
	return 0;
}
