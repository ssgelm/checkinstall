/* AT_EMPTY_PATH asks about the descriptor itself. Rebuilding a pathname for
 * it changes the question: an O_PATH descriptor on a symbolic link becomes a
 * name that gets followed, and a dangling one then reports ENOENT rather
 * than the link. glibc below 2.32 sends tar's fchmodat replacement down
 * exactly this path, which is how it first showed up. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	struct stat st;
	int fd, rc;

	if (symlink("nowhere", "dangling") && errno != EEXIST) {
		printf("    could not make the link: %s\n", strerror(errno));
		return 2;
	}
	if ((fd = open("dangling", O_PATH|O_NOFOLLOW|O_CLOEXEC)) < 0) {
		printf("    O_PATH unsupported here: %s\n", strerror(errno));
		return 77;
	}

	errno = 0;
	rc = fstatat(fd, "", &st, AT_EMPTY_PATH);
	if (rc != 0) {
		printf("    fstatat(fd,\"\",AT_EMPTY_PATH) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}
	if (!S_ISLNK(st.st_mode)) {
		printf("    reported mode %07o, not a symbolic link\n", (unsigned)st.st_mode);
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}
