/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 1998-9 Pancrazio `Ezio' de Mauro <p@demauro.net>
 * Copyright (C) 2026 Stephen Gelman <ssgelm@gmail.com>
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
 *
 * 
 * april-15-2001 - Modifications by Felipe Eduardo Sanchez Diaz Duran
 *                                  <izto@asic-linux.com.mx>
 * Added backup() and make_path() functions.
 *
 * november-25-2002 - Modifications by Olivier Fleurigeon
 *                                  <olivier.fleurigeon@cegedim.fr>
 *
 * march-31-2007 - Modifications by Frederick Emmott
 *                                  <mail@fredemmott.co.uk>
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <inttypes.h>
#undef basename
#include <string.h>
#include <time.h>
#include <utime.h>
#include <dlfcn.h>
#include <dirent.h>

/* There's no d_off on GNU/kFreeBSD */
#if defined(__FreeBSD_kernel__)
#define D_OFF(X) (-1)
#else
#define D_OFF(X) (X)
#endif
	
#include "localdecls.h"

#define DEBUG 1  

#define LOGLEVEL (LOG_USER | LOG_INFO | LOG_PID)
#define BUFSIZE 1024

#define error(X) (X < 0 ? strerror(errno) : "success")

int __installwatch_refcount = 0;
int __installwatch_timecount = 0;

#define REFCOUNT __installwatch_refcount++
#define TIMECOUNT __installwatch_timecount++

static time_t (*true_time) (time_t *);
static int (*true_chdir)(const char *);
static int (*true_chmod)(const char *, mode_t);
static int (*true_chown)(const char *, uid_t, gid_t);
static int (*true_chroot)(const char *);
static int (*true_creat)(const char *, mode_t);
static int (*true_fchmod)(int, mode_t);
static int (*true_fchown)(int, uid_t, gid_t);
static FILE *(*true_fopen)(const char *,const char*);
static int (*true_ftruncate)(int, TRUNCATE_T);
static char *(*true_getcwd)(char*,size_t);
static int (*true_lchown)(const char *, uid_t, gid_t);
static int (*true_link)(const char *, const char *);
static int (*true_mkdir)(const char *, mode_t);
static int (*true_xmknod)(int ver,const char *, mode_t, dev_t *);
static int (*true_open)(const char *, int, ...);
static DIR *(*true_opendir)(const char *);
static struct dirent *(*true_readdir)(DIR *dir);
static ssize_t (*true_readlink)(const char*,char *,size_t);
static char *(*true_realpath)(const char *,char *);
static int (*true_rename)(const char *, const char *);
static int (*true_rmdir)(const char *);
static int (*true_xstat)(int,const char *,struct stat *);
static int (*true_lxstat)(int,const char *,struct stat *);

#if(GLIBC_MINOR >= 10)

static int (*true_scandir)(	const char *,struct dirent ***,
				int (*)(const struct dirent *),
				int (*)(const struct dirent **,const struct dirent **));

#else

static int (*true_scandir)(	const char *,struct dirent ***,
				int (*)(const struct dirent *),
				int (*)(const void *,const void *));
#endif

static int (*true_symlink)(const char *, const char *);
static int (*true_truncate)(const char *, TRUNCATE_T);
static int (*true_unlink)(const char *);
static int (*true_utime)(const char *,const struct utimbuf *);
static int (*true_utimes)(const char *,const struct timeval *);
static int (*true_utimensat)(int, const char *, const struct timespec *, int);

  /*
   *   Where time_t is 32 bits, a program built with _TIME_BITS=64 is given
   * these entry points in place of the ordinary ones, and coreutils among
   * others is built that way. We never look inside the buffer a caller hands
   * us, so it stays a void * here and only the path needs translating.
   *   Build installwatch itself that way, as armhf does by default, and
   * glibc renames the wrappers below to these same symbols. Defining them
   * again is then a duplicate, so leave that to the renaming.
   */
#if defined(__TIMESIZE) && __TIMESIZE == 32 && \
    !defined(__USE_TIME_BITS64) && !defined(__USE_TIME64_REDIRECTS)
#define WRAP_TIME64_ENTRY_POINTS
#endif

#ifdef WRAP_TIME64_ENTRY_POINTS
static int (*true_stat64_time64)(const char *, void *);
static int (*true_lstat64_time64)(const char *, void *);
static int (*true_fstatat64_time64)(int, const char *, void *, int);
static int (*true_utime64)(const char *, const void *);
static int (*true_utimes64)(const char *, const void *);
static int (*true_utimensat64)(int, const char *, const void *, int);
#endif

static int (*true_access)(const char *, int);
static int (*true_setxattr)(const char *,const char *,const void *,
                            size_t, int);
static int (*true_removexattr)(const char *,const char *);

static int (*true_creat64)(const char *, __mode_t);
static FILE *(*true_fopen64)(const char *,const char *);
static int (*true_ftruncate64)(int, __off64_t);
static int (*true_open64)(const char *, int, ...);
static struct dirent64 *(*true_readdir64)(DIR *dir);

#if(GLIBC_MINOR >= 10)
static int (*true_scandir64)(	const char *,struct dirent64 ***,
				int (*)(const struct dirent64 *),
				int (*)(const struct dirent64 **,const struct dirent64 **));
#else
static int (*true_scandir64)(	const char *,struct dirent64 ***,
				int (*)(const struct dirent64 *),
				int (*)(const void *,const void *));
#endif
static int (*true_xstat64)(int,const char *, struct stat64 *);
static int (*true_lxstat64)(int,const char *, struct stat64 *);
static int (*true_truncate64)(const char *, __off64_t);


static int (*true_openat)(int, const char *, int, ...);
static int (*true_fchmodat)(int, const char *, mode_t, int);
static int (*true_fchownat)(int, const char *, uid_t, gid_t, int);
static int (*true_fxstatat)(int, int, const char *, struct stat *, int);
static int (*true_fxstatat64)(int, int, const char *, struct stat64 *, int);
static int (*true_linkat)(int, const char *, int, const char *, int);
static int (*true_mkdirat)(int, const char *, mode_t);
static int (*true_readlinkat)(int, const char *, char *, size_t);
static int (*true_xmknodat)(int, int, const char *, mode_t, dev_t *);
static int (*true_renameat)(int, const char *, int, const char *);
static int (*true_renameat2)(int, const char *, int, const char *);
static int (*true_symlinkat)(const char *, int, const char *);
static int (*true_unlinkat)(int, const char *, int);

  /*
   *   statx() arrived in glibc 2.28 and has no older form to stand in for
   * it. coreutils calls it directly, so it needs a wrapper of its own.
   * Without one, a translated file is invisible to ls(1), stat(1) and
   * anything else that asks this way.
   */
#if (GLIBC_MINOR >= 28)
static int (*true_statx)(int, const char *, int, unsigned int,
                         struct statx *);
#endif

  /* These functions open their files internally, bypassing open(). */
static int (*true_mkstemp)(char *);
static int (*true_mkstemp64)(char *);
static int (*true_mkostemp)(char *, int);
static int (*true_mkostemp64)(char *, int);
static int (*true_mkstemps)(char *, int);
static int (*true_mkstemps64)(char *, int);
static int (*true_mkostemps)(char *, int, int);
static int (*true_mkostemps64)(char *, int, int);
static char *(*true_mkdtemp)(char *);

  /* glibc 2.33 exports these names directly. Keep the __xstat wrappers */
  /* below for binaries built against older glibc.                     */
#if (GLIBC_MINOR >= 33)
static int (*true_stat)(const char *, struct stat *);
static int (*true_stat64)(const char *, struct stat64 *);
static int (*true_mknod)(const char *, mode_t, dev_t);
static int (*true_lstat)(const char *, struct stat *);
static int (*true_lstat64)(const char *, struct stat64 *);
static int (*true_fstatat)(int, const char *, struct stat *, int);
static int (*true_fstatat64)(int, const char *, struct stat64 *, int);
static int (*true_mknodat)(int, const char *, mode_t, dev_t);
#endif

#if defined __GNUC__ && __GNUC__>=2
	#define inline inline
#else
	#define inline
#endif	

#ifndef _STAT_VER
 #if defined (__aarch64__) || defined(__riscv)
  #define _STAT_VER 0
 #elif defined (__s390x__)
  # define _STAT_VER 1
 #elif defined (__x86_64__)
  #define _STAT_VER 1
 #else
  #define _STAT_VER 3
 #endif
#endif
#ifndef _MKNOD_VER
 #if defined (__aarch64__) || defined(__riscv)
  #define _MKNOD_VER 0
 #elif defined (__x86_64__)
  #define _MKNOD_VER 0
 #else
  #define _MKNOD_VER 1
 #endif
#endif


#if (GLIBC_MINOR < 33)
static inline int true_stat(const char *pathname,struct stat *info) {
	return true_xstat(_STAT_VER,pathname,info);
}

static inline int true_mknod(const char *pathname,mode_t mode,dev_t dev) {
	return true_xmknod(_MKNOD_VER,pathname,mode,&dev);
}

static inline int true_lstat(const char *pathname,struct stat *info) {
	return true_lxstat(_STAT_VER,pathname,info);
}

static inline int true_fstatat(int dirfd, const char *pathname, struct stat *info, int flags) {
	return true_fxstatat(_STAT_VER, dirfd, pathname, info, flags);
}

static inline int true_fstatat64(int dirfd, const char *pathname, struct stat64 *info, int flags) {
	return true_fxstatat64(_STAT_VER, dirfd, pathname, info, flags);
}

static inline int true_mknodat(int dirfd, const char *pathname,mode_t mode,dev_t dev) {
	return true_xmknodat(_MKNOD_VER, dirfd, pathname, mode, &dev);
}
#endif


  /* A few defines to fix things a little */
#define INSTW_OK 0 
  /* If not set, no work with translation is allowed */
#define INSTW_INITIALIZED	(1<<0)
  /* If not set, a wrapped function only do its "real" job */
#define INSTW_OKWRAP		(1<<1)
#define INSTW_OKBACKUP		(1<<2)
#define INSTW_OKTRANSL		(1<<3)

#define INSTW_TRANSLATED	(1<<0)
  /* Indicates that a translated file is identical to original */
#define INSTW_IDENTITY  	(1<<1)

  /* The file currently exists in the root filesystem */
#define INSTW_ISINROOT		(1<<6)
  /* The file currently exists in the translated filesystem */
#define INSTW_ISINTRANSL	(1<<7)

#define _BACKUP "/BACKUP"
#define _TRANSL "/TRANSL"

  /* The root that contains all the needed metas-infos */
#define _META   "/META"
  /* We store under this subtree the translated status */
#define _MTRANSL _TRANSL
  /* We construct under this subtree fake directory listings */
#define _MDIRLS  "/DIRLS"  

  /* String cell used to chain excluded paths */
typedef struct string_t string_t;
struct string_t {
	char *string;
	string_t *next;
};	

  /* Used to keep all infos needed to cope with backup, translated fs... */
typedef struct instw_t {
	  /*
	   * global fields 
	   */
	int gstatus;
	int dbglvl;
	pid_t pid;
	char *root;
	char *backup;
	char *transl;
	char *meta;
	char *mtransl;
	char *mdirls;
	  /* the list of all the paths excluded from translation */
	string_t *exclude;
	
	  /*
	   * per instance fields
	   */
	int error;
	int status;
	  /* the public path, hiding translation */
	char path[PATH_MAX+1];
	  /* the public resolved path, hiding translation */
	char reslvpath[PATH_MAX+1];  
	  /* the real resolved path, exposing tranlsation */
	char truepath[PATH_MAX+1];
	  /* the real translated path */
	char translpath[PATH_MAX+1];
	  /* the list of all the equiv paths conducing to "reslvpath" */
	string_t *equivpaths;  
	  /* the real path used to flag translation status */
	char mtranslpath[PATH_MAX+1];
	  /* the path given to a wrapped opendir */
	char mdirlspath[PATH_MAX+1];
} instw_t;

static instw_t __instw;

static int canonicalize(const char *,char *);
static int reduce(char *);
static int make_path(const char *);
static int copy_path(const char *,const char *);
static const char *resolve_parent(const char *,char *,size_t);
static const char *resolve_dir(const char *,char *,size_t);
static inline int path_excluded(const char *);
static int unlink_recursive(const char *);

int expand_path(string_t **,const char *,const char *);
int parse_suffix(char *,char *,const char *);

  /* a lazy way to avoid sizeof */
#define mallok(T,N)  (T *)malloc((N)*sizeof(T))
  /* single method used to minimize excessive returns */
#define finalize(code) {rcod=code;goto finalize;}

#if DEBUG
#ifndef __USE_FILE_OFFSET64
static int __instw_printdirent(struct dirent*);
#endif
static int __instw_printdirent64(struct dirent64*);
#endif

#ifdef DEBUG
static int instw_print(instw_t *);
#endif
static int instw_init(void);
static int instw_fini(void);

static int instw_new(instw_t *);
static int instw_delete(instw_t *);

  /* references a translated file in /mtransl */
static int instw_setmetatransl(instw_t *);

static int instw_setpath(instw_t *,const char *);
static int instw_setpathrel(instw_t *, int, const char *);
static int instw_getstatus(instw_t *,int *);
static int instw_apply(instw_t *);
static int instw_filldirls(instw_t *);
static int instw_makedirls(instw_t *);

static int backup(const char *);

static int vlambda_log(const char *logname,const char *format,va_list ap);

/*
static int lambda_log(const char *logname,const char *format,...)
#ifdef __GNUC__
	__attribute__((format(printf,2,3)))
#endif 
;
*/

static inline int logg(const char *format,...)
#ifdef __GNUC__
	/* Tell gcc that this function behaves like printf()
	 * for parameters 1 and 2                            */
	__attribute__((format(printf, 1, 2)))
#endif /* defined __GNUC__ */
;

static inline int debug(int dbglvl,const char *format,...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif 
;

#define unset_okwrap() (__instw.gstatus &= ~INSTW_OKWRAP)
#define reset_okwrap() (__instw.gstatus |= INSTW_OKWRAP)

/*
 * *****************************************************************************
 */

static void *libc_handle=NULL;
static void initialize(void) {
	if (libc_handle)
		return;

	#ifdef BROKEN_RTLD_NEXT
//        	printf ("RTLD_LAZY");
        	libc_handle = dlopen(LIBC_FILE, RTLD_LAZY);
	#else
 //       	printf ("RTLD_NEXT");
        	libc_handle = RTLD_NEXT;
	#endif

	true_time        = dlsym(libc_handle, "time");
	true_chdir       = dlsym(libc_handle, "chdir");
	true_chmod       = dlsym(libc_handle, "chmod");
	true_chown       = dlsym(libc_handle, "chown");
	true_chroot      = dlsym(libc_handle, "chroot");
	true_creat       = dlsym(libc_handle, "creat");
	true_fchmod      = dlsym(libc_handle, "fchmod");
	true_fchown      = dlsym(libc_handle, "fchown");
	true_fopen       = dlsym(libc_handle, "fopen");
	true_ftruncate   = dlsym(libc_handle, "ftruncate");
	true_getcwd      = dlsym(libc_handle, "getcwd");
	true_lchown      = dlsym(libc_handle, "lchown");
	true_link        = dlsym(libc_handle, "link");
	true_mkdir       = dlsym(libc_handle, "mkdir");
	true_xmknod      = dlsym(libc_handle, "__xmknod");
	true_open        = dlsym(libc_handle, "open");
	true_opendir     = dlsym(libc_handle, "opendir");
	true_readdir     = dlsym(libc_handle, "readdir");
	true_readlink    = dlsym(libc_handle, "readlink");
	true_realpath    = dlsym(libc_handle, "realpath");
	true_rename      = dlsym(libc_handle, "rename");
	true_rmdir       = dlsym(libc_handle, "rmdir");
	true_scandir     = dlsym(libc_handle, "scandir");
	true_xstat       = dlsym(libc_handle, "__xstat");
	true_lxstat      = dlsym(libc_handle, "__lxstat");
	true_symlink     = dlsym(libc_handle, "symlink");
	true_truncate    = dlsym(libc_handle, "truncate");
	true_unlink      = dlsym(libc_handle, "unlink");
	true_utime       = dlsym(libc_handle, "utime");
	true_setxattr    = dlsym(libc_handle, "setxattr");
        true_utimes      = dlsym(libc_handle, "utimes");
        true_utimensat   = dlsym(libc_handle, "utimensat");

#ifdef WRAP_TIME64_ENTRY_POINTS
	true_stat64_time64    = dlsym(libc_handle, "__stat64_time64");
	true_lstat64_time64   = dlsym(libc_handle, "__lstat64_time64");
	true_fstatat64_time64 = dlsym(libc_handle, "__fstatat64_time64");
	true_utime64          = dlsym(libc_handle, "__utime64");
	true_utimes64         = dlsym(libc_handle, "__utimes64");
	true_utimensat64      = dlsym(libc_handle, "__utimensat64");
#endif

        true_access      = dlsym(libc_handle, "access");



	true_creat64     = dlsym(libc_handle, "creat64");
	true_fopen64     = dlsym(libc_handle, "fopen64");
	true_ftruncate64 = dlsym(libc_handle, "ftruncate64");
	true_open64      = dlsym(libc_handle, "open64");
	true_readdir64   = dlsym(libc_handle, "readdir64");
	true_scandir64   = dlsym(libc_handle, "scandir64");
	true_xstat64     = dlsym(libc_handle, "__xstat64");
	true_lxstat64    = dlsym(libc_handle, "__lxstat64");
	true_truncate64  = dlsym(libc_handle, "truncate64");
        true_removexattr = dlsym(libc_handle, "removexattr");


	true_openat      = dlsym(libc_handle, "openat");
	true_fchmodat      = dlsym(libc_handle, "fchmodat");
	true_fchownat      = dlsym(libc_handle, "fchownat");
	true_fxstatat      = dlsym(libc_handle, "__fxstatat");
	true_fxstatat64      = dlsym(libc_handle, "__fxstatat64");
	true_linkat      = dlsym(libc_handle, "linkat");
	true_mkdirat      = dlsym(libc_handle, "mkdirat");
	true_readlinkat      = dlsym(libc_handle, "readlinkat");
	true_xmknodat      = dlsym(libc_handle, "__xmknodat");
	true_renameat      = dlsym(libc_handle, "renameat");
	true_renameat2     = dlsym(libc_handle, "renameat2");
	true_symlinkat     = dlsym(libc_handle, "symlinkat");
	true_unlinkat      = dlsym(libc_handle, "unlinkat");

#if (GLIBC_MINOR >= 28)
	true_statx         = dlsym(libc_handle, "statx");
#endif

	true_mkstemp       = dlsym(libc_handle, "mkstemp");
	true_mkstemp64     = dlsym(libc_handle, "mkstemp64");
	true_mkostemp      = dlsym(libc_handle, "mkostemp");
	true_mkostemp64    = dlsym(libc_handle, "mkostemp64");
	true_mkstemps      = dlsym(libc_handle, "mkstemps");
	true_mkstemps64    = dlsym(libc_handle, "mkstemps64");
	true_mkostemps     = dlsym(libc_handle, "mkostemps");
	true_mkostemps64   = dlsym(libc_handle, "mkostemps64");
	true_mkdtemp       = dlsym(libc_handle, "mkdtemp");

#if (GLIBC_MINOR >= 33)
	true_stat	 = dlsym(libc_handle, "stat");
	true_stat64	 = dlsym(libc_handle, "stat64");
	true_mknod	 = dlsym(libc_handle, "mknod");
	true_mknodat	 = dlsym(libc_handle, "mknodat");
	true_lstat	 = dlsym(libc_handle, "lstat");
	true_lstat64	 = dlsym(libc_handle, "lstat64");
	true_fstatat	 = dlsym(libc_handle, "fstatat");
	true_fstatat64 	 = dlsym(libc_handle, "fstatat64");
#endif


	if(instw_init()) exit(-1);
}

void _init(void) {
	initialize();
}

void _fini(void) {
	instw_fini();	
}

/*
 * *****************************************************************************
 */

/*
 * procedure = / rc:=vlambda_log(logname,format,ap) /
 *
 * task      = / the va_list version of the lambda_log() procedure. /
 */
static int vlambda_log(const char *logname,const char *format,va_list ap) {
	char buffer[BUFSIZE];
	int count;
	int logfd;
	int rcod=0;
        int s_errno;
 
        /* save errno */
        s_errno = errno;

        buffer[BUFSIZE-2] = '\n';
        buffer[BUFSIZE-1] = '\0';
   
	count=vsnprintf(buffer,BUFSIZE,format,ap);
	if(count == -1) {
		  /* The buffer was not big enough */
		strcpy(&(buffer[BUFSIZE - 5]), "...\n");
		count=BUFSIZE-1;
	}
        else
        {
                count = strlen(buffer);
        }
       	
	if(logname!=NULL) {
		logfd=true_open(logname,O_WRONLY|O_CREAT|O_APPEND,0666);
		if(logfd>=0) {
			if(write(logfd,buffer,count)!=count)
				syslog(	LOGLEVEL,
					"Count not write `%s' in `%s': %s\n",
					buffer,logname,strerror(errno));
			if(close(logfd) < 0)
				syslog(	LOGLEVEL,
					"Could not close `%s': %s\n",
					logname,strerror(errno));
		} else {
			syslog(	LOGLEVEL,
				"Could not open `%s' to write `%s': %s\n",
				logname,buffer,strerror(errno));
		}
	} else {
		syslog(LOGLEVEL, "%s", buffer);
	}	

	/* restore errno */
        errno = s_errno;
	
	return rcod;
}

