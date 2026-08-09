/* getcwd() returns the buffer it was given. Returning anything else is a
 * dangling pointer into the wrapper's own frame, which reads as the right
 * string on an architecture whose stack grows down and as garbage on one
 * whose stack grows up. Check the pointer, not the string. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

int main(void) {
	char buf[PATH_MAX];
	char *ret;

	ret = getcwd(buf, sizeof buf);
	if (ret == NULL) {
		puts("getcwd failed");
		return 1;
	}
	if (ret != buf) {
		puts("getcwd returned a pointer other than the buffer given");
		return 1;
	}

	/* and the allocating form still has to hand back its own storage */
	ret = getcwd(NULL, 0);
	if (ret == NULL) {
		puts("getcwd(NULL,0) failed");
		return 1;
	}
	if (ret == buf || strlen(ret) == 0) {
		puts("getcwd(NULL,0) did not allocate");
		return 1;
	}

	puts("ok");
	return 0;
}
