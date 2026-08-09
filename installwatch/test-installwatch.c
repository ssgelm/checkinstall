/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 1998-99 Pancrazio `Ezio' de Mauro <p@demauro.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "localdecls.h"

#ifndef	LIBDIR 
	#define LIBDIR "/usr/local/lib"
#endif

#define TESTFILE "/tmp/installwatch-test"
#define TESTFILE2 TESTFILE "2"

int *refcount;
int *timecount;
int passed, failed;

/* A test function is void and do_test only compares a refcount, so until now
 * the only way to report a wrong value was to exit and take the rest of the
 * run with it. Record it here instead and let do_test fold it in. */
static int test_failures;

static void fail_test(const char *fmt, ...) {
	va_list ap;

	va_start(ap,fmt);
	vfprintf(stderr,fmt,ap);
	va_end(ap);
	test_failures++;
}
void* libc_handle=NULL;

void check_installwatch(void) {
	char *error;

	time(NULL);

	libc_handle=dlopen(LIBDIR"/installwatch.so",RTLD_LAZY);
	if(!libc_handle) {
		puts("Unable to open "LIBDIR"/installwatch.so");
		exit(255);
	}

	time(NULL);

	timecount=(int*)dlsym(libc_handle,"__installwatch_timecount");	
	if ((error = dlerror()) != NULL)  {
		fputs(error, stderr);
		exit(255);
	}

	if((*timecount)<2) {
		puts("This program must be run with installwatch");
		dlclose(libc_handle);
		exit(255);
	}

	refcount=(int*)dlsym(libc_handle,"__installwatch_refcount");	
	if ((error = dlerror()) != NULL)  {
		fputs(error, stderr);
		exit(255);
	}
}

void test_chmod(void) {
	int fd;

	fd = creat(TESTFILE, 0600);
	close(fd);
	chmod(TESTFILE, 0600);
	unlink(TESTFILE);
}

void test_chown(void) {
	int fd;

	fd = creat(TESTFILE, 0600);
	close(fd);
	chown(TESTFILE, geteuid(), getegid());
	unlink(TESTFILE);
}

void test_chroot(void) {
	char *error;
	char *longpath;
	void *libc;
	int (*libc_chroot)(const char *);
	int old_refcount;
	int libc_result,libc_errno;
	int wrapper_result,wrapper_errno;

	chroot("/");
	longpath=malloc(PATH_MAX*2+1);
	if(!longpath) exit(255);
	memset(longpath,'a',PATH_MAX*2);
	longpath[0]='/';
	longpath[PATH_MAX*2]='\0';

	libc=dlopen(LIBC_FILE,RTLD_LAZY);
	if(!libc) {
		fprintf(stderr,"Unable to open " LIBC_FILE ": %s\n",dlerror());
		exit(255);
	}
	dlerror();
	libc_chroot=(int (*)(const char *))dlsym(libc,"chroot");
	if((error=dlerror()) != NULL) {
		fprintf(stderr,"Unable to resolve chroot in " LIBC_FILE ": %s\n",
		        error);
		exit(255);
	}

	old_refcount=*refcount;
	errno=0;
	libc_result=libc_chroot(longpath);
	libc_errno=errno;
	if(*refcount != old_refcount) {
		fail_test("FAIL: direct libc chroot changed refcount from %d to %d\n",
		          old_refcount,*refcount);
	}

	errno=0;
	wrapper_result=chroot(longpath);
	wrapper_errno=errno;
	if(wrapper_result != libc_result || wrapper_errno != libc_errno) {
		fail_test("FAIL: libc chroot returned %d with errno %d; "
		          "installwatch returned %d with errno %d\n",
		          libc_result,libc_errno,wrapper_result,wrapper_errno);
	}

	dlclose(libc);
	free(longpath);
}

/* getcwd() returns the buffer it was given. Returning its own instead is a
 * dangling pointer into a frame that has gone, which reads back as the right
 * string where the stack grows down and as garbage where it grows up. */
void test_getcwd(void) {
	char buf[PATH_MAX];
	char *ret;

	ret=getcwd(buf,sizeof(buf));
	if(ret == NULL)
		fail_test("FAIL: getcwd(buf) failed\n");
	else if(ret != buf)
		fail_test("FAIL: getcwd returned %p, not the buffer %p it was given\n",
		          (void *)ret,(void *)buf);

	ret=getcwd(NULL,0);
	if(ret == NULL)
		fail_test("FAIL: getcwd(NULL,0) failed\n");
	else if(ret == buf)
		fail_test("FAIL: getcwd(NULL,0) returned the caller's buffer\n");
	else
		free(ret);
}

void test_creat(void) {
	int fd;

	fd = creat(TESTFILE, 0600);
	close(fd);
	unlink(TESTFILE);
}

void test_fchmod(void) {
	int fd;

	fd = creat(TESTFILE, 0600);
	fchmod(fd, 0600);
	close(fd);
	unlink(TESTFILE);
}

void test_fchown(void) {
	int fd;

	fd = creat(TESTFILE, 0600);
	fchown(fd, geteuid(), getegid());
	close(fd);
	unlink(TESTFILE);
}

void test_fopen(void) {
        FILE *fd;

        fd = fopen(TESTFILE,"w");
        fclose(fd);
        unlink(TESTFILE);
}

void test_ftruncate(void) {
	int fd;

	fd = creat(TESTFILE, 0600);
	ftruncate(fd, 0);
	close(fd);
	unlink(TESTFILE);
}

void test_lchown(void) {
	int fd;

	fd = creat(TESTFILE, 0600);
	close(fd);
	lchown(TESTFILE, geteuid(), getegid());
	unlink(TESTFILE);
}

void test_link(void) {
	int fd;

	fd = creat(TESTFILE, 0600);
	close(fd);
	link(TESTFILE, TESTFILE2);
	unlink(TESTFILE);
	unlink(TESTFILE2);
}

void test_mkdir(void) {
	mkdir(TESTFILE, 0700);
	rmdir(TESTFILE);
}

void test_open(void) {
	int fd;

	fd = open(TESTFILE, O_CREAT, O_RDWR, 0700);
	close(fd);
	unlink(TESTFILE);
}

void test_rename(void) {
	int fd;

	fd = creat(TESTFILE, 0700);
	close(fd);
	rename(TESTFILE, TESTFILE2);
	unlink(TESTFILE2);
}

void test_symlink(void) {
	int fd;

	fd = creat(TESTFILE, 0700);
	close(fd);
	symlink(TESTFILE, TESTFILE2);
	unlink(TESTFILE);
	unlink(TESTFILE2);
}

void test_truncate(void) {
	int fd;

	fd = creat(TESTFILE, 0700);
	close(fd);
	truncate(TESTFILE, 0);
	unlink(TESTFILE);
}

void test_unlink(void) {
	int fd;

	fd = creat(TESTFILE, 0700);
	close(fd);
	unlink(TESTFILE);
}

int do_test(const char *name, void (*function)(void), int increment) {
	int old_refcount;
	
	printf("Testing %s... ", name);
	test_failures = 0;
	old_refcount = *refcount;
	function();
	if(*refcount == old_refcount + increment && test_failures == 0) {
		printf("wanted refcount=%d returned refcount=%d",
			(old_refcount+increment),*refcount);
		puts("passed");
		passed++;
		return 0;
	} else {
		printf("wanted refcount=%d returned refcount=%d",
			(old_refcount+increment),*refcount);
	        puts("failed");
		failed++;
		return 1;
	}
}

int main(int argc, char **argv) {
	struct stat statbuf;

	check_installwatch();

	if(stat(TESTFILE, &statbuf) != -1) {
		printf(TESTFILE " already exists. Please remove it and run %s again\n", argv[0]);
		exit(254);
	}
	if(stat(TESTFILE2, &statbuf) != -1) {
		printf(TESTFILE2 " already exists. Please remove it and run %s again\n", argv[0]);
		exit(254);
	}
	puts("Testing installwatch " VERSION);
	puts("Using " TESTFILE " and " TESTFILE2 " as a test files\n");
	passed = failed = 0;
	do_test("chmod", test_chmod, 3);
	do_test("chown", test_chown, 3);
	do_test("chroot", test_chroot, 2);
	do_test("getcwd", test_getcwd, 0);
	do_test("creat", test_creat, 2);
	do_test("fchmod", test_fchmod, 3);
	do_test("fchown", test_fchown, 3);
	do_test("fopen",test_fopen,2);
	do_test("ftruncate", test_ftruncate, 3);
	do_test("lchown", test_lchown, 3);
	do_test("link", test_link, 4);
	do_test("mkdir", test_mkdir, 2);
	/* do_test("mknod", test_mknod, 2); */
	do_test("open", test_open, 2);
	do_test("rename", test_rename, 3);
	do_test("rmdir", test_mkdir, 2);
	do_test("symlink", test_symlink, 4);
	do_test("truncate", test_truncate, 3);
	do_test("unlink", test_unlink, 2);

	putchar('\n');
	if(failed != 0) {
		printf("%d tests were not successful!\n", failed);
		printf("Please email this log to the maintainer with the output of\n");
		printf("\tnm %s\n", argv[0]);
	} else
		printf("All tests successful!\n");

	if(libc_handle!=NULL)
		dlclose(libc_handle);

	return failed;
}