/*
 * procedure = / rc:=lambda_log(logname,format,...) /
 *
 * task      = /   logs a message to the specified file, or via syslog
 *               if no file is specified. /
 *
 * returns   = /  0 ok. message logged / 
 *
 * note      = / 
 * 	--This *general* log procedure was justified by the debug mode 
 *  which used either stdout or stderr, thus interfering with the 
 *  observed process.
 *      --From now, we output nothing to stdout/stderr.
 * /
 *
 */
/*
static int lambda_log(const char *logname,const char *format, ...) {
	va_list ap;
	int rcod=0;;

	va_start(ap,format);
	rcod=vlambda_log(logname,format,ap);
	va_end(ap);

	return rcod;
}
*/

static inline int logg(const char *format,...) {
	char *logname;
	va_list ap;
	int rcod; 
	
	logname=getenv("INSTW_LOGFILE");
	va_start(ap,format);
	rcod=vlambda_log(logname,format,ap);
	va_end(ap);
	
	return rcod;
}

static inline int debug(int dbglvl,const char *format,...) {
	int rcod=0; 
#ifdef DEBUG
	char *logname;
	va_list ap;

	if(	__instw.dbglvl==0 ||
		dbglvl>__instw.dbglvl ||
		dbglvl<0	) return rcod;

	logname=getenv("INSTW_DBGFILE");
	va_start(ap,format);
	rcod=vlambda_log(logname,format,ap);
	va_end(ap);
#endif	

	return rcod;
}

/*
 * procedure = / rc:=canonicalize(path,resolved_path) /
 *
 * note      = /
 *	--We use realpath here, but this function calls __lxstat().
 *	We want to only use "real" calls in wrapping code, hence the 
 *      barrier established by unset_okwrap()/reset_okwrap().
 *      --We try to canonicalize as much as possible, considering that 
 * /
 */
static int canonicalize(const char *path, char *resolved_path) {
        int s_errno;

        /* save errno */
        s_errno = errno;

	unset_okwrap();

	if(!realpath(path,resolved_path)) {
		if((path[0] != '/')) {
			/* The path could not be canonicalized, append it
		 	 * to the current working directory if it was not 
		 	 * an absolute path                               */
			true_getcwd(resolved_path, PATH_MAX-2);
			resolved_path[MAXPATHLEN-2] = '\0';
			strcat(resolved_path, "/");
			strncat(resolved_path, path, MAXPATHLEN - 1 - strlen(resolved_path));
		} else {
			strcpy(resolved_path,path);
		}
	}

	reset_okwrap();

#if DEBUG
	debug(4,"canonicalize(%s,%s)\n",path,resolved_path);
#endif
        /* restore errno */
        errno = s_errno;

	return 0;
} 

/*
 * procedure = / rc:=reduce(path) /
 *
 * task      = /   reduces all occurences of "..", ".", and extra "/" in path.
 *
 * inputs    = / path               The modifiable string containing the path
 * outputs   = / path               The reduced path.
 *
 * returns   = /  0 ok. path reduced
 *               -1 failed. cf errno /
 * note      = /
 *      --Very similar to canonicalize()/realpath() except we don’t do link-
 *      expansion
 *      --This is purely a string manipulation function (i.e., no verification
 *      of a path’s validity occurs).
 *      --Additionally, we try to do reduction “in-place” since the ending
 *      path is shorter than the beginning path.
 *      --Also, we want only absolute paths (other paths will throw an error)
 * /
 */
static int reduce(char *path) {
	int len;
	char *off;

	if(path == NULL || *path != '/') {
		errno = EINVAL;
		return -1;
	}

	len = strlen(path);

	/* First, get rid of double / */
	if((off = strstr(path, "//"))) {
		memmove(off, off+1, len - (off-path));
		return reduce(path);
	}

	/* Then, worry about /./  */
	if((off = strstr(path, "/./"))) {
		memmove(off, off+2, len - 1 - (off-path));
		return reduce(path);
	}
	
	/* Finally, do /../ */
	if((off = strstr(path, "/../"))) {
		char *off2 = off;
		if(off2++ != path)
			while((--off2)[-1] != '/');
		memmove(off2, off+4, len - 3 - (off-path));
		return reduce(path);
	}

	/* Beautify ending */
	switch(path[len - 1]) {
		case '.':
			switch(path[len - 2]) {
				default:
					return 0;
				case '.':
					if(len != 3) {
						off = path+len-3;
						if(*off-- != '/')
							return 0;
						while(*--off != '/');
						off[1] = 0;
						return reduce(path);
					}
				case '/': ;
			}
		case '/':
			if(len != 1) {
				path[len-1] = 0;
				return reduce(path);
			}
		default:
			return 0;
	}
}

/*
 * procedure = / result:=resolve_parent(path,canonical,canonsz) /
 *
 * task      = / resolve the symlinks found in the directory part of 'path',
 *               leaving its last component untouched, and store the outcome
 *               in 'canonical', a buffer of 'canonsz' bytes.
 *
 *               This matters when mirroring a path into a directory tree of
 *               our own, the backup area. We recreate the parent
 *               directories there with mkdir(), so a file reached through a
 *               symlinked directory such as /lib on a merged-/usr system
 *               lands under a real directory named 'lib'. Restore that tree
 *               over / and the /lib symlink becomes a directory, leaving
 *               the system unable to run a single binary.
 *
 *               The last component stays as it is, on purpose: a symlink
 *               there has to be saved as a symlink.                        /
 *
 * returns   = / 'canonical' on success, 'path' itself if the parent
 *               directory cannot be resolved or the result does not fit    /
 */

static const char *resolve_parent(const char *path,char *canonical,size_t canonsz) {
	char parent[PATH_MAX+1];
	char resolved[PATH_MAX+1];
	const char *base;
	size_t plen,rlen;
	int s_errno;

	  /* realpath() clobbers errno even when it succeeds */
	s_errno=errno;

	if((base=strrchr(path,'/'))==NULL) return path;
	base++;

	  /* the parent is / itself (or path *is* /): nothing to resolve */
	if((plen=base-path)<=1) return path;

	  /* everything but the trailing '/' */
	if((plen-1)>PATH_MAX) return path;
	memcpy(parent,path,plen-1);
	parent[plen-1]='\0';

	if(realpath(parent,resolved)==NULL) {
		errno=s_errno;
		return path;
	}

	  /* realpath() only yields a trailing '/' for the root directory */
	rlen=strlen(resolved);
	if(rlen && resolved[rlen-1]=='/') resolved[--rlen]='\0';

	if((rlen+1+strlen(base))>=canonsz) {
		errno=s_errno;
		return path;
	}

	strcpy(canonical,resolved);
	canonical[rlen]='/';
	strcpy(canonical+rlen+1,base);

	errno=s_errno;

#if DEBUG
	if(strcmp(path,canonical))
		debug(3,"resolve_parent: %s -> %s\n",path,canonical);
#endif

	return canonical;
}

/* Resolve an existing symlink to a directory, as mkdir() needs for */
/* aliases such as /lib -> /usr/lib.                                */

static const char *resolve_dir(const char *path,char *canonical,size_t canonsz) {
	char resolved[PATH_MAX+1];
	struct stat inode;
	int s_errno;

	  /* realpath() and the failed stats clobber errno */
	s_errno=errno;

	if(	true_lstat(path,&inode)==0 && S_ISLNK(inode.st_mode) &&
		true_stat(path,&inode)==0 && S_ISDIR(inode.st_mode) &&
		realpath(path,resolved)!=NULL &&
		strlen(resolved)<canonsz ) {

		strcpy(canonical,resolved);
		errno=s_errno;

#if DEBUG
		debug(3,"resolve_dir: %s -> %s\n",path,canonical);
#endif

		return canonical;
	}

	errno=s_errno;

	return path;
}

static int make_path (const char *path) {
	char checkdir[BUFSIZ];
	struct stat inode;
	int s_errno;
	int i = 0;
        /* save errno */
        s_errno = errno;

#if DEBUG
	debug(2,"===== make_path: %s\n", path);
#endif

	while ( path[i] != '\0' ) {
		checkdir[i] = path[i];
		if (checkdir[i] == '/') {  /* Each time a '/' is found, check if the    */
			checkdir[i+1] = '\0';   /* path exists. If it doesn't, we create it. */
			if (true_stat (checkdir, &inode) < 0)
				true_mkdir (checkdir, S_IRWXU);
		}
		i++;
	}
	
        /* restore errno */
        errno = s_errno;
        
	return 0;
}


/*
 * procedure = / rc:=copy_path(truepath,translroot) /
 *
 * task      = /   do an exact translation of 'truepath' under 'translroot', 
 *               the directory path to the new objet is not created / 
 *
 * returns   = /  0 ok. translation done 
 *               -1 failed. cf errno /
 *
 * note      = / 
 *	--we suppose that 'translroot' has no trailing '/' 
 *	--no overwrite is done is the target object already exists 
 *	--the copy method depends on the source object type. 
 *	--we don't fail if the source object doesn't exist. 
 *	--we don't create the directory path because that would lead in the 
 *      the translation case not to reference the newly created directories
 * /
 */
static int copy_path(const char *truepath,const char *translroot) {
	int rcod;
	char buffer[BUFSIZ];
	int bytes;
	char translpath[PATH_MAX+1];
	struct stat trueinfo;
	struct stat translinfo;
	int truefd;
	int translfd;
	struct utimbuf timbuf;
	char linkpath[PATH_MAX+1];
	ssize_t linksz;

#if DEBUG
	debug(2,"copy_path(%s,%s)\n",truepath,translroot);
#endif

	rcod=true_lstat(truepath,&trueinfo);
	if(rcod<0 && errno!=ENOENT) return -1;
	if(!rcod) {
		int plen=snprintf(translpath,sizeof(translpath),"%s%s",
		                  translroot,truepath);
		if(plen<0) return -1;
		if(plen>=(int)sizeof(translpath)) {
			errno=ENAMETOOLONG;
			return -1;
		}

		if(!true_lstat(translpath,&translinfo)) return 0;

		  /* symbolic links */
		if(S_ISLNK(trueinfo.st_mode)) {
			if((linksz=true_readlink(truepath,linkpath,PATH_MAX))<0) return -1;
			linkpath[linksz]='\0';
			if(true_symlink(linkpath,translpath)!=0) return -1;
		}

		  /* regular file */
		if(S_ISREG(trueinfo.st_mode)) {
			int failed=0,saved=0;

			if((truefd=true_open(truepath,O_RDONLY))<0) return -1;
			if((translfd=true_open(	translpath,
						O_WRONLY|O_CREAT|O_TRUNC,
						trueinfo.st_mode))<0	) {
				saved=errno;
				close(truefd);
				errno=saved;
				return -1;
			}			
			
			  /* A short write leaves a truncated copy behind, and
			   * the caller would go on to package it. */
			while(!failed && (bytes=read(truefd,buffer,BUFSIZ))>0) {
				int off=0;
				while(off<bytes) {
					ssize_t w=write(translfd,buffer+off,
					                bytes-off);
					if(w>0) { off+=w; continue; }
					  /* w==0 would spin here forever */
					if(w<0 && errno==EINTR) continue;
					if(w==0) errno=ENOSPC;
					failed=1;
					break;
				}
			}
			if(bytes<0) failed=1;
			if(failed) saved=errno;
	
			close(truefd);
			close(translfd);

			if(failed) {
				  /* Leave nothing half written for the package
				   * to pick up. */
				true_unlink(translpath);
				errno=saved;
				return -1;
			}
		}
	
		  /* directory */
		if(S_ISDIR(trueinfo.st_mode)) {
			if(true_mkdir(translpath,trueinfo.st_mode)) return -1;
		}
	
		  /* block special file */
		if(S_ISBLK(trueinfo.st_mode)) {
			if(true_mknod(	translpath,trueinfo.st_mode|S_IFBLK,
					trueinfo.st_rdev	)) return -1;
		}
	
		  /* character special file */
		if(S_ISCHR(trueinfo.st_mode)) {
			if(true_mknod(	translpath,trueinfo.st_mode|S_IFCHR,
					trueinfo.st_rdev	)) return -1;
		}
		 
		  /* fifo special file */
		if(S_ISFIFO(trueinfo.st_mode)) {
			if(true_mknod(translpath,trueinfo.st_mode|S_IFIFO,0))
				return -1;
		}
		
		timbuf.actime=trueinfo.st_atime;
		timbuf.modtime=trueinfo.st_mtime;
		true_utime(translpath,&timbuf);
		
		if(!S_ISLNK(trueinfo.st_mode)) {
			true_chown(translpath,trueinfo.st_uid,trueinfo.st_gid);
			true_chmod(translpath,trueinfo.st_mode); 
		}	
	}

	return 0;
}

/*
 * procedure = / rc:=path_excluded(truepath) /
 *
 * task      = /   indicates if the given path is or is hosted under any
 *               of the exclusion list members. /
 *
 * returns   = /  0 is not a member
 *                1 is a member /
 *
 * note      = /   __instw.exclude must be initialized / 
 * 
 */
