/* A call that succeeds must not leave errno holding someone else's error.
 * The interposition stats paths speculatively on its way to the real call,
 * and those misses used to reach the caller. Real code looks: gnulib's
 * lchmod reads errno after a successful lstat, and a stale ENOENT there
 * turns a working extraction into a fatal one. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

static int bad;

#define CHECK(what, expr) do {                                          \
	int r_;                                                         \
	errno = 0;                                                      \
	r_ = (expr);                                                    \
	if (r_ < 0) {                                                   \
		printf("    %-22s call failed (%s), not testable\n",    \
		       what, strerror(errno));                          \
	} else if (errno != 0) {                                        \
		printf("    %-22s succeeded but left errno=%d (%s)\n",  \
		       what, errno, strerror(errno));                   \
		bad++;                                                  \
	}                                                               \
} while (0)

int main(void) {
	struct stat st;
	int fd;

	/* a dangling link is the case that bites: the speculative stats
	 * behind the wrapper miss, and the miss used to escape */
	symlink("nowhere", "dangling");

	CHECK("lstat dangling",   lstat("dangling", &st));
	CHECK("open+create",      (fd = open("made", O_WRONLY|O_CREAT, 0644)));
	if (fd >= 0) close(fd);
	CHECK("stat existing",    stat("made", &st));
	CHECK("lstat existing",   lstat("made", &st));
	CHECK("chmod existing",   chmod("made", 0644));
	CHECK("access existing",  access("made", F_OK));
	CHECK("mkdir",            mkdir("adir", 0755));
	CHECK("symlink",          symlink("made", "alink"));
	CHECK("readlink",         (int)readlink("alink", (char[64]){0}, 63));
	CHECK("rename",           rename("made", "moved"));
	CHECK("unlink",           unlink("moved"));
	CHECK("rmdir",            rmdir("adir"));

	if (bad) printf("    %d call(s) left errno dirty\n", bad);
	return bad ? 1 : 0;
}
