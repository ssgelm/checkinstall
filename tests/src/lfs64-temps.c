/* Call the LFS64 spellings of the temporary file creators, which is what a
 * _FILE_OFFSET_BITS=64 build gets when it writes mkstemps()/mkostemps(). */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

extern int mkstemps64(char *, int);
extern int mkostemps64(char *, int, int);

int main(int argc, char **argv) {
	char a[512], b[512];
	int fd, rc = 0;

	snprintf(a, sizeof a, "%s/tmpXXXXXX.log", argv[1]);
	snprintf(b, sizeof b, "%s/tmoXXXXXX.log", argv[1]);

	if ((fd = mkstemps64(a, 4)) < 0) { perror("  mkstemps64"); rc = 1; }
	else { write(fd, "x", 1); close(fd); printf("  mkstemps64  -> %s\n", a); }

	if ((fd = mkostemps64(b, 4, 0)) < 0) { perror("  mkostemps64"); rc = 1; }
	else { write(fd, "x", 1); close(fd); printf("  mkostemps64 -> %s\n", b); }

	return rc;
}