static inline int path_excluded(const char *truepath) {
	string_t *pnext;
	int result;

	result=0;
	pnext=__instw.exclude;

	while(pnext!=NULL) {
		if(strstr(truepath,pnext->string)==truepath) {
			result=1;
			break;
		}
		pnext=pnext->next;	
	}

	return result;
}

/*
 * procedure = / rc:=unlink_recursive(truepath) /
 *
 * task      = /   dangerous function that unlink either a file or 
 *               an entire subtree. / 
 *
 * returns   = /  0 ok. recursive unlink done 
 *               -1 failed. cf errno /
 *
 * note      = / 
 *	--this procedure was needed by instw_makedirls(), in order to 
 *	erase a previously created temporary subtree.
 *      --it must be called with an absolute path, and only to delete 
 *      well identified trees.
 *      --i think it is a very weak implementation, so avoid using it
 *      to unlink too deep trees, or rewrite it to avoid recursivity.
 * /
 * 
 */
static int unlink_recursive(const char *truepath) {
	int rcod;
	struct stat trueinfo;
	DIR *wdir;
	struct dirent *went;
	char wpath[PATH_MAX+1];
	struct stat winfo;

#if DEBUG
	debug(2,"unlink_recursive(%s)\n",truepath);
#endif

	rcod=true_lstat(truepath,&trueinfo);
	if(rcod<0 && errno!=ENOENT) return -1;
	if(rcod!=0) return 0;

	if(S_ISDIR(trueinfo.st_mode)) {
		wdir=true_opendir(truepath);
		if(wdir==NULL) return -1;
		while((went=true_readdir(wdir))!=NULL) {
			  /* we avoid big inifinite recursion troubles */
			if( 	went->d_name[0]=='.' && 
				(	(went->d_name[1]=='\0') ||
					(	went->d_name[1]=='.' &&
						went->d_name[2]=='\0') ) ) 
				{ continue; }	
			
			  /* let's get the absolute path to this entry */
			strcpy(wpath,truepath);	
			strcat(wpath,"/");
			strcat(wpath,went->d_name);
			rcod=true_lstat(wpath,&winfo);
			if(rcod!=0) {
				closedir(wdir);
				return -1;
			}	
		
			if(S_ISDIR(winfo.st_mode)) {
				unlink_recursive(wpath);
				true_rmdir(wpath);
			} else {
				true_unlink(wpath);
			}
		}
		closedir(wdir);
		true_rmdir(truepath);
	} else {
		true_unlink(truepath);
	}

	return rcod;
}

/* 
 * procedure = / rc:=expand_path(&list,prefix,suffix) /
 *
 * task      = /   from a given path, generates all the paths that could 
 *               be derived from it, through symlinks expansion. /
 * 
 * note      = /
 *	--this procedure has been implemented to enhance the method used
 *	to reference files that have been translated.
 *	--briefly, it is necessary to reference all the paths that could
 *	lead to a file, not only the path and the associated real path.
 * /
 */
int expand_path(string_t **list,const char *prefix,const char *suffix) {
	char nprefix[PATH_MAX+1];
	char nwork[PATH_MAX+1];
	char nsuffix[PATH_MAX+1];
	char lnkpath[PATH_MAX+1];
	size_t lnksz=0;
	string_t *pthis=NULL;
	string_t *list1=NULL;
	string_t *list2=NULL;
	struct stat ninfo;
	int rcod=0;
	char pnp[PATH_MAX+1];
	char pns[PATH_MAX+1];
	size_t len;

#if DEBUG
	debug(4,"expand_path(%p,%s,%s)\n",list,prefix,suffix);
#endif

	  /* nothing more to expand, stop condition */
	if(suffix[0]=='\0') {
		(*list)=mallok(string_t,1);
		(*list)->string=malloc(strlen(prefix)+1);
		strcpy((*list)->string,prefix);
		(*list)->next=NULL;
		finalize(0);	
	}

	  /* we parse the next suffix subscript */	
	parse_suffix(pnp,pns,suffix);	
	strcpy(nprefix,prefix);
	strcat(nprefix,pnp);
	strcpy(nsuffix,pns);

	rcod=true_lstat(nprefix,&ninfo);
	if( (rcod!=0) ||
	    (rcod==0 && !S_ISLNK(ninfo.st_mode))) {
		expand_path(list,nprefix,nsuffix);	
	} else {
		expand_path(&list1,nprefix,nsuffix);
		
		lnksz=true_readlink(nprefix,lnkpath,PATH_MAX);
		lnkpath[lnksz]='\0';
		if(lnkpath[0]!='/') {
			strcpy(nprefix,prefix);
			len=strlen(lnkpath);
			if(lnkpath[len-1]=='/') {lnkpath[len-1]='\0';}
			strcpy(nwork,"/");
			strcat(nwork,lnkpath);
			strcat(nwork,nsuffix);
			strcpy(nsuffix,nwork);
			expand_path(&list2,nprefix,nsuffix);
		} else {
			len=strlen(lnkpath);
			if(lnkpath[len-1]=='/') {lnkpath[len-1]='\0';}
			strcpy(nprefix,"");
			strcpy(nwork,lnkpath);
			strcat(nwork,nsuffix);	
			strcpy(nsuffix,nwork);
			expand_path(&list2,nprefix,nsuffix);
		}

		*list=list1;
		pthis=*list;
		while(pthis->next!=NULL) {pthis=pthis->next;}
		pthis->next=list2;
	}	

	finalize:

	return rcod;		
}

int parse_suffix(char *pnp,char *pns,const char *suffix) {
	int rcod=0;
	char *p;

	strcpy(pnp,suffix);
	strcpy(pns,"");

	p=pnp;
	
	if(*p=='\0') {
		strcpy(pns,"");
	} else {
		p++;
		while((*p)!='\0') {
			if(*p=='/') {
				strcpy(pns,p);
				*p='\0';
				break;
			}
			p++;	
		}
	}

	return rcod;
}

/*
 * *****************************************************************************
 */

#ifndef __USE_FILE_OFFSET64
static int __instw_printdirent(struct dirent *entry) {

	if(entry!=NULL) {
		debug(	4,
			"entry(%p) {\n"
			"\td_ino     : %" PRId64 "\n"
			"\td_off     : %" PRId64 "\n"
			"\td_reclen  : %d\n"
			"\td_type    : %d\n"
			"\td_name    : \"%.*s\"\n",
			entry,
			(int64_t) entry->d_ino,
			(int64_t) D_OFF(entry->d_off),
			entry->d_reclen,
			(int)entry->d_type,
			(int)entry->d_reclen,(char*)(entry->d_name)
			);
	} else {
		debug(	4,"entry(null) \n");
	}

	return 0;
}
#endif

static int __instw_printdirent64(struct dirent64 *entry) {

	if(entry!=NULL) {
		debug(	4,
			"entry(%p) {\n"
			"\td_ino     : %" PRId64 "\n"
			"\td_off     : %" PRId64 "d\n"
			"\td_reclen  : %d\n"
			"\td_type    : %d\n"
			"\td_name    : \"%.*s\"\n",
			entry,
			entry->d_ino,
			D_OFF(entry->d_off),
			entry->d_reclen,
			(int)entry->d_type,
			(int)entry->d_reclen,(char*)(entry->d_name)
			);
	} else {
		debug(	4,"entry(null) \n");
	}

	return 0;
}

/*
 * *****************************************************************************
 */

#ifdef DEBUG
static int instw_print(instw_t *instw) {
	string_t *pnext;
	int i;
	
	debug(	4,
		"instw(%p) {\n"
		"\tgstatus     : %d\n"
		"\terror       : %d\n"
		"\tstatus      : %d\n"
		"\tdbglvl      : %d\n"
		"\tpid         : %d\n"
		"\troot        : \"%.*s\"\n"
		"\tbackup      : \"%.*s\"\n"
		"\ttransl      : \"%.*s\"\n"
		"\tmeta        : \"%.*s\"\n"
		"\tmtransl     : \"%.*s\"\n"
		"\tmdirls      : \"%.*s\"\n",
		instw,
		instw->gstatus,
		instw->error,
		instw->status,
		instw->dbglvl,
		instw->pid,
		PATH_MAX,(char*)((instw->root)?:"(null)"),
		PATH_MAX,(char*)((instw->backup)?:"(null)"),
		PATH_MAX,(char*)((instw->transl)?:"(null)"),
		PATH_MAX,(char*)((instw->meta)?:"(null)"),
		PATH_MAX,(char*)((instw->mtransl)?:"(null)"),
		PATH_MAX,(char*)((instw->mdirls)?:"(null)")
		);

	pnext=instw->exclude;
	i=0;
	while(pnext!=NULL) {
		debug(	4,
			"\texclude     : (%02d) \"%.*s\"\n",
			++i,PATH_MAX,pnext->string	);
		pnext=pnext->next;	
	}

	debug(	4,
		"\tpath        : \"%.*s\"\n"
		"\treslvpath   : \"%.*s\"\n"
		"\ttruepath    : \"%.*s\"\n"
		"\ttranslpath  : \"%.*s\"\n",
		PATH_MAX,(char*)(instw->path),
		PATH_MAX,(char*)(instw->reslvpath),
		PATH_MAX,(char*)(instw->truepath),
		PATH_MAX,(char*)(instw->translpath)
		);

	pnext=instw->equivpaths;
	i=0;
	while(pnext!=NULL) {
		debug(	4,
			"\tequivpaths  : (%02d) \"%.*s\"\n",
			++i,PATH_MAX,pnext->string	);
		pnext=pnext->next;	
	}

	debug(	4,
		"\tmtranslpath : \"%.*s\"\n"
		"\tmdirlspath  : \"%.*s\"\n"
		"}\n",
		PATH_MAX,(char*)(instw->mtranslpath),
		PATH_MAX,(char*)(instw->mdirlspath)
		);

	return 0;	
}
#endif

/*
 * procedure = / rc:=instw_init() /
 *
 * task      = /   initializes the '__transl' fields, and fills the fields 
 *		 provided by the environment. 
 *                 this structure is a reference enabling faster 
 *		 local structure creations. /
 *
 * returns   = /  0 ok. env set 
 *               -1 failed. /
 */
static int instw_init(void) {
	char *proot;
	char *pbackup;
	char *ptransl;
	char *pdbglvl;
	struct stat info;
	char wrkpath[PATH_MAX+1];
	char *pexclude;
	char *exclude;
	string_t **ppnext;
	int okinit;
	int okbackup;
	int oktransl;
	int okwrap;

#if DEBUG
	  /*
	   * We set the requested dynamic debug level
	   */
	__instw.dbglvl=0;
	if((pdbglvl=getenv("INSTW_DBGLVL"))) {
		__instw.dbglvl=atoi(pdbglvl);
		if(__instw.dbglvl>4) { __instw.dbglvl=4; }
		if(__instw.dbglvl<0) { __instw.dbglvl=0; }
	}

	debug(2,"instw_init()\n");
#endif

	okinit=0;
	okbackup=0;
	oktransl=0;
	okwrap=0;
	
	__instw.gstatus=0;
	__instw.error=0;
	__instw.status=0;
	__instw.pid=getpid();
	__instw.root=NULL;
	__instw.backup=NULL;
	__instw.transl=NULL;
	__instw.meta=NULL;
	__instw.mtransl=NULL;
	__instw.mdirls=NULL;
	__instw.exclude=NULL;

	__instw.path[0]='\0';
	__instw.reslvpath[0]='\0';
	__instw.truepath[0]='\0';
	__instw.translpath[0]='\0';

	__instw.equivpaths=NULL;

	__instw.mtranslpath[0]='\0';
	__instw.mdirlspath[0]='\0';

	  /* nothing can be activated without that, anyway */
	if((proot=getenv("INSTW_ROOTPATH"))) {
		size_t rlen;
		  /* On failure realpath leaves wrkpath undefined, so fall
		   * back to the value as it was given to us. */
		if(realpath(proot,wrkpath)==NULL) {
			strncpy(wrkpath,proot,PATH_MAX);
			wrkpath[PATH_MAX]='\0';
		}
		  /* Both of those can leave it empty, and then there is no
		   * last character to look at. */
		rlen=strlen(wrkpath);
		if(rlen && wrkpath[rlen-1]=='/')
			wrkpath[rlen-1]='\0';
		__instw.root=malloc(strlen(wrkpath)+1);
		if(NULL==__instw.root) return -1;
		strcpy(__instw.root,wrkpath);

		  /* this root path must exist */
		if(__instw.root[0]=='\0' || true_stat(__instw.root,&info)) {
			fprintf(stderr,
				"Please check the INSTW_ROOTPATH and "
				"be sure that it does exist please !\n"
				"given value : %s\n", __instw.root);
			return -1;	
		}

		if((pbackup=getenv("INSTW_BACKUP"))) {
			if(	!strcmp(pbackup,"1") ||
				!strcmp(pbackup,"yes") ||
				!strcmp(pbackup,"true")	) {
			
				if((strlen(__instw.root)+strlen(_BACKUP))>PATH_MAX) {
					fprintf(stderr,
						"Backup path would exceed PATH_MAX. Aborting.\n");
					return -1;	
				}
				__instw.backup=malloc(strlen(__instw.root)+strlen(_BACKUP)+1);
				if(NULL==__instw.backup) return -1;
				strcpy(__instw.backup,__instw.root);
				strcat(__instw.backup,_BACKUP);

				  /* we create the path that precautiously shouldn't exist */
				true_mkdir(__instw.backup,S_IRWXU);  

				okbackup=1;
			} 
			else if(	strcmp(pbackup,"0") && 
					strcmp(pbackup,"no") &&
					strcmp(pbackup,"false")	) {
				fprintf(stderr,
					"Please check the INSTW_BACKUP value please !\n"
					"Recognized values are : 1/0,yes/no,true/false.\n");
				return -1;	
			}		
		}

		if((ptransl=getenv("INSTW_TRANSL"))) {
			if(	!strcmp(ptransl,"1") ||
				!strcmp(ptransl,"yes") ||
				!strcmp(ptransl,"true")	) {
		
				if((strlen(__instw.root)+strlen(_TRANSL))>PATH_MAX) {
					fprintf(stderr,
						"Transl path would exceed PATH_MAX. Aborting.\n");
					return -1;	
				}
				__instw.transl=malloc(strlen(__instw.root)+strlen(_TRANSL)+1);
				if(NULL==__instw.transl) return -1;
				strcpy(__instw.transl,__instw.root);
				strcat(__instw.transl,_TRANSL);
			
				  /* we create the path that precautiously shouldn't exist */
				true_mkdir(__instw.transl,S_IRWXU);  
			
				if((strlen(__instw.root)+strlen(_META))>PATH_MAX) {
					fprintf(stderr,
						"Meta path would exceed PATH_MAX. Aborting.\n");
					return -1;	
				}
				
				__instw.meta=malloc(strlen(__instw.root)+strlen(_META)+1);
				if(NULL==__instw.meta) return -1;
				strcpy(__instw.meta,__instw.root);
				strcat(__instw.meta,_META);
			
				  /* we create the path that precautiously shouldn't exist */
				true_mkdir(__instw.meta,S_IRWXU); 
	
				__instw.mtransl=malloc(strlen(__instw.meta)+strlen(_MTRANSL)+1);
				if(NULL==__instw.mtransl) return -1;
				strcpy(__instw.mtransl,__instw.meta);
				strcat(__instw.mtransl,_MTRANSL);
			
				  /* we create the path that precautiously shouldn't exist */
				true_mkdir(__instw.mtransl,S_IRWXU); 

				__instw.mdirls=malloc(strlen(__instw.meta)+strlen(_MDIRLS)+1);
				if(NULL==__instw.mdirls) return -1;
				strcpy(__instw.mdirls,__instw.meta);
				strcat(__instw.mdirls,_MDIRLS);
			
				  /* we create the path that precautiously shouldn't exist */
				true_mkdir(__instw.mdirls,S_IRWXU); 

				oktransl=1;
			} 
			else if(	strcmp(ptransl,"0") && 
					strcmp(ptransl,"no") &&
					strcmp(ptransl,"false")	) {
				fprintf(stderr,
					"Please check the INSTW_TRANSL value please !\n"
					"Recognized values are : 1/0,yes/no,true/false.\n");
				return -1;	
			}		
		}
	}

	  /*
	   * we end up constructing the exclusion list
	   */

	ppnext=&__instw.exclude;

	  /* we systematically add the root directory */
	if(__instw.gstatus&INSTW_OKTRANSL) {
		*ppnext=mallok(string_t,1);
		if(*ppnext==NULL) return -1;
		(*ppnext)->string=NULL;
		(*ppnext)->next=NULL;
		if(realpath(__instw.root,wrkpath)==NULL) {
			strncpy(wrkpath,__instw.root,PATH_MAX);
			wrkpath[PATH_MAX]='\0';
		}
		(*ppnext)->string=malloc(strlen(wrkpath)+1);
		strcpy((*ppnext)->string,wrkpath);
		ppnext=&(*ppnext)->next;
	}	
	   
	if((pexclude=getenv("INSTW_EXCLUDE"))) {
		exclude=malloc(strlen(pexclude)+1);
		strcpy(exclude,pexclude);
		pexclude=strtok(exclude,",");

		while(pexclude!=NULL) {
			*ppnext=malloc(sizeof(string_t));
			if(*ppnext==NULL) return -1;
			(*ppnext)->string=NULL;
			(*ppnext)->next=NULL;
			  /* let's store the next excluded path */
			if(strlen(pexclude)>PATH_MAX) return -1;
			  /* An excluded path need not exist on this
			   * machine, and then realpath fails. */
			if(realpath(pexclude,wrkpath)==NULL) {
				strncpy(wrkpath,pexclude,PATH_MAX);
				wrkpath[PATH_MAX]='\0';
			}
			(*ppnext)->string=malloc(strlen(wrkpath)+1);
			strcpy((*ppnext)->string,wrkpath);
			ppnext=&(*ppnext)->next;
			pexclude=strtok(NULL,",");
		}
	
	}


	okinit=1;
	okwrap=1;

	if(okinit) __instw.gstatus |= INSTW_INITIALIZED;
	if(okwrap) __instw.gstatus |= INSTW_OKWRAP;
	if(okbackup) __instw.gstatus |= INSTW_OKBACKUP;
	if(oktransl) __instw.gstatus |= INSTW_OKTRANSL;	
	
#if DEBUG
	debug(4,"__instw(%p)\n",&__instw);
	instw_print(&__instw);
#endif

	return 0;
}

