/* Materialise an O_TMPFILE file with linkat(fd, "", ..., AT_EMPTY_PATH).
 * The file has no name of its own, so a wrapper that rebuilds the source
 * path from /proc/self/fd gets "/dir/#1234 (deleted)" and loses the data. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv) {
	int fd, r;

	fd = open(argv[1], O_TMPFILE | O_RDWR, 0600);
	if (fd < 0) {
		printf("  O_TMPFILE unsupported here: %s\n", strerror(errno));
		return 77;
	}
	if (write(fd, "payload\n", 8) != 8) { perror("  write"); return 1; }

	errno = 0;
	r = linkat(fd, "", AT_FDCWD, argv[2], AT_EMPTY_PATH);
	if (r < 0) {
		printf("  linkat(AT_EMPTY_PATH): %s\n", strerror(errno));
		return 1;
	}
	return 0;
}
