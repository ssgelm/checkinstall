/* access(), faccessat() and euidaccess() all answer questions about a path
 * and all have to be asked about the translated one. rm checks write access
 * before unlinking, and an unwrapped check reports the file missing. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
	const char *path;
	int dirfd, rc, bad = 0;
	char dir[4096], *slash;

	if (argc < 2)
		return 2;
	path = argv[1];

	rc = access(path, F_OK);
	if (rc != 0) { puts("access F_OK failed"); bad = 1; }

	rc = euidaccess(path, F_OK);
	if (rc != 0) { puts("euidaccess F_OK failed"); bad = 1; }

	rc = faccessat(AT_FDCWD, path, F_OK, 0);
	if (rc != 0) { puts("faccessat AT_FDCWD F_OK failed"); bad = 1; }

	rc = faccessat(AT_FDCWD, path, W_OK, AT_EACCESS);
	if (rc != 0) { puts("faccessat AT_EACCESS W_OK failed"); bad = 1; }

	/* and relative to a real directory descriptor */
	snprintf(dir, sizeof dir, "%s", path);
	slash = strrchr(dir, '/');
	if (slash != NULL && slash != dir) {
		*slash = '\0';
		dirfd = open(dir, O_RDONLY | O_DIRECTORY);
		if (dirfd < 0) {
			puts("could not open the parent directory");
			bad = 1;
		} else {
			rc = faccessat(dirfd, slash + 1, F_OK, 0);
			if (rc != 0) { puts("faccessat dirfd F_OK failed"); bad = 1; }
			close(dirfd);
		}
	}

	if (!bad)
		puts("ok");
	return bad;
}