/*
 * procedure = / rc:=instw_fini() /
 *
 * task      = /   properly finalizes the instw job /  
 *
 * returns   = /  0 ok. env set 
 *               -1 failed. /
 */
static int instw_fini(void) {
	int rcod=0;
	string_t *pnext;
	string_t *pthis;

#if DEBUG
	debug(2,"instw_fini()\n");
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ) finalize(0);

	__instw.gstatus &= ~INSTW_INITIALIZED;

	if(__instw.root != NULL) {free(__instw.root);__instw.root=NULL;}	
	if(__instw.backup != NULL) {free(__instw.backup);__instw.backup=NULL;}	
	if(__instw.transl != NULL) {free(__instw.transl);__instw.transl=NULL;}	
	if(__instw.meta != NULL) {free(__instw.meta);__instw.meta=NULL;}	
	if(__instw.mtransl != NULL) {free(__instw.mtransl);__instw.mtransl=NULL;}	
	if(__instw.mdirls != NULL) {free(__instw.mdirls);__instw.mdirls=NULL;}	

	pthis=__instw.exclude;
	while(pthis != NULL) {
		free(pthis->string);
		pnext=pthis->next;
		free(pthis);
		pthis=pnext;
	}
	__instw.exclude=NULL;

	finalize:

	return rcod;
}

/*
 * procedure = / rc:=instw_new(instw) /
 *
 * task      = / Initializes a new instw_t structure before any work on it /
 *
 * returns   = /  0 ok. ready to be used
 *               -1 failed. /
 */
static int instw_new(instw_t *instw) {
	int rcod=0;

	*instw=__instw;

	instw->error=0;
	instw->status=0;
	instw->path[0]='\0';
	instw->reslvpath[0]='\0';
	instw->truepath[0]='\0';
	instw->translpath[0]='\0';
	instw->equivpaths=NULL;
	instw->mtranslpath[0]='\0';
	instw->mdirlspath[0]='\0';

	return rcod;
}

/*
 * procedure = / rc:=instw_delete(instw) /
 *
 * task      = / properly finalizes an instw structure /
 *
 * returns   = /  0 ok. ready to be used
 *               -1 failed. /
 */
static int instw_delete(instw_t *instw) {
	int rcod=0;
	string_t *pnext;
	string_t *pthis;

	pthis=instw->equivpaths;
	while(pthis != NULL) {
		free(pthis->string);
		pnext=pthis->next;
		free(pthis);
		pthis=pnext;
	}

	instw->status=0;

	return rcod;
}

/* 
 * procedure = / rc:=instw_setmetatransl(instw) /
 *
 * task      = / Refreshes as mush as possible the translation 
 *               status of a translated file /
 *
 * note      = /
 *	--this procedure is meant to be called after the various
 *   translation status flags have been setted.
 *        the only thing it does is referencing a file that has been
 *   flagged as "translated".
 *        if it is, we musn't try to use the eventual real version 
 *   of the file anymore, hence the full referencement under /mtransl.
 *
 *      --in some cases, for example when you create manually a subtree 
 *  and a file in this subtree directly directly in the translated fs 
 *  (yes, it is possible) the meta infos won't exist.
 *        so, to be able to cope with this case, we firstly try to
 *  create the full reference to the file, and if this fails, we try
 *  to reference all the traversed directories.
 * /
 */
static int instw_setmetatransl(instw_t *instw) {
	int rcod=0;
	struct stat info;
	char mtransldir[PATH_MAX+1];
	char mtranslpath[PATH_MAX+1];
	char reslvpath[PATH_MAX+1];
	size_t mesz=0;
	int i=0;
	string_t *pthis;

#if DEBUG
	debug(3,"instw_setmetatransl(%p)\n",instw);
	instw_print(instw);
#endif

	if( !(instw->gstatus & INSTW_INITIALIZED) ||
	    !(instw->gstatus & INSTW_OKTRANSL) ) finalize(0); 

	if(!(instw->status & INSTW_TRANSLATED) ) finalize(0);

	if(instw->equivpaths==NULL) { 	
		expand_path(&(instw->equivpaths),"",instw->reslvpath);
	}	

#if DEBUG
	instw_print(instw);
#endif

	pthis=instw->equivpaths;	
	while(pthis!=NULL) {
		strcpy(mtranslpath,instw->mtransl);
		strcat(mtranslpath,pthis->string);
		strcpy(reslvpath,pthis->string);

		if( (true_stat(mtranslpath,&info)) && 
		    (true_mkdir(mtranslpath,S_IRWXU)) ) {
			strcpy(mtransldir,mtranslpath);
			mesz=strlen(instw->mtransl);
	
			for(i=0;reslvpath[i]!='\0';i++) {
				mtransldir[mesz+i]=reslvpath[i];
				if(reslvpath[i]=='/') {
					mtransldir[mesz+i+1]='\0';
					true_mkdir(mtransldir,S_IRWXU);
				}
			}
	
			true_mkdir(mtranslpath,S_IRWXU);
		}

		pthis=pthis->next;
	}

	finalize: 

	return rcod;
}

/*
 * procedure = / rc:=instw_setpath(instw,path) /
 *
 * task      = /   sets the 'instw->path' field and updates all the fields that 
 *               can be deduced from 'path', such as 'instw->translpath'. /
 *
 * inputs    = / path               The given path, as is 
 * outputs   = / instw->path        A stored copy of 'path'
 *               instw->truepath    The given path, canonicalized
 *               instw->translpath  The real translated path 
 *               instw->mtranslpath The translation status path  /
 *
 * returns   = /  0 ok. path set
 *               -1 failed. cf errno /
 */
static int instw_setpath(instw_t *instw,const char *path) {
	char canonical[PATH_MAX+1];
	const char *pcanon;

#if DEBUG
	debug(2,"instw_setpath(%p,%s)\n",instw,path);
#endif

	instw->status=0;

	  /* Truncating here would quietly operate on a different file, so
	   * refuse instead. */
	if(strlen(path)>PATH_MAX) {
		instw->error=errno=ENAMETOOLONG;
		return -1;
	}
	strcpy(instw->path,path);
	instw->truepath[0]='\0';

	if(instw->path[0]!='/') {
		size_t cwlen;
		true_getcwd(instw->truepath,PATH_MAX+1);
		cwlen=strlen(instw->truepath);
		if(cwlen && instw->truepath[cwlen-1]!='/'){
			strcat(instw->truepath,"/");
			cwlen++;
		}
		  /* The two together can be longer than the buffer. */
		if(cwlen+strlen(instw->path)>PATH_MAX) {
			instw->error=errno=ENAMETOOLONG;
			return -1;
		}
		strcat(instw->truepath,instw->path);
	} else {
		reduce(instw->path);
		strcpy(instw->truepath,instw->path);
	}
	/* remove relative elements from the truepath */
	reduce(instw->truepath);

	  /* Record paths under their real parent directories. Leave the final */
	  /* component unresolved so operations still see a symlink there.    */
	pcanon=resolve_parent(instw->truepath,canonical,sizeof(canonical));
	if(pcanon!=instw->truepath) {
		strncpy(instw->truepath,pcanon,PATH_MAX);
		instw->truepath[PATH_MAX]='\0';
	}

	  /* 
	   *   if library is not completely initialized, or if translation 
	   * is not active, we make things so it is equivalent to the
	   * to the identity, this avoid needs to cope with special cases.
	   */
	if(	!(instw->gstatus&INSTW_INITIALIZED) || 
		!(instw->gstatus&INSTW_OKTRANSL)) {
		strncpy(instw->reslvpath,instw->truepath,PATH_MAX);
		instw->reslvpath[PATH_MAX]='\0';
		strncpy(instw->translpath,instw->truepath,PATH_MAX);
		instw->translpath[PATH_MAX]='\0';
		return 0;
	}
	
	  /*
	   *   we fill instw->reslvpath , applying the inversed translation
	   * if truepath is inside /transl.
	   */
	if(strstr(instw->truepath,instw->transl)==instw->truepath) {
		strcpy(instw->reslvpath,instw->truepath+strlen(instw->transl));
	} else {
		strcpy(instw->reslvpath,instw->truepath);
	}

	  /*
	   *   if instw->path is relative, no troubles.
	   *   but if it is absolute and located under /transl, we have 
	   * to untranslate it.
	   */
	if( (instw->path[0]=='/') &&
	    (strstr(instw->path,instw->transl)==instw->path)) {
		strcpy(instw->path,instw->reslvpath);
	} 

	  /*
	   * We must detect early 'path' matching with already translated files  
	   */
	if(path_excluded(instw->truepath)) {
		strncpy(instw->translpath,instw->truepath,PATH_MAX);
		instw->translpath[PATH_MAX]='\0';
		instw->status |= ( INSTW_TRANSLATED | INSTW_IDENTITY);
	} else {
		  /* Building the real translated path */
		if(snprintf(instw->translpath,sizeof(instw->translpath),"%s%s",
		            instw->transl,instw->reslvpath)
		   >=(int)sizeof(instw->translpath)) {
			instw->error=errno=ENAMETOOLONG;
			return -1;
		}
	}	

	  /* Building the translation status path */
	if(snprintf(instw->mtranslpath,sizeof(instw->mtranslpath),"%s%s",
	            instw->mtransl,instw->reslvpath)
	   >=(int)sizeof(instw->mtranslpath)) {
		instw->error=errno=ENAMETOOLONG;
		return -1;
	}

	return 0;
}

/*
 * procedure = / rc:=instw_setpathrel(instw,dirfd,relpath) /
 *
 * task      = /   sets the 'instw->path' field and updates all the fields that
 *               can be deduced from 'path', such as 'instw->translpath'. Much
 *               like instw_setpath, except for paths relative to a dirfd. /
 *
 * inputs    = / dirfd              An open file descriptor to a directory
 *               relpath            The given path relative to dirfd, as is
 * outputs   = / instw->path        The full absolute (non-relative) path
 *               instw->truepath    The given path, canonicalized
 *               instw->translpath  The real translated path 
 *               instw->mtranslpath The translation status path  /
 *
 * returns   = /  0 ok. path set
 *               -1 failed. cf errno /
 */
static int instw_setpathrel(instw_t *instw, int dirfd, const char *relpath) {

/* This constant should be large enough to make a string that holds
 * /proc/self/fd/xxxxx  if you have an open fd with more than five digits,
 * something is seriously messed up.
 */
#define PROC_PATH_LEN 20
	
	debug(2,"instw_setpathrel(%p,%d,%s)\n",instw,dirfd,relpath);
	int retval = -1, l;
	char *newpath;
	char proc_path[PROC_PATH_LEN];
	struct stat s;


	/* If dirfd is AT_FDCWD then we got nothing to do, return the */
	/* path as-is                                                 */

	if ( dirfd == AT_FDCWD ) return instw_setpath(instw, relpath);

	snprintf(proc_path, PROC_PATH_LEN, "/proc/self/fd/%d", dirfd);
	if(true_lstat(proc_path, &s) == -1)
		goto out;
	if(!(newpath = malloc(PATH_MAX+strlen(relpath)+2)))
		goto out;
	if((l = true_readlink(proc_path, newpath, PATH_MAX)) == -1)
		goto free_out;
	newpath[l] = '/';
	strcpy(newpath + l + 1, relpath);
	
	retval = instw_setpath(instw, newpath);

free_out:
	free(newpath);
out:
	return retval;

#undef PROC_PATH_LEN
}

/*
 * procedure = / rc:=instw_getstatus(instw,status) /
 *
 * outputs   = / status  instw->path flags field status in the translated fs
 *                 INSTW_ISINROOT   file exists in the real fs 
 *                 INSTW_ISINTRANSL file exists in the translated fs
 *                 INSTW_TRANSLATED file has been translated /
 *
 * returns   = /  0 ok. stated 
 *               -1 failed. cf errno /
 */
static int instw_getstatus(instw_t *instw,int *status) {
	struct stat inode;
	struct stat rinode;
	struct stat tinode;

#if DEBUG
	debug(2,"instw_getstatus(%p,%p)\n",instw,status);
#endif

	  /* 
	   * is the file referenced as being translated ?
	   */
	if( (instw->gstatus&INSTW_INITIALIZED) &&
	    (instw->gstatus&INSTW_OKTRANSL) &&
	   !(instw->status&INSTW_TRANSLATED) &&  
	   !true_stat(instw->mtranslpath,&inode) ) {
		instw->status |= INSTW_TRANSLATED;
	}

	  /*
	   * do the file currently exist in the translated fs ?
	   */
	if( (instw->gstatus&INSTW_INITIALIZED) &&
	     (instw->gstatus&INSTW_OKTRANSL) && 
	     !true_stat(instw->translpath,&tinode) ) {
		instw->status |= INSTW_ISINTRANSL;
	}	

	  /*
	   * is it a newly created file, or a modified one ?
	   */
	if( instw->gstatus&INSTW_INITIALIZED &&
	    !true_stat(instw->reslvpath,&rinode) ) {
		instw->status |= INSTW_ISINROOT;
	}
	
	  /*
	   *   if the file exists, why is it not referenced as 
	   * being translated ?
	   *   we have to reference it and all the traversed 
	   * directories leading to it.
	   */
	if( (instw->gstatus&INSTW_INITIALIZED) &&
	    (instw->gstatus&INSTW_OKTRANSL) &&
	    (instw->status&INSTW_ISINTRANSL) &&
	    !(instw->status&INSTW_TRANSLATED) ) {
		instw->status |= INSTW_TRANSLATED;
		instw_setmetatransl(instw);
	}    
	  
	  /*
	   *   are the public resolved path and its translated counterpart 
	   * identical ? if so, we flag it
	   */
	if( (instw->gstatus & INSTW_INITIALIZED) &&
	    (instw->gstatus & INSTW_OKTRANSL) && 
	    (instw->status & INSTW_TRANSLATED) && 
	    (0==(strcmp(instw->truepath,instw->translpath))) ) {
		instw->status |= INSTW_IDENTITY;  
	}    

	*status=instw->status;

	return 0;
}

/*
 * procedure = / rc:=instw_apply(instw) /
 *
 * task      = /   actually do the translation prepared in 'transl' /
 *
 * note      = /   --after a call to instw_apply(), the translation related 
 *                 status flags are updated. 
 *                 --if a translation is requested and if the original file 
 *                 exists, all parent directories are created and referenced
 *                 if necessary.
 *                   if the original file does not exist, we translate at
 *                 least the existing path. /
 *
 * returns   = /  0 ok. translation done 
 *               -1 failed. cf errno     /
 */
static int instw_apply(instw_t *instw) {
	int rcod=0;
	int status=0;
	
	char dirpart[PATH_MAX+1];
	char basepart[PATH_MAX+1];
	char *pdir;
	char *pbase;
	struct stat reslvinfo;
	instw_t iw;
	char wpath[PATH_MAX+1];
	ssize_t wsz=0;
	char linkpath[PATH_MAX+1];


#if DEBUG
	debug(2,"instw_apply(%p)\n",instw);
	instw_print(instw);
#endif

	  /* 
	   * if library incompletely initialized or if translation 
	   * is inactive, nothing to apply 
	   */
	if( !(instw->gstatus&INSTW_INITIALIZED) ||
	    !(instw->gstatus&INSTW_OKTRANSL) )  finalize(0); 
	 
	  /* let's get the file translation status */
	if(instw_getstatus(instw,&status)) finalize(-1);

	  /* we ignore files already translated */
	if(status & INSTW_TRANSLATED) return 0;

	strcpy(basepart,instw->reslvpath);
	strcpy(dirpart,instw->reslvpath);
	
	pbase=basename(basepart);
	pdir=dirname(dirpart);

	  /* recursivity termination test, */
	if(pdir[0]=='/' && pdir[1]=='\0' && pbase[0]=='\0') {
		instw->status|=INSTW_TRANSLATED;
		finalize(0);
	}	
	
	instw_new(&iw);	
	instw_setpath(&iw,pdir);
	instw_apply(&iw);
	instw_delete(&iw);

	  /* will we have to copy the original file ? */
	if(!true_lstat(instw->reslvpath,&reslvinfo)) {
		  /* Marking the path translated after a failed copy would put
		   * a file that was never written into the package. */
		if(copy_path(instw->reslvpath,instw->transl)<0) {
			instw->error=errno;
			finalize(-1);
		}

		  /* a symlink ! we have to translate the target */
		if(S_ISLNK(reslvinfo.st_mode) &&
		   (wsz=true_readlink(instw->reslvpath,wpath,PATH_MAX))>=0) {
			wpath[wsz]='\0';

			instw_new(&iw);
			if(wpath[0]!='/') { 
				strcpy(linkpath,pdir);
				strcat(linkpath,"/");
				strcat(linkpath,wpath);
			} else {
				strcpy(linkpath,wpath);
			}

			instw_setpath(&iw,linkpath);
			instw_apply(&iw);
			instw_delete(&iw);
		}
	}

	
	instw->status|=INSTW_TRANSLATED;
	instw_setmetatransl(instw);

	finalize:

	return rcod;
}

