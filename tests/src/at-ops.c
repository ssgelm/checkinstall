/* The *at calls with a real directory descriptor and a relative name. A
 * wrapper that drops dirfd resolves the name against the working directory
 * instead, so these run from a different directory than the target. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int dirfd, r=2;
	if(argc<4) return 2;                 /* op, dir, name */

	if((dirfd=open(argv[2],O_RDONLY|O_DIRECTORY))<0) return 3;

	if(!strcmp(argv[1],"openat")) {
		int fd=openat(dirfd,argv[3],O_WRONLY|O_CREAT|O_TRUNC,0644);
		if(fd>=0) { r=write(fd,"x\n",2)==2?0:1; close(fd); } else r=1;
	} else if(!strcmp(argv[1],"mkdirat")) {
		r=mkdirat(dirfd,argv[3],0755)?1:0;
	} else if(!strcmp(argv[1],"symlinkat")) {
		r=symlinkat("target",dirfd,argv[3])?1:0;
	} else if(!strcmp(argv[1],"unlinkat")) {
		r=unlinkat(dirfd,argv[3],0)?1:0;
	} else if(!strcmp(argv[1],"fstatat")) {
		struct stat s;
		r=fstatat(dirfd,argv[3],&s,0)?1:0;
	} else if(!strcmp(argv[1],"renameat")) {
		if(argc<5) { close(dirfd); return 2; }
		r=renameat(dirfd,argv[3],dirfd,argv[4])?1:0;
	}

	close(dirfd);
	return r;
}
