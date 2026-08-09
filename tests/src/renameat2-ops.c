/* renameat2 flags must reach the kernel. RENAME_NOREPLACE has to fail
 * rather than overwrite, and RENAME_EXCHANGE has to swap rather than
 * degrade into a one way rename that destroys one side. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1<<0)
#endif
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1<<1)
#endif

int main(int argc, char **argv) {
	unsigned int flags;
	int r;
	if(argc<4) return 2;

	if(!strcmp(argv[1],"noreplace"))     flags=RENAME_NOREPLACE;
	else if(!strcmp(argv[1],"exchange")) flags=RENAME_EXCHANGE;
	else                                 flags=0;

	errno=0;
	r=renameat2(AT_FDCWD,argv[2],AT_FDCWD,argv[3],flags);
	printf("rc=%d errno=%d\n", r, r<0?errno:0);
	return r<0 ? 1 : 0;
}