/*
 * procedure = / rc:=instw_filldirls(instw) /
 *
 * task      = /   used to create dummy entries in the mdirlspath reflecting 
 *               the content that would have been accessible with no
 *               active translation. /
 *
 * note      = / 
 *	--This procedure must be called after instw_makedirls() has been 
 *	called itself.
 *	--It implies that the translated directory and the real one are
 *      distincts, but it does not matter if one of them, or both is empty
 * /
 */
static int instw_filldirls(instw_t *instw) {
	int rcod=0;
	DIR *wdir;
	struct dirent *went;
	char spath[PATH_MAX+1];
	char dpath[PATH_MAX+1];
	char lpath[PATH_MAX+1];
	struct stat sinfo;
	struct stat dinfo;
	int wfd;
	ssize_t wsz;
	instw_t iw_entry;
	int status=0;

#if DEBUG
	debug(2,"instw_filldirls(%p)\n",instw);
#endif

	if((wdir=true_opendir(instw->translpath))==NULL) { return -1; }	
	while((went=true_readdir(wdir))!=NULL) {
		if( 	went->d_name[0]=='.' && 
			(	(went->d_name[1]=='\0') ||
				(	went->d_name[1]=='.' &&
					went->d_name[2]=='\0') ) ) 
			{ continue; }	

		strcpy(spath,instw->translpath);
		strcat(spath,"/");
		strcat(spath,went->d_name);
		
		if(true_lstat(spath,&sinfo)) { continue; }

		strcpy(dpath,instw->mdirlspath);
		strcat(dpath,"/");
		strcat(dpath,went->d_name);

		  /* symbolic links */
		if(S_ISLNK(sinfo.st_mode)) {
			if((wsz=true_readlink(spath,lpath,PATH_MAX))>=0) { 
				lpath[wsz]='\0';
				true_symlink(lpath,dpath); 
#if DEBUG
				debug(4,"\tfilled symlink       : %s\n",dpath);
#endif
			}
				
		}

		  /* regular file */
		if(S_ISREG(sinfo.st_mode)) {
			if((wfd=true_creat(dpath,sinfo.st_mode))>=0) {
				close(wfd); 
#if DEBUG
				debug(4,"\tfilled regular file  : %s\n",dpath);
#endif
			}
		}
	
		  /* directory */
		if(S_ISDIR(sinfo.st_mode)) {
			true_mkdir(dpath,sinfo.st_mode);
#if DEBUG
			debug(4,"\tfilled directory     : %s\n",dpath);
#endif

		}
	
		  /* block special file */
		if(S_ISBLK(sinfo.st_mode)) {
			true_mknod(dpath,sinfo.st_mode|S_IFBLK,sinfo.st_rdev);
#if DEBUG
			debug(4,"\tfilled special block : %s\n",dpath);
#endif

		}
	
		  /* character special file */
		if(S_ISCHR(sinfo.st_mode)) {
			true_mknod(dpath,sinfo.st_mode|S_IFCHR,sinfo.st_rdev);
#if DEBUG
			debug(4,"\tfilled special char  : %s\n",dpath);
#endif
		}
		 
		  /* fifo special file */
		if(S_ISFIFO(sinfo.st_mode)) {
			true_mknod(dpath,sinfo.st_mode|S_IFIFO,0);
#if DEBUG
			debug(4,"\tfilled special fifo  : %s\n",dpath);
#endif
		}
			
	}
	closedir(wdir);

	if((wdir=true_opendir(instw->reslvpath))==NULL) return -1;
	while((went=true_readdir(wdir))!=NULL) {
		if( 	went->d_name[0]=='.' && 
			(	(went->d_name[1]=='\0') ||
				(	went->d_name[1]=='.' &&
					went->d_name[2]=='\0') ) ) 
			{ continue; }	

		strcpy(spath,instw->reslvpath);
		strcat(spath,"/");
		strcat(spath,went->d_name);
		if(true_lstat(spath,&sinfo)) { continue; }

		instw_new(&iw_entry);
		instw_setpath(&iw_entry,spath);
		instw_getstatus(&iw_entry,&status);

		  /*
		   *   This entry exists in the real fs, but has been 
		   * translated and destroyed in the translated fs.
		   *   So, we mustn't present it !!!
		   */
		if( (status & INSTW_TRANSLATED) &&
		    !(status & INSTW_ISINTRANSL) ) { continue; }

		strcpy(dpath,instw->mdirlspath);
		strcat(dpath,"/");
		strcat(dpath,went->d_name);
	
		  /* already exists in the translated fs, we iterate */
		if(!true_lstat(dpath,&dinfo)) { continue; }

		  /* symbolic links */
		if(S_ISLNK(sinfo.st_mode)) {
			if((wsz=true_readlink(spath,lpath,PATH_MAX))>=0) {
				lpath[wsz]='\0';
				true_symlink(lpath,dpath);
#if DEBUG
				debug(4,"\tfilled symlink       : %s\n",dpath);
#endif
			}
		}

		  /* regular file */
		if(S_ISREG(sinfo.st_mode)) {
			if((wfd=true_creat(dpath,sinfo.st_mode))>=0) {
				close(wfd); 
#if DEBUG
				debug(4,"\tfilled regular file  : %s\n",dpath);
#endif
			}	
		}
	
		  /* directory */
		if(S_ISDIR(sinfo.st_mode)) {
			true_mkdir(dpath,sinfo.st_mode);
#if DEBUG
			debug(4,"\tfilled directory     : %s\n",dpath);
#endif
		}
	
		  /* block special file */
		if(S_ISBLK(sinfo.st_mode)) {
			true_mknod(dpath,sinfo.st_mode|S_IFBLK,sinfo.st_rdev);
#if DEBUG
			debug(4,"\tfilled special block : %s\n",dpath);
#endif
		}
	
		  /* character special file */
		if(S_ISCHR(sinfo.st_mode)) {
			true_mknod(dpath,sinfo.st_mode|S_IFCHR,sinfo.st_rdev);
#if DEBUG
			debug(4,"\tfilled special char  : %s\n",dpath);
#endif
		}
		 
		  /* fifo special file */
		if(S_ISFIFO(sinfo.st_mode)) {
			true_mknod(dpath,sinfo.st_mode|S_IFIFO,0);
#if DEBUG
			debug(4,"\tfilled special fifo  : %s\n",dpath);
#endif
		}
		
		instw_delete(&iw_entry);
	}
	closedir(wdir);

	return rcod;
}

/*
 * procedure = / rc:=instw_makedirls(instw) /
 *
 * task      = /   eventually prepares a fake temporary directory used to 
 *               present 'overlaid' content to opendir(),readdir()... /
 *
 * note      = /
 *      --This procedure must be called after instw_setpath(). 
 *
 * 	--The "fake" temporary directories are created and...forgotten.
 *      If we need to reuse later the same directory, it is previously 
 *      erased, which ensures that it is correclty refreshed.
 * /
 *
 * returns   = /  0 ok. makedirls done 
 *               -1 failed. cf errno     /
 */
static int instw_makedirls(instw_t *instw) {
	int rcod=0;
	int status=0;
	struct stat translinfo;
	struct stat dirlsinfo;
	char wdirname[NAME_MAX+1];

#if DEBUG
	debug(2,"instw_makedirls(%p)\n",instw);
#endif

	  /* 
	   * if library incompletely initialized or if translation 
	   * is inactive, nothing to do 
	   */
	if( !(instw->gstatus&INSTW_INITIALIZED) ||
	    !(instw->gstatus&INSTW_OKTRANSL)) {
	    strcpy(instw->mdirlspath,instw->path);
	    return 0; 
	} 
	 
	  /* let's get the file translation status */
	if(instw_getstatus(instw,&status)) return -1;

	if( !(status&INSTW_TRANSLATED) ||
	    ((status&INSTW_TRANSLATED) && (status&INSTW_IDENTITY)) ) {
		strcpy(instw->mdirlspath,instw->path);
	} else {
		  /*   if it's a new directory, we open it in 
		   * the translated fs .
		   *   otherwise, it means that we will have to construct a
		   * merged directory.
		   */
		if(!(status & INSTW_ISINROOT)) {
			strcpy(instw->mdirlspath,instw->translpath);
		} else {
			rcod=true_stat(instw->translpath,&translinfo);

			sprintf(wdirname,"/%d_%lld_%lld",
					instw->pid,
					(long long int) translinfo.st_dev,
					(long long int) translinfo.st_ino);
		
			strcpy(instw->mdirlspath,instw->mdirls);
			strcat(instw->mdirlspath,wdirname);

			  /* we erase a previous identical dirls */
			if(!true_stat(instw->mdirlspath,&dirlsinfo)) {
				unlink_recursive(instw->mdirlspath);
			}
			true_mkdir(instw->mdirlspath,S_IRWXU);
	
			  /* we construct the merged directory here */
			instw_filldirls(instw);
		}
	}

#if DEBUG
	instw_print(instw);
#endif

	return rcod;
}

/* 
 *
 */
static int backup(const char *path) {
	char checkdir[BUFSIZ];
	char backup_path[BUFSIZ];
	char canonical[PATH_MAX+1];
	int	placeholder,
		i,
		blen;
	struct stat inode,backup_inode;
	struct utimbuf timbuf;

#if DEBUG
	debug(2,"========= backup () =========  path: %s\n", path); 
#endif

	  /* INSTW_OKBACKUP not set, we won't do any backups */
	if (!(__instw.gstatus&INSTW_OKBACKUP)) {
		#ifdef DEBUG
		debug(3,"Backup not enabled, path: %s\n", path);
		#endif
		return 0;
	}

	/* Mirror the file under the name its parent directories really have, */
	/* so a symlinked directory never gets recreated here as a real one.  */
	/* Everything below works on that name, exclusions included: a        */
	/* symlink leading into /tmp or into the backup area has to be caught */
	/* just the same.                                                     */
	path = resolve_parent(path,canonical,sizeof(canonical));

	/* Check if this is inside /dev */
	if (strstr (path, "/dev") == path) {
		#if DEBUG
		debug(3,"%s is inside /dev. Ignoring.\n", path);
		#endif
		return 0; 
	}

	/* Now check for /tmp */
	if (strstr (path, "/tmp") == path) {
		#if DEBUG
		debug(3,"%s is inside /tmp. Ignoring.\n", path);
		#endif
		return 0; 
	}

	/* Finally, the backup path itself */
	if (strstr (path,__instw.backup ) == path) {
		#if DEBUG
		debug(3,"%s is inside the backup path. Ignoring.\n", path);
		#endif
		return 0; 
	}

	/* Does it exist already? */
	#if DEBUG
	debug(3,"Exists %s?\n", path);
	#endif
	if (true_stat(path, &inode) < 0) {

		/* It doesn't exist, we'll tag it so we won't back it up  */
		/* if we run into it later                                */

		strcpy(backup_path,__instw.backup );
		strncat(backup_path, "/no-backup", 11);
		strcat(backup_path, path);
		make_path(backup_path);

		/* This one's just a placeholder */
		placeholder = true_creat(backup_path, S_IREAD);  
		if (!(placeholder < 0)) close (placeholder);

		#if DEBUG
		debug(3,"does not exist\n");
		#endif
		return 0;
	}


	/* Is this one tagged for no backup (i.e. it didn't previously exist)? */
	strcpy (backup_path,__instw.backup);
	strncat (backup_path, "/no-backup", 11);
	strcat (backup_path, path);

	if (true_stat (backup_path, &backup_inode) >= 0) {
		#if DEBUG
		debug(3,"%s should not be backed up\n", backup_path);
		#endif
		return 0;
	}


	#if DEBUG
	debug(3,"Exists in real path. Lets see what it is.\n");
	#endif

	/* Append the path to the backup_path */
	strcpy (backup_path,__instw.backup);
	strcat (backup_path, path);

	/* Create the directory tree for this file in the backup dir */
	make_path (backup_path);

	  /* let's backup the source file */
	if(copy_path(path,__instw.backup))
		return -1;

	  /* Check the owner and permission of the created directories */
 	i=0;
	blen = strlen (__instw.backup);
	while ( path[i] != '\0' ) {
		checkdir[i] = backup_path[blen+i] = path[i];
		if (checkdir[i] == '/') {  /* Each time a '/' is found, check if the    */
			checkdir[i+1] = '\0';   /* path exists. If it does, set it's perms.  */
			if (!true_stat (checkdir, &inode)) {
			backup_path[blen+i+1]='\0';
			timbuf.actime=inode.st_atime;
			timbuf.modtime=inode.st_mtime;
			true_utime(backup_path, &timbuf);
			true_chmod(backup_path, inode.st_mode);
			true_chown(backup_path, inode.st_uid, inode.st_gid);
			}
		}
		i++;
	}
	
	return 0;
}

time_t time (time_t *timer) {
	TIMECOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"time\n");
#endif

return true_time(timer);
}

/* 
 * *****************************************************************************
 */

int chdir(const char *pathname) {
	int result;
	instw_t instw;
	int status;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"chdir(%s)\n",pathname);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_chdir(pathname);
		return result;
	}    

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

	if(status&INSTW_TRANSLATED && !(status&INSTW_ISINROOT)) {
		result=true_chdir(instw.translpath);
		debug(3,"\teffective chdir(%s)\n",instw.translpath);
	} else {
		result=true_chdir(pathname);
		debug(3,"\teffective chdir(%s)\n",pathname);
	}

	instw_delete(&instw);

	return result;
}

int chmod(const char *path, mode_t mode) {
	int result;
	instw_t instw;

	REFCOUNT;
	
	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"chmod(%s,mode)\n",path);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_chmod(path,mode);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,path);

#if DEBUG
	instw_print(&instw);
#endif

	backup (instw.truepath);
	instw_apply(&instw);

	result = true_chmod(instw.translpath, mode);
	logg("%d\tchmod\t%s\t0%04o\t#%s\n",result,
	    instw.reslvpath,mode,error(result));

	instw_delete(&instw);

	return result;
}

int chown(const char *path, uid_t owner, gid_t group) {
	int result;
	instw_t instw;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"chown(%s,owner,group)\n",path);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_chown(path,owner,group);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,path);

#if DEBUG
	instw_print(&instw);
#endif

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_chown(instw.translpath,owner,group);
	logg("%d\tchown\t%s\t%d\t%d\t#%s\n",result,
	    instw.reslvpath,owner,group,error(result));

	instw_delete(&instw);

	return result;
}


int chown32(const char *path, uid_t owner, gid_t group) {

   return chown(path, owner, group);

}

int chroot(const char *path) {
	int result;
	char canonic[MAXPATHLEN];

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"chroot(%s)\n",path);
#endif

	canonicalize(path, canonic);
	result = true_chroot(path);
	  /*
	   * From now on, another log file will be written if 
	   * INSTW_LOGFILE is set                          
	   */
	logg("%d\tchroot\t%s\t#%s\n", result, canonic, error(result));
	return result;
}

#ifndef __USE_FILE_OFFSET64
int creat(const char *pathname, mode_t mode) {
/* Is it a system call? */
	int result;
	instw_t instw;
	
	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"creat(%s,mode)\n",pathname);
#endif
	
	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_creat(pathname,mode);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);

#if DEBUG
	instw_print(&instw);
#endif

	backup(instw.truepath);
	instw_apply(&instw);

	result = true_open(instw.translpath,O_CREAT|O_WRONLY|O_TRUNC,mode);
	logg("%d\tcreat\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}
#endif

int fchmod(int filedes, mode_t mode) {
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"fchmod\n");
#endif

	result = true_fchmod(filedes, mode);
	logg("%d\tfchmod\t%d\t0%04o\t#%s\n",result,filedes,mode,error(result));
	return result;
}

int fchown(int fd, uid_t owner, gid_t group) {
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"fchown\n");
#endif

	result = true_fchown(fd, owner, group);
	logg("%d\tfchown\t%d\t%d\t%d\t#%s\n",result,fd,owner,group,error(result));
	return result;
}

#ifndef __USE_FILE_OFFSET64
FILE *fopen(const char *pathname, const char *mode) {
	FILE *result = NULL;
	instw_t instw;
	int status=0;

	REFCOUNT;

	if (!libc_handle)
		initialize();

        result = NULL;

#if DEBUG
	debug(2,"fopen(%s,%s)\n",pathname,mode);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_fopen(pathname,mode);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	
#if DEBUG
	instw_print(&instw);
#endif

	if(mode[0]=='w'||mode[0]=='a'||mode[1]=='+') {
		backup(instw.truepath);
		instw_apply(&instw);
		logg("%" PRIdPTR "\tfopen\t%s\t#%s\n",(intptr_t)result,
		    instw.reslvpath,error(result));
	}

	instw_getstatus(&instw,&status);
	
	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective fopen(%s)\n",instw.translpath);
		result=true_fopen(instw.translpath,mode); 
	} else {
		debug(4,"\teffective fopen(%s)\n",instw.path);
		result=true_fopen(instw.path,mode);
	}
	
	if(mode[0]=='w'||mode[0]=='a'||mode[1]=='+') 
		logg("%" PRIdPTR "\tfopen\t%s\t#%s\n",(intptr_t)result,
		    instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

int ftruncate(int fd, TRUNCATE_T length) {
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"ftruncate\n");
#endif

	result = true_ftruncate(fd, length);
	logg("%d\tftruncate\t%d\t%d\t#%s\n",result,fd,(int)length,error(result));
	return result;
}
#endif

char *getcwd(char *buffer,size_t size) {
	char wpath[PATH_MAX+1];
	char *result;
	char *wptr;
	size_t wsize;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"getcwd(%p,%ld)\n",buffer,(long int)size);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_getcwd(buffer,size);
		return result;
	}

	if(	__instw.gstatus&INSTW_INITIALIZED &&
		__instw.gstatus&INSTW_OKTRANSL && 
		(NULL!=(result=true_getcwd(wpath,sizeof(wpath)))) ) {
		  /* we untranslate any translated path */
		if(strstr(wpath,__instw.transl)==wpath)	{
			wptr=wpath+strlen(__instw.transl);
			wsize=strlen(wptr)+1;
		} else {
			wptr=wpath;
			wsize=strlen(wptr)+1;
		}

		if (buffer == NULL) {
			if (size !=0 && size < wsize) {
				result=NULL;
				errno=(size<=0?EINVAL:ERANGE);
			} else {
				result=malloc(wsize);
				if(result == NULL) {
					errno=ENOMEM;
				} else {
					strcpy(result,wptr);
				}
			}
		} else {
			if(size>=wsize) {
				strcpy(buffer,wptr);
			} else {
				result=NULL;
				errno=(size<=0?EINVAL:ERANGE);
			}
		}
	} else  {
		result=true_getcwd(buffer,size);
	}

#if DEBUG
	debug(3,"\teffective getcwd(%s,%ld)\n",
	      (result?buffer:"(null)"),(long int)size);
#endif	

	return result;
}

int lchown(const char *path, uid_t owner, gid_t group) {
/* Linux specific? */
	int result;
	instw_t instw;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"lchown(%s,owner,group)\n",path);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_lchown(path,owner,group);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,path);
	
#if DEBUG
	instw_print(&instw);
#endif

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_lchown(instw.translpath,owner,group);
	logg("%d\tlchown\t%s\t%d\t%d\t#%s\n",result,
	    instw.reslvpath,owner,group,error(result));
	    
	instw_delete(&instw);

	return result;
}

int link(const char *oldpath, const char *newpath) {
	int result;
	instw_t instw_o;
	instw_t instw_n;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"link(%s,%s)\n",oldpath,newpath);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_link(oldpath,newpath);
		return result;
	}

	instw_new(&instw_o);
	instw_new(&instw_n);
	instw_setpath(&instw_o,oldpath);
	instw_setpath(&instw_n,newpath);

#if DEBUG
	instw_print(&instw_o);
	instw_print(&instw_n);
#endif

	backup(instw_o.truepath);
	instw_apply(&instw_o);
	instw_apply(&instw_n);
	
	result=true_link(instw_o.translpath,instw_n.translpath);
	logg("%d\tlink\t%s\t%s\t#%s\n",result,
	    instw_o.reslvpath,instw_n.reslvpath,error(result));
	    
	instw_delete(&instw_o);
	instw_delete(&instw_n);

	return result;
}

int mkdir(const char *pathname, mode_t mode) {
	char canonical[PATH_MAX+1];
	int result;
	instw_t instw;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkdir(%s,mode)\n",pathname);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_mkdir(pathname,mode);
		return result;
	}

	  /* An existing symlink to a directory is the directory it points */
	  /* at: creating "/lib" is creating "/usr/lib".                   */
	instw_new(&instw);
	instw_setpath(&instw,resolve_dir(pathname,canonical,sizeof(canonical)));

#if DEBUG
	instw_print(&instw);
#endif

	instw_apply(&instw);

	result=true_mkdir(instw.translpath,mode);
	logg("%d\tmkdir\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

#if (GLIBC_MINOR >= 33)
int mknod(const char *pathname, mode_t mode, dev_t dev) {
	int result;
	instw_t instw;
	
	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mknod(%s,mode,dev)\n",pathname);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_mknod(pathname,mode,dev);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);

#if DEBUG
	instw_print(&instw);
#endif

	instw_apply(&instw);
	backup(instw.truepath);

	result=true_mknod(instw.translpath,mode,dev);
	logg("%d\tmknod\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);
	
	return result;
}
#endif

int __xmknod(int version,const char *pathname, mode_t mode,dev_t *dev) {
	int result;
	instw_t instw;
	
	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mknod(%s,mode,dev)\n",pathname);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_xmknod(version,pathname,mode,dev);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);

#if DEBUG
	instw_print(&instw);
#endif

	instw_apply(&instw);
	backup(instw.truepath);

	result=true_xmknod(version,instw.translpath,mode,dev);
	logg("%d\tmknod\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);
	
	return result;
}

#ifndef __USE_FILE_OFFSET64
int open(const char *pathname, int flags, ...) {
/* Eventually, there is a third parameter: it's mode_t mode */
	va_list ap;
	mode_t mode;
	int result;
	instw_t instw;
	int status;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"open(%s,%d,mode)\n",pathname,flags);
#endif

	va_start(ap, flags);
	mode = va_arg(ap, int /*promoted from mode_t*/);
	va_end(ap);

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_open(pathname,flags,mode);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	
#if DEBUG
	instw_print(&instw);
#endif

	if(flags & (O_WRONLY | O_RDWR)) {
		backup(instw.truepath);
		instw_apply(&instw);
	}

	instw_getstatus(&instw,&status);

	if(status&INSTW_TRANSLATED) 
		result=true_open(instw.translpath,flags,mode);
	else
		result=true_open(instw.path,flags,mode);
	
	if(flags & (O_WRONLY | O_RDWR)) 
		logg("%d\topen\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}
#endif

/*
 *
 */
DIR *opendir(const char *dirname) {
	DIR *result;
	instw_t instw;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"opendir(%s)\n",dirname);
#endif
	
	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_opendir(dirname);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,dirname);
	instw_makedirls(&instw);

#if DEBUG
	instw_print(&instw);
#endif

	result=true_opendir(instw.mdirlspath);

	instw_delete(&instw);

	return result;
}

#ifndef __USE_FILE_OFFSET64
struct dirent *readdir(DIR *dir) {
	struct dirent *result;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(3,"readdir(%p)\n",dir);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_readdir(dir);
		return result;
	}

	result=true_readdir(dir);

#if DEBUG
	__instw_printdirent(result);
#endif

	return result;
}

ssize_t readlink(const char *path,char *buf,size_t bufsiz) {
	ssize_t result;
	instw_t instw;
	int status;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"readlink(\"%s\",%p,%ld)\n",path,buf,(long int)bufsiz);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_readlink(path,buf,bufsiz);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,path);
	instw_getstatus(&instw,&status);
	
#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED)
		result=true_readlink(instw.translpath,buf,bufsiz);
	else
		result=true_readlink(instw.path,buf,bufsiz);

	instw_delete(&instw);

	return result;	
}
#endif

char *realpath(const char *file_name,char *resolved_name) {
	char *result;

	if (!libc_handle)
		initialize();

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_realpath(file_name,resolved_name);
		return result;
	}

	result=true_realpath(file_name,resolved_name);

	return result;
}

int rename(const char *oldpath, const char *newpath) {
	int result;
	instw_t oldinstw;
	instw_t newinstw;

	REFCOUNT;
	
	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"rename(\"%s\",\"%s\")\n",oldpath,newpath);	
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_rename(oldpath,newpath);
		return result;
	}

	instw_new(&oldinstw);
	instw_new(&newinstw);
	instw_setpath(&oldinstw,oldpath);
	instw_setpath(&newinstw,newpath);

#if DEBUG
	instw_print(&oldinstw);
	instw_print(&newinstw);
#endif

	backup(oldinstw.truepath);
	instw_apply(&oldinstw);
	instw_apply(&newinstw);

	result=true_rename(oldinstw.translpath,newinstw.translpath);
	logg("%d\trename\t%s\t%s\t#%s\n",result,
	    oldinstw.reslvpath,newinstw.reslvpath,error(result));

	instw_delete(&oldinstw);
	instw_delete(&newinstw);

	return result;
}

int rmdir(const char *pathname) {
	int result;
	instw_t instw;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"rmdir(%s)\n",pathname);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_rmdir(pathname);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_rmdir(instw.translpath);
	logg("%d\trmdir\t%s\t#%s\n",result,instw.reslvpath,error(result));
	
	instw_delete(&instw);
	
	return result;
}

#ifndef __USE_FILE_OFFSET64
int scandir(	const char *dir,struct dirent ***namelist,
		int (*select)(const struct dirent *),
#if (GLIBC_MINOR >= 10)
		int (*compar)(const struct dirent **,const struct dirent **)	) {
#else
		int (*compar)(const void *,const void *)	) {
#endif
	int result;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"scandir(%s,%p,%p,%p)\n",dir,namelist,select,compar);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_scandir(dir,namelist,select,compar);
		return result;
	}

	result=true_scandir(dir,namelist,select,compar);

	return result;
}		
#endif

#if (GLIBC_MINOR >= 33)
#ifndef __USE_FILE_OFFSET64
int stat(const char *pathname, struct stat *info) {
	int result;
	instw_t instw;
	int status;

	if(!libc_handle)
		initialize();
#if DEBUG
	debug(2, "stat(%s, %p)\n", pathname, info);
#endif
	/* We were asked to work in "real" mode */
	if(!(__instw.gstatus & INSTW_INITIALIZED) || 
	   !(__instw.gstatus & INSTW_OKWRAP)) {
		result = true_stat(pathname, info);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw, pathname);
	instw_getstatus(&instw, &status);

#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective stat(%s,%p)\n",
		      instw.translpath,info);
		result=true_stat(instw.translpath,info);
	} else {
		debug(4,"\teffective stat(%s,%p)\n",
		      instw.path,info);
		result=true_stat(instw.path,info);
	}

	instw_delete(&instw);

	return result;	
}
#endif

#endif

int __xstat(int version,const char *pathname,struct stat *info) {
	int result;
	instw_t instw;
	int status;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"stat(%s,%p)\n",pathname,info);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_xstat(version,pathname,info);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective stat(%s,%p)\n",
		      instw.translpath,info);
		result=true_xstat(version,instw.translpath,info);
	} else {
		debug(4,"\teffective stat(%s,%p)\n",
		      instw.path,info);
		result=true_xstat(version,instw.path,info);
	}

	instw_delete(&instw);

	return result;	
}

#if (GLIBC_MINOR >= 33)
#ifndef __USE_FILE_OFFSET64
int lstat(const char *pathname,struct stat *info) {
	int result;
	instw_t instw;
	int status;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"lstat(%s,%p)\n",pathname,info);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_lstat(pathname,info);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective lstat(%s,%p)\n",
			instw.translpath,info);
		result=true_lstat(instw.translpath,info);
	} else {
		debug(4,"\teffective lstat(%s,%p)\n",
			instw.path,info);
		result=true_lstat(instw.path,info);
	}

	instw_delete(&instw);

	return result;	
}
#endif
#endif

int __lxstat(int version,const char *pathname,struct stat *info) {
	int result;
	instw_t instw;
	int status;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"lstat(%s,%p)\n",pathname,info);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_lxstat(version,pathname,info);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective lstat(%s,%p)\n",
			instw.translpath,info);
		result=true_lxstat(version,instw.translpath,info);
	} else {
		debug(4,"\teffective lstat(%s,%p)\n",
			instw.path,info);
		result=true_lxstat(version,instw.path,info);
	}

	instw_delete(&instw);

	return result;	
}

int symlink(const char *pathname, const char *slink) {
	int result;
	instw_t instw;
	instw_t instw_slink;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"symlink(%s,%s)\n",pathname,slink);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_symlink(pathname,slink);
		return result;
	}

	instw_new(&instw);
	instw_new(&instw_slink);
	instw_setpath(&instw,pathname);
	instw_setpath(&instw_slink,slink);

#if DEBUG
	instw_print(&instw_slink);
#endif

	backup(instw_slink.truepath);
	instw_apply(&instw_slink);
	
	result=true_symlink(pathname,instw_slink.translpath);
	logg("%d\tsymlink\t%s\t%s\t#%s\n",
           result,instw.path,instw_slink.reslvpath,error(result));

	    
	instw_delete(&instw);
	instw_delete(&instw_slink);

	return result;
}

#ifndef __USE_FILE_OFFSET64
int truncate(const char *path, TRUNCATE_T length) {
	int result;
	instw_t instw;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"truncate(%s,length)\n",path);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_truncate(path,length);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,path);

#if DEBUG
	instw_print(&instw);
#endif

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_truncate(instw.translpath,length);
	logg("%d\ttruncate\t%s\t%d\t#%s\n",result,
	    instw.reslvpath,(int)length,error(result));

	instw_delete(&instw);

	return result;
}
#endif

int unlink(const char *pathname) {
	int result;
	instw_t instw;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"unlink(%s)\n",pathname);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_unlink(pathname);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);

#if DEBUG
	instw_print(&instw);
#endif

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_unlink(instw.translpath);
	logg("%d\tunlink\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

int utime (const char *pathname, const struct utimbuf *newtimes) {
	int result;
	instw_t instw;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"utime(%s,newtimes)\n",pathname);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_utime(pathname,newtimes);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);

#if DEBUG
	instw_print(&instw);
#endif

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_utime(instw.translpath,newtimes);
	logg("%d\tutime\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

int utimes (const char *pathname, const struct timeval *newtimes) {
       int result;
       instw_t instw;

       if (!libc_handle)
               initialize();

#if DEBUG
       debug(2,"utimes(%s,newtimes)\n",pathname);
#endif

         /* We were asked to work in "real" mode */
       if( !(__instw.gstatus & INSTW_INITIALIZED) ||
           !(__instw.gstatus & INSTW_OKWRAP) ) {
               result=true_utimes(pathname,newtimes);
               return result;
       }

       instw_new(&instw);
       instw_setpath(&instw,pathname);

#if DEBUG
       instw_print(&instw);
#endif

       backup(instw.truepath);
       instw_apply(&instw);

       result=true_utimes(instw.translpath,newtimes);
       logg("%d\tutimes\t%s\t#%s\n",result,instw.reslvpath,error(result));

       instw_delete(&instw);

       return result;
}

int utimensat (int dirfd, const char *pathname,
               const struct timespec times[2], int flags) {
       int result;
       instw_t instw;

       if (!libc_handle)
               initialize();

#if DEBUG
       debug(2,"utimensat(%d,%s,times,0%o)\n",dirfd,pathname,flags);
#endif

         /* We were asked to work in "real" mode */
       if( !(__instw.gstatus & INSTW_INITIALIZED) ||
           !(__instw.gstatus & INSTW_OKWRAP) )
               return true_utimensat(dirfd,pathname,times,flags);

         /* A null path means the descriptor itself, which needs no */
         /* translating: it already refers to the translated file.  */
       if(pathname==NULL)
               return true_utimensat(dirfd,pathname,times,flags);

       instw_new(&instw);
       instw_setpathrel(&instw,dirfd,pathname);

#if DEBUG
       instw_print(&instw);
#endif

       backup(instw.truepath);
       instw_apply(&instw);

         /* instw resolved an absolute path, so the descriptor is spent */
       result=true_utimensat(AT_FDCWD,instw.translpath,times,flags);
       logg("%d\tutimensat\t%s\t#%s\n",result,instw.reslvpath,error(result));

       instw_delete(&instw);

       return result;
}

#ifdef WRAP_TIME64_ENTRY_POINTS

int __stat64_time64(const char *pathname, void *info) {
	int result;
	instw_t instw;
	int status;

	if(!libc_handle)
		initialize();

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_stat64_time64(pathname,info);

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

	if(status&INSTW_TRANSLATED)
		result=true_stat64_time64(instw.translpath,info);
	else
		result=true_stat64_time64(instw.path,info);

	instw_delete(&instw);

	return result;
}

int __lstat64_time64(const char *pathname, void *info) {
	int result;
	instw_t instw;
	int status;

	if(!libc_handle)
		initialize();

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_lstat64_time64(pathname,info);

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

	if(status&INSTW_TRANSLATED)
		result=true_lstat64_time64(instw.translpath,info);
	else
		result=true_lstat64_time64(instw.path,info);

	instw_delete(&instw);

	return result;
}

int __fstatat64_time64(int dirfd, const char *path, void *s, int flags) {
	int result;
	instw_t instw;

	if(!libc_handle)
		initialize();

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		if(flags & AT_SYMLINK_NOFOLLOW)
			return true_lstat64_time64(path,s);
		return true_stat64_time64(path,s);
	}

	instw_new(&instw);
	instw_setpathrel(&instw,dirfd,path);

	  /* instw resolved an absolute path, so the descriptor is spent */
	if(flags & AT_SYMLINK_NOFOLLOW)
		result=__lstat64_time64(instw.path,s);
	else
		result=__stat64_time64(instw.path,s);

	instw_delete(&instw);

	return result;
}

int __utime64(const char *pathname, const void *newtimes) {
	int result;
	instw_t instw;

	if(!libc_handle)
		initialize();

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_utime64(pathname,newtimes);

	instw_new(&instw);
	instw_setpath(&instw,pathname);

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_utime64(instw.translpath,newtimes);
	logg("%d\tutime\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

int __utimes64(const char *pathname, const void *newtimes) {
	int result;
	instw_t instw;

	if(!libc_handle)
		initialize();

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_utimes64(pathname,newtimes);

	instw_new(&instw);
	instw_setpath(&instw,pathname);

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_utimes64(instw.translpath,newtimes);
	logg("%d\tutimes\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

int __utimensat64(int dirfd, const char *pathname, const void *times, int flags) {
	int result;
	instw_t instw;

	if(!libc_handle)
		initialize();

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_utimensat64(dirfd,pathname,times,flags);

	if(pathname==NULL)
		return true_utimensat64(dirfd,pathname,times,flags);

	instw_new(&instw);
	instw_setpathrel(&instw,dirfd,pathname);

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_utimensat64(AT_FDCWD,instw.translpath,times,flags);
	logg("%d\tutimensat\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

#endif

int access (const char *pathname, int type) {
       int result;
       instw_t instw;

       if (!libc_handle)
               initialize();

#if DEBUG
       debug(2,"access(%s,%d)\n",pathname,type);
#endif

         /* We were asked to work in "real" mode */
       if( !(__instw.gstatus & INSTW_INITIALIZED) ||
           !(__instw.gstatus & INSTW_OKWRAP) ) {
               result=true_access(pathname,type);
               return result;
       }

       instw_new(&instw);
       instw_setpath(&instw,pathname);

#if DEBUG
       instw_print(&instw);
#endif

         /* access() cannot modify anything, so there is nothing to save */
       instw_apply(&instw);

       result=true_access(instw.translpath,type);
       logg("%d\taccess\t%s\t#%s\n",result,instw.reslvpath,error(result));

       instw_delete(&instw);

       return result;
}

int setxattr (const char *pathname, const char *name,
              const void *value, size_t size, int flags)
{
        int result;
        instw_t instw;

        REFCOUNT;

        if (!libc_handle)
               initialize();

#if DEBUG
        debug(2,"setxattr(%s,%s)\n",pathname,name);
#endif

         /* We were asked to work in "real" mode */
        if( !(__instw.gstatus & INSTW_INITIALIZED) ||
           !(__instw.gstatus & INSTW_OKWRAP) ) {
               result=true_setxattr(pathname,name,
                       value,size,flags);
               return result;
        }

        instw_new(&instw);
        instw_setpath(&instw,pathname);

#if DEBUG
        instw_print(&instw);
#endif

        backup(instw.truepath);
        instw_apply(&instw);

        result=true_setxattr(instw.translpath,name,value,size,flags);
        logg("%d\tsetxattr\t%s\t#%s\n",result,instw.reslvpath,error(result));

        instw_delete(&instw);

        return result;
}

int removexattr (const char *pathname, const char *name)
{
        int result;
        instw_t instw;

        REFCOUNT;

        if (!libc_handle)
               initialize();

#if DEBUG
        debug(2,"removexattr(%s,%s)\n",pathname,name);
#endif

         /* We were asked to work in "real" mode */
        if( !(__instw.gstatus & INSTW_INITIALIZED) ||
            !(__instw.gstatus & INSTW_OKWRAP) ) {
                result=true_removexattr(pathname,name);
                return result;
        }

        instw_new(&instw);
        instw_setpath(&instw,pathname);

#if DEBUG
        instw_print(&instw);
#endif

        backup(instw.truepath);
        instw_apply(&instw);

        result=true_removexattr(instw.translpath,name);
        logg("%d\tremovexattr\t%s\t#%s\n",result,instw.reslvpath,error(result));

        instw_delete(&instw);

        return result;
}


int creat64(const char *pathname, __mode_t mode) {
/* Is it a system call? */
	int result;
	instw_t instw;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"creat64(%s,mode)\n",pathname);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_creat64(pathname,mode);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);

#if DEBUG
	instw_print(&instw);
#endif

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_open64(instw.translpath,O_CREAT | O_WRONLY | O_TRUNC, mode);
	logg("%d\tcreat\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

int ftruncate64(int fd, __off64_t length) {
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"ftruncate64\n");
#endif

	result = true_ftruncate64(fd, length);
	logg("%d\tftruncate\t%d\t%d\t#%s\n",result,fd,(int)length,error(result));
	return result;
}

FILE *fopen64(const char *pathname, const char *mode) {
	FILE *result;
	instw_t instw;
	int status;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"fopen64(%s,%s)\n",pathname,mode);
#endif
	
	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_fopen64(pathname,mode);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	
#if DEBUG
	instw_print(&instw);
#endif

	if(mode[0]=='w'||mode[0]=='a'||mode[1]=='+') {
		backup(instw.truepath);
		instw_apply(&instw);
	}

	instw_getstatus(&instw,&status);
	
	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective fopen64(%s)\n",instw.translpath);
		result=true_fopen64(instw.translpath,mode); 
	} else {
		debug(4,"\teffective fopen64(%s)\n",instw.path);
		result=true_fopen64(instw.path,mode);
	}

	if(mode[0]=='w'||mode[0]=='a'||mode[1]=='+') 
		logg("%" PRIdPTR "\tfopen64\t%s\t#%s\n",(intptr_t)result,
		    instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

int open64(const char *pathname, int flags, ...) {
/* Eventually, there is a third parameter: it's mode_t mode */
	va_list ap;
	mode_t mode;
	int result;
	instw_t instw;
	int status;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"open64(%s,%d,mode)\n",pathname,flags);
#endif

	va_start(ap, flags);
	mode = va_arg(ap, int /*promoted from mode_t*/);
	va_end(ap);

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_open64(pathname,flags,mode);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);

#if DEBUG
	instw_print(&instw);
#endif

	if(flags & (O_WRONLY | O_RDWR)) {
		backup(instw.truepath);
		instw_apply(&instw);
	}

	instw_getstatus(&instw,&status);

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective open64(%s)\n",instw.translpath);
		result=true_open64(instw.translpath,flags,mode);
	} else {
		debug(4,"\teffective open64(%s)\n",instw.path);
		result=true_open64(instw.path,flags,mode);
	}
	
	if(flags & (O_WRONLY | O_RDWR)) 
		logg("%d\topen\t%s\t#%s\n",result,
		    instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

struct dirent64 *readdir64(DIR *dir) {
	struct dirent64 *result;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(3,"readdir64(%p)\n",dir);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_readdir64(dir);
		return result;
	}

	result=true_readdir64(dir);

#if DEBUG
	__instw_printdirent64(result);
#endif

	return result;
}

int scandir64(	const char *dir,struct dirent64 ***namelist,
		int (*select)(const struct dirent64 *),
#if (GLIBC_MINOR >= 10)
		int (*compar)(const struct dirent64 **,const struct dirent64 **)	) {
#else
		int (*compar)(const void *,const void *)	) {
#endif
	int result;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"scandir64(%s,%p,%p,%p)\n",dir,namelist,select,compar);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_scandir64(dir,namelist,select,compar);
		return result;
	}

	result=true_scandir64(dir,namelist,select,compar);

	return result;
}		

#if (GLIBC_MINOR >= 33)
int stat64(const char *pathname,struct stat64 *info) {
	int result;
	instw_t instw;
	int status;

  if (!libc_handle)
 		initialize();

#if DEBUG
	debug(2,"stat64(%s,%p)\n",pathname,info);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_stat64(pathname,info);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

       if (!libc_handle)
           initialize();

#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective stat64(%s,%p)\n",
		      instw.translpath,info);
		result=true_stat64(instw.translpath,info);
	} else {
		debug(4,"\teffective stat64(%s,%p)\n",
		      instw.path,info);
		result=true_stat64(instw.path,info);
	}	

	instw_delete(&instw);

	return result;	
}
#endif

int __xstat64(int version,const char *pathname,struct stat64 *info) {
	int result;
	instw_t instw;
	int status;

  if (!libc_handle)
 		initialize();

#if DEBUG
	debug(2,"stat64(%s,%p)\n",pathname,info);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_xstat64(version,pathname,info);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

       if (!libc_handle)
           initialize();

#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective stat64(%s,%p)\n",
		      instw.translpath,info);
		result=true_xstat64(version,instw.translpath,info);
	} else {
		debug(4,"\teffective stat64(%s,%p)\n",
		      instw.path,info);
		result=true_xstat64(version,instw.path,info);
	}	

	instw_delete(&instw);

	return result;	
}

#if (GLIBC_MINOR >= 33)
int lstat64(const char *pathname,struct stat64 *info) {
	int result;
	instw_t instw;
	int status;

	if(!libc_handle)
		initialize();

#if DEBUG
	debug(2,"lstat64(%s,%p)\n",pathname,info);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_lstat64(pathname,info);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective lstat64(%s,%p)\n",
			instw.translpath,info);
		result=true_lstat64(instw.translpath,info);
	} else {
		debug(4,"\teffective lstat64(%s,%p)\n",
			instw.path,info);
		result=true_lstat64(instw.path,info);
	}

	instw_delete(&instw);

	return result;	
}
#endif

int __lxstat64(int version,const char *pathname,struct stat64 *info) {
	int result;
	instw_t instw;
	int status;

	if(!libc_handle)
		initialize();

#if DEBUG
	debug(2,"lstat64(%s,%p)\n",pathname,info);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_lxstat64(version,pathname,info);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,pathname);
	instw_getstatus(&instw,&status);

#if DEBUG
	instw_print(&instw);
#endif

	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective lstat64(%s,%p)\n",
			instw.translpath,info);
		result=true_lxstat64(version,instw.translpath,info);
	} else {
		debug(4,"\teffective lstat64(%s,%p)\n",
			instw.path,info);
		result=true_lxstat64(version,instw.path,info);
	}

	instw_delete(&instw);

	return result;	
}

int truncate64(const char *path, __off64_t length) {
	int result;
	instw_t instw;

	if (!libc_handle)
		initialize();

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"truncate64(%s,length)\n",path);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) ) {
		result=true_truncate64(path,length);
		return result;
	}

	instw_new(&instw);
	instw_setpath(&instw,path);

#if DEBUG
	instw_print(&instw);
#endif

	backup(instw.truepath);
	instw_apply(&instw);

	result=true_truncate64(instw.translpath,length);
	
	logg("%d\ttruncate\t%s\t%d\t#%s\n",result,
	    instw.reslvpath,(int)length,error(result));

	instw_delete(&instw);

	return result;
}



/***********************************************
 * openat() and its relatives are defined here
 *
 * They are mostly wrappers for the already
 * defined "non-at" functions defined above.
 *
 * They transform the path relative to the
 * fd into an absolute path and then call
 * the normal functions. The transformation
 * is done by calling instw_setpathrel().
 *
 * Maybe we could wrap all of these into a
 * single generic wrap-any function?
 *
 * Thanks to Gilbert Ashley for his work on this!
 */

 
#ifndef __USE_FILE_OFFSET64
int openat (int dirfd, const char *path, int flags, ...) {
 	mode_t mode = 0;
 	va_list arg;
 	if(flags & O_CREAT) {
 		va_start(arg, flags);
 		mode = va_arg(arg, int /*promoted from mode_t*/);
 		va_end (arg);
 	}
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
                 return open(path, flags, mode);
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
 	debug(2, "openat(%d, %s, 0x%x, 0%o)\n", dirfd, path, flags, mode);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
 		return true_open(path,flags,mode);
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 	
 	result=open(instw.path,flags,mode);
 	
 	instw_delete(&instw);
 
	return result;
}
#endif

  /* Programs built for large files call this one, and on a 32-bit system
   * that is most of them. Left unwrapped, their writes went straight to
   * the real filesystem. */

int openat64 (int dirfd, const char *path, int flags, ...) {
	mode_t mode = 0;
	va_list arg;

	if(flags & O_CREAT) {
		va_start(arg, flags);
		mode = va_arg(arg, int /*promoted from mode_t*/);
		va_end (arg);
	}

	return openat(dirfd, path, flags, mode);
}


int fchmodat (int dirfd, const char *path, mode_t mode, int flag) {
 	
 	int result;
 	instw_t instw;
 
 	  /* Without AT_SYMLINK_NOFOLLOW this is chmod() on a path, and the
 	   * chmod() wrapper below does the translating. The flag has to be
 	   * carried when it is set, because chmod() follows the link and the
 	   * mode then lands on the target: a dangling link fails with ENOENT,
 	   * and a live one has the link's permissions written over its own.
 	   * tar sets the flag for every symlink it extracts. */
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(!(flag & AT_SYMLINK_NOFOLLOW) && (dirfd == AT_FDCWD || *path == '/'))
		{
		 #if DEBUG
			debug(2, "fchmodat(%d,%s,0%o)\n", dirfd, path, mode);
		 #endif
		 return chmod(path, mode);
		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
 	debug(2, "fchmodat(%d,%s,0%o)\n", dirfd, path, mode);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
 		return true_fchmodat(dirfd,path,mode,flag);
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 	
	if(flag & AT_SYMLINK_NOFOLLOW) {
		  /* Nothing is backed up here: this changes the link rather
		   * than the file it names. */
		instw_apply(&instw);
		result=true_fchmodat(AT_FDCWD,instw.translpath,mode,flag);
		logg("%d\tfchmodat\t%s\t0%04o\t#%s\n",result,
		    instw.reslvpath,mode,error(result));
	} else {
		result=chmod(instw.path,mode);
	}
 	
 	instw_delete(&instw);
 
	return result;
}

int fchownat (int dirfd, const char *path,uid_t owner,gid_t group,int flags) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "fchownat(%d,%s,%d,%d,0%o)\n", dirfd, path, owner, group, flags);
		 #endif

		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lchwon() behaviour, according to fchownat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return lchown(path, owner, group); 
		 }
		 else {
		    return chown(path, owner, group);
		 }

		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2,"fchownat(%d,%s,%d,%d,0%o)\n", dirfd, path, owner, group, flags);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP)) {

		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lchwon() behaviour, according to fchownat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return true_lchown(path, owner, group); 
		 }
		 else {
		    return true_chown(path, owner, group);
		 }
 	}
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
	 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
	 /* lchwon() behaviour, according to fchownat(2) */

	 if ( flags & AT_SYMLINK_NOFOLLOW ) {
	    result=lchown(instw.path, owner, group); 
	 }
	 else {
 	    result=chown(instw.path, owner, group);
	 }


 	
 	instw_delete(&instw);
 
	return result;
}

#if (GLIBC_MINOR >= 33)
#ifndef __USE_FILE_OFFSET64
int fstatat (int dirfd, const char *path, struct stat *s, int flags) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "fstatat(%d,%s,%p,0%o)\n", dirfd, path, s, flags);
		 #endif

		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return lstat(path, s); 
		 }
		 else {
		    return stat(path, s);
		 }

		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "fstatat(%d,%s,%p,0%o)\n", dirfd, path, s, flags);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP)) {


		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return true_lstat(path, s); 
		 }
		 else {
 		    return true_stat(path, s);
		 }
	}

	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 
		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
 		    result=lstat(instw.path, s);
		 }
		 else {
 		    result=stat(instw.path, s);
 		    
		 }	
 	
 	instw_delete(&instw);
 
	return result;
}
#endif
#endif

int __fxstatat (int version, int dirfd, const char *path, struct stat *s, int flags) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "__fxstatat(%d,%s,%p,0%o)\n", dirfd, path, s, flags);
		 #endif

		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return __lxstat(version, path, s); 
		 }
		 else {
		    return __xstat(version, path, s);
		 }

		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "__fxstatat(%d,%s,%p,0%o)\n", dirfd, path, s, flags);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP)) {


		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return true_lxstat(version, path, s); 
		 }
		 else {
 		    return true_xstat(version, path, s);
		 }
	}

	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 
		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
 		    result=__lxstat(version, instw.path, s);
		 }
		 else {
 		    result=__xstat(version, instw.path, s);
 		    
		 }	
 	
 	instw_delete(&instw);
 
	return result;
}

#if (GLIBC_MINOR >= 33)
int fstatat64 (int dirfd, const char *path, struct stat64 *s, int flags) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "fstatat(%d,%s,%p,0%o)\n", dirfd, path, s, flags);
		 #endif

		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return lstat64(path, s); 
		 }
		 else {
		    return stat64(path, s);
		 }

		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "fstatat(%d,%s,%p,0%o)\n", dirfd, path, s, flags);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP)) {

		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return true_lstat64(path, s); 
		 }
		 else {
 		    return true_stat64(path, s);
		 }
	}

	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif

	/* If we have AT_SYMLINK_NOFOLLOW then we need  */
	/* lstat() behaviour, according to fstatat(2) */

	if ( flags & AT_SYMLINK_NOFOLLOW ) {
 	   result=lstat64(instw.path, s);
	}
	else {
 	   result=stat64(instw.path, s);
	}
 	
 	instw_delete(&instw);
 
	return result;

}
#endif

int __fxstatat64 (int version, int dirfd, const char *path, struct stat64 *s, int flags) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "__fxstatat(%d,%s,%p,0%o)\n", dirfd, path, s, flags);
		 #endif

		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return __lxstat64(version, path, s); 
		 }
		 else {
		    return __xstat64(version, path, s);
		 }

		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "__fxstatat(%d,%s,%p,0%o)\n", dirfd, path, s, flags);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP)) {

		 /* If we have AT_SYMLINK_NOFOLLOW then we need  */
		 /* lstat() behaviour, according to fstatat(2) */

		 if ( flags & AT_SYMLINK_NOFOLLOW ) {
		    return true_lxstat64(version, path, s); 
		 }
		 else {
 		    return true_xstat64(version, path, s);
		 }
	}

	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif

	/* If we have AT_SYMLINK_NOFOLLOW then we need  */
	/* lstat() behaviour, according to fstatat(2) */

	if ( flags & AT_SYMLINK_NOFOLLOW ) {
 	   result=__lxstat64(version, instw.path, s);
	}
	else {
 	   result=__xstat64(version, instw.path, s);
	}
 	
 	instw_delete(&instw);
 
	return result;

}


int linkat (int olddirfd, const char *oldpath,
                  int newdirfd, const char *newpath, int flags) {

 	int result;
 	int unnamed_source;
 	instw_t instwold;
 	instw_t instwnew;

 	REFCOUNT;

 	if (!libc_handle)
 		initialize();

#if DEBUG
	debug(2, "linkat(%d, %s, %d, %s, 0%o)\n", olddirfd, oldpath, newdirfd, newpath, flags );
#endif

 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
		return true_linkat(olddirfd, oldpath, newdirfd, newpath, flags);

	  /* An empty oldpath with AT_EMPTY_PATH names olddirfd itself, which */
	  /* may be an O_TMPFILE with no path to translate.                   */
	unnamed_source=(flags&AT_EMPTY_PATH) && oldpath[0]=='\0';

 	instw_new(&instwold);
 	instw_new(&instwnew);
	if(!unnamed_source) instw_setpathrel(&instwold,olddirfd,oldpath);
 	instw_setpathrel(&instwnew,newdirfd,newpath);

#if DEBUG
 	instw_print(&instwold);
 	instw_print(&instwnew);
#endif

	if(!unnamed_source) {
		backup(instwold.truepath);
		instw_apply(&instwold);
	}
	instw_apply(&instwnew);

	  /* instw resolved the named ends to absolute paths, so those      */
	  /* descriptors are spent and the files to link are the translated */
	  /* ones.                                                          */
	if(unnamed_source)
		result=true_linkat(olddirfd, oldpath,
		                   AT_FDCWD, instwnew.translpath, flags);
	else
		result=true_linkat(AT_FDCWD, instwold.translpath,
		                   AT_FDCWD, instwnew.translpath, flags);
	logg("%d\tlinkat\t%s\t%s\t#%s\n",result,
	    instwold.reslvpath,instwnew.reslvpath,error(result));

 	instw_delete(&instwold);
 	instw_delete(&instwnew);

	return result;
}


int mkdirat (int dirfd, const char *path, mode_t mode) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "mkdirat(%d,%s,0%o)\n", dirfd, path, mode);
		 #endif
		 return mkdir(path, mode);
		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "mkdirat(%d,%s,0%o)\n", dirfd, path, mode);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
 		return true_mkdir(path,mode);
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 	
 	result=mkdir(instw.path,mode);
 	
 	instw_delete(&instw);
 
	return result;
}


/* Translate the template before passing it to glibc. */

static int temp_prepare(instw_t *instw,const char *template,
                        char *translated,size_t translsz) {
	int status;

	instw_new(instw);
	instw_setpath(instw,template);
	instw_apply(instw);
	instw_getstatus(instw,&status);

	if(!(status&INSTW_TRANSLATED)) return 0;
	if(strlen(instw->translpath)>=translsz) return 0;

	strcpy(translated,instw->translpath);

	return 1;
}

/* Copy the six generated characters back to the caller's template. Only  */
/* the X's differ between the two names, and mkstemps() keeps suffixlen   */
/* characters after them, so count from the end.                          */

static void temp_commit(char *template,const char *created,int suffixlen) {
	size_t tlen,clen;

	tlen=strlen(template);
	clen=strlen(created);

	if(tlen<(size_t)(6+suffixlen) || clen<(size_t)(6+suffixlen)) return;

	memcpy(template+tlen-6-suffixlen,created+clen-6-suffixlen,6);
}

#ifndef __USE_FILE_OFFSET64
int mkstemp(char *template) {
	char translated[PATH_MAX+1];
	instw_t instw;
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkstemp(%s)\n",template);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkstemp(template);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkstemp(template);
	}

	result=true_mkstemp(translated);
	if(result>=0) {
		temp_commit(template,translated,0);
		  /* re-read the path: it names a real file now */
		instw_setpath(&instw,template);
	}
	logg("%d\tmkstemp\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}
#endif

int mkstemp64(char *template) {
	char translated[PATH_MAX+1];
	instw_t instw;
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkstemp64(%s)\n",template);
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkstemp64(template);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkstemp64(template);
	}

	result=true_mkstemp64(translated);
	if(result>=0) {
		temp_commit(template,translated,0);
		instw_setpath(&instw,template);
	}
	logg("%d\tmkstemp\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

#ifndef __USE_FILE_OFFSET64
int mkostemp(char *template,int flags) {
	char translated[PATH_MAX+1];
	instw_t instw;
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkostemp(%s,0%o)\n",template,flags);
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkostemp(template,flags);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkostemp(template,flags);
	}

	result=true_mkostemp(translated,flags);
	if(result>=0) {
		temp_commit(template,translated,0);
		instw_setpath(&instw,template);
	}
	logg("%d\tmkstemp\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}
#endif

int mkostemp64(char *template,int flags) {
	char translated[PATH_MAX+1];
	instw_t instw;
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkostemp64(%s,0%o)\n",template,flags);
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkostemp64(template,flags);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkostemp64(template,flags);
	}

	result=true_mkostemp64(translated,flags);
	if(result>=0) {
		temp_commit(template,translated,0);
		instw_setpath(&instw,template);
	}
	logg("%d\tmkstemp\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

#ifndef __USE_FILE_OFFSET64
int mkstemps(char *template,int suffixlen) {
	char translated[PATH_MAX+1];
	instw_t instw;
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkstemps(%s,%d)\n",template,suffixlen);
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkstemps(template,suffixlen);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkstemps(template,suffixlen);
	}

	result=true_mkstemps(translated,suffixlen);
	if(result>=0) {
		temp_commit(template,translated,suffixlen);
		instw_setpath(&instw,template);
	}
	logg("%d\tmkstemp\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}
#endif

int mkstemps64(char *template,int suffixlen) {
	char translated[PATH_MAX+1];
	instw_t instw;
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkstemps64(%s,%d)\n",template,suffixlen);
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkstemps64(template,suffixlen);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkstemps64(template,suffixlen);
	}

	result=true_mkstemps64(translated,suffixlen);
	if(result>=0) {
		temp_commit(template,translated,suffixlen);
		instw_setpath(&instw,template);
	}
	logg("%d\tmkstemp\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

#ifndef __USE_FILE_OFFSET64
int mkostemps(char *template,int suffixlen,int flags) {
	char translated[PATH_MAX+1];
	instw_t instw;
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkostemps(%s,%d,0%o)\n",template,suffixlen,flags);
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkostemps(template,suffixlen,flags);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkostemps(template,suffixlen,flags);
	}

	result=true_mkostemps(translated,suffixlen,flags);
	if(result>=0) {
		temp_commit(template,translated,suffixlen);
		instw_setpath(&instw,template);
	}
	logg("%d\tmkstemp\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}
#endif

int mkostemps64(char *template,int suffixlen,int flags) {
	char translated[PATH_MAX+1];
	instw_t instw;
	int result;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkostemps64(%s,%d,0%o)\n",template,suffixlen,flags);
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkostemps64(template,suffixlen,flags);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkostemps64(template,suffixlen,flags);
	}

	result=true_mkostemps64(translated,suffixlen,flags);
	if(result>=0) {
		temp_commit(template,translated,suffixlen);
		instw_setpath(&instw,template);
	}
	logg("%d\tmkstemp\t%s\t#%s\n",result,instw.reslvpath,error(result));

	instw_delete(&instw);

	return result;
}

char *mkdtemp(char *template) {
	char translated[PATH_MAX+1];
	instw_t instw;
	char *result;
	int rcod;

	REFCOUNT;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"mkdtemp(%s)\n",template);
#endif

	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_mkdtemp(template);

	if(!temp_prepare(&instw,template,translated,sizeof(translated))) {
		instw_delete(&instw);
		return true_mkdtemp(template);
	}

	result=true_mkdtemp(translated);
	rcod=(result==NULL)?-1:0;
	if(result!=NULL) {
		temp_commit(template,translated,0);
		instw_setpath(&instw,template);
	}
	logg("%d\tmkdtemp\t%s\t#%s\n",rcod,instw.reslvpath,error(rcod));

	instw_delete(&instw);

	  /* the caller owns the template; hand back their own buffer */
	return (result==NULL)?NULL:template;
}

  /* Fortified calls bypass the ordinary interposed symbols. */
  /* The caller supplies no mode, so pass one explicitly.    */

#ifndef __USE_FILE_OFFSET64
int __open_2(const char *pathname, int flags) {
	return open(pathname, flags, 0);
}
#endif

int __open64_2(const char *pathname, int flags) {
	return open64(pathname, flags, 0);
}

#ifndef __USE_FILE_OFFSET64
int __openat_2(int dirfd, const char *pathname, int flags) {
	return openat(dirfd, pathname, flags, 0);
}
#endif

int __openat64_2(int dirfd, const char *pathname, int flags) {
	return openat(dirfd, pathname, flags, 0);
}


#if (GLIBC_MINOR >= 28)
int statx(int dirfd, const char *path, int flags,
          unsigned int mask, struct statx *buf) {

	int result;
	int status;
	instw_t instw;

	if (!libc_handle)
		initialize();

#if DEBUG
	debug(2,"statx(%d,%s,0%o,%u,%p)\n",dirfd,path,flags,mask,buf);
#endif

	  /* We were asked to work in "real" mode */
	if( !(__instw.gstatus & INSTW_INITIALIZED) ||
	    !(__instw.gstatus & INSTW_OKWRAP) )
		return true_statx(dirfd,path,flags,mask,buf);

	  /* An empty path asks about 'dirfd' itself (AT_EMPTY_PATH), */
	  /* so there is nothing for us to translate.                 */
	if(path==NULL || *path=='\0')
		return true_statx(dirfd,path,flags,mask,buf);

	instw_new(&instw);
	instw_setpathrel(&instw,dirfd,path);
	instw_getstatus(&instw,&status);

#if DEBUG
	instw_print(&instw);
#endif

	  /* instw gave us an absolute path, so 'dirfd' is spent */
	if(status&INSTW_TRANSLATED) {
		debug(4,"\teffective statx(%s)\n",instw.translpath);
		result=true_statx(AT_FDCWD,instw.translpath,flags,mask,buf);
	} else {
		debug(4,"\teffective statx(%s)\n",instw.path);
		result=true_statx(AT_FDCWD,instw.path,flags,mask,buf);
	}

	instw_delete(&instw);

	return result;
}
#endif


READLINKAT_T readlinkat (int dirfd, const char *path,
                      char *buf, size_t bufsiz) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
                    debug(2, "readlinkat(%d,%s, %s, %ld)\n", dirfd, path, buf, (long)bufsiz);
		 #endif
		 return readlink(path, buf, bufsiz);
		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "readlinkat(%d,%s, %s, %ld)\n", dirfd, path, buf, (long)bufsiz);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
 		return true_readlink(path, buf, bufsiz);
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 	
 	result=readlink(instw.path, buf, bufsiz);
 	
 	instw_delete(&instw);
 
	return result;
}

#if (GLIBC_MINOR >= 33)
int mknodat (int dirfd,const char *path,mode_t mode,dev_t dev) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "mknod(%s, mode, dev)\n", path);
		 #endif
		 return mknod(path, mode, dev);
		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "mknod(%s, mode, dev)\n", path);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
 		return true_mknod(path, mode, dev);
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 	
 	result=mknod(instw.path, mode, dev);
 	
 	instw_delete(&instw);
 
	return result;
}
#endif


int __xmknodat (int version, int dirfd,const char *path,mode_t mode,dev_t *dev) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "__xmknod(%d, %s, 0%o, %p)\n", version, path, mode, dev);
		 #endif
		 return __xmknod(version, path, mode, dev);
		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "__xmknod(%d, %s, 0%o, %p)\n", version, path, mode, dev);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
 		return true_xmknod(version, path, mode, dev);
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 	
 	result=__xmknod(version, instw.path, mode, dev);
 	
 	instw_delete(&instw);
 
	return result;
}


int renameat (int olddirfd, const char *oldpath,
                  int newdirfd, const char *newpath) {
 	
 	int result;
 	instw_t instwold;
 	instw_t instwnew;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if( (olddirfd == AT_FDCWD || *oldpath == '/') &&
             (newdirfd == AT_FDCWD || *newpath == '/') )
		{
		 #if DEBUG
			debug(2, "renameat(%d, %s, %d, %s)\n", olddirfd, oldpath, newdirfd, newpath);
		 #endif

		 return rename(oldpath, newpath); 


		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "renameat(%d, %s, %d, %s)\n", olddirfd, oldpath, newdirfd, newpath);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
 		return true_rename(oldpath, newpath);
	
 	instw_new(&instwold);
 	instw_new(&instwnew);
 	instw_setpathrel(&instwold,olddirfd,oldpath);
 	instw_setpathrel(&instwnew,newdirfd,newpath);
 	
#if DEBUG
 	instw_print(&instwold);
 	instw_print(&instwnew);
#endif
 	
 	result=rename(instwold.path, instwnew.path);
 	
 	instw_delete(&instwold);
 	instw_delete(&instwnew);
 
	return result;
}

/* Note that this only implements RENAME_NOREPLACE */
int renameat2 (int olddirfd, const char *oldpath,
                  int newdirfd, const char *newpath, unsigned int flags) {
  if (!libc_handle)
    initialize();

  if ( (flags & RENAME_NOREPLACE) == RENAME_NOREPLACE ) {
    instw_t instwnew;
    instw_new(&instwnew);
    instw_setpathrel(&instwnew,newdirfd,newpath);
    struct stat buf;
    if (stat(instwnew.path, &buf) == 0) return EEXIST;
  }
  return renameat (olddirfd, oldpath, newdirfd, newpath);
}

int symlinkat (const char *oldpath, int dirfd, const char *newpath) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *newpath == '/')
		{
		 #if DEBUG
			debug(2, "symlinkat(%s, %d, %s)\n", oldpath, dirfd, newpath);
		 #endif
		 return symlink(oldpath, newpath);
		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "symlinkat(%s, %d, %s)\n", oldpath, dirfd, newpath);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP))
 		return true_symlink(oldpath, newpath);
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,newpath);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 	
 	result=symlink(oldpath, instw.path);
 	
 	instw_delete(&instw);
 
	return result;
}


int unlinkat (int dirfd, const char *path, int flags) {
 	
 	int result;
 	instw_t instw;
 
 	/* If all we are doing is normal open, forgo refcounting, etc. */
         if(dirfd == AT_FDCWD || *path == '/')
		{
		 #if DEBUG
			debug(2, "unlinkat(%d,%s,0%o)\n", dirfd, path, flags);
		 #endif

		 /* If we have AT_REMOVEDIR then we need        */
		 /* rmdir() behaviour, according to unlinkat(2) */

		 if ( flags & AT_REMOVEDIR ) {
		    return rmdir(path); 
		 }
		 else {
		    return unlink(path);
		 }

		}
 
 	REFCOUNT;
 
 	if (!libc_handle)
 		initialize();
 
#if DEBUG
	debug(2, "unlinkat(%d,%s,0%o)\n", dirfd, path, flags);
#endif
 	
 	/* We were asked to work in "real" mode */
 	if(!(__instw.gstatus & INSTW_INITIALIZED) ||
 	   !(__instw.gstatus & INSTW_OKWRAP)) {
	 	/* If we have AT_REMOVEDIR then we need        */
		 /* rmdir() behaviour, according to unlinkat(2) */

		 if ( flags & AT_REMOVEDIR ) {
		    result=true_rmdir(path); 
		 }
		 else {
	 	    result=true_unlink(path);
		 }
	}
 	
	
 	instw_new(&instw);
 	instw_setpathrel(&instw,dirfd,path);
 	
#if DEBUG
 	instw_print(&instw);
#endif
 	
	 /* If we have AT_REMOVEDIR then we need        */
	 /* rmdir() behaviour, according to unlinkat(2) */

	 if ( flags & AT_REMOVEDIR ) {
	    result=rmdir(instw.path); 
	 }
	 else {
 	    result=unlink(instw.path);
	 }
 	
 	instw_delete(&instw);
 
	return result;

}
