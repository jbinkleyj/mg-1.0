/**************************************************************************
 *
 * sysfuncs.h -- Special system stuff
 * Copyright (C) 1994  Neil Sharman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *       @(#)sysfuncs.h	1.11 09 Mar 1994
 *
 **************************************************************************/

#ifndef H_SYSFUNCS
#define H_SYSFUNCS

#include "config.h"




#ifdef SUNOS5
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <math.h>


#define log2(x) (log(x)/M_LN2)

#define bzero(a, b) memset((a), 0, (b))

#define bcopy(src, dst, n) memcpy((dst), (src), (n))

int _sysconf(int);
void *__builtin_alloca(size_t);
#define setbuffer(f, b, s)  setvbuf(f, b, _IOFBF, s)

#endif

#ifdef MIPS
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <math.h>
#include <getopt.h>


#define log2(x) (log(x)/M_LN2)

#define bzero(a, b) memset((a), 0, (b))

#define bcopy(src, dst, n) memcpy((dst), (src), (n))

char *regcmp (char *string1, ...);
char *regex (char *re, char *subject, ...);


#endif


#ifdef HP_PA
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <math.h>


#define log2(x) (log(x)/M_LN2)

#define bzero(a, b) memset((a), 0, (b))

#define bcopy(src, dst, n) memcpy((dst), (src), (n))

#endif



#ifdef SUNOS4
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/vfs.h>

/*****************************************************************************
 *
 * Section 2 of the manual pages 
 *
 *****************************************************************************/

/*******************/
/*    EXTERNALS    */
/*******************/

extern int errno;


/*******************/
/*    FUNCTIONS    */
/*******************/

int close(int fd);

int flock(int fd, int operation);

int fork(void);

int ftruncate(int fd, off_t length);

int getpagesize(void);

pid_t getpid(void);

pid_t getppid(void);

int getrusage(int, struct rusage *); 

off_t lseek(int, off_t, int);

int mknod(char *path, int mode);

int munmap(caddr_t *addr, int len);

int read(int fd, void *buf, unsigned nbyte);

int rename(const char *path1, const char *path2);

int select(int width, fd_set *readfds, fd_set *writefds, 
	   fd_set *exceptfds, struct timeval *timeout);

void *shmat(int shmid, void *shmaddr, int shmflg);

int shmctl(int shmid, int cmd, ...);

int shmdt(void *shmaddr);

int shmget(key_t key, int size, int shmflg);

int statfs(char *path, struct statfs *buf);

int unlink(const char *);

time_t time(time_t *tloc);

int truncate(char *path, off_t length);

pid_t wait(void *);

pid_t waitpid(int pid, int *statusp, int options);

int write(int fd, const void *buf, unsigned nbyte);



/*****************************************************************************
 *
 * Section 3 of the manual pages 
 *
 *****************************************************************************/

/*******************/
/*    EXTERNALS    */
/*******************/

extern char *optarg;		
extern int optind, opterr;

extern int sys_nerr;
extern char *sys_errlist[];


/*******************/
/*    FUNCTIONS    */
/*******************/


void bcopy(char *src, char *dst, int length);

void bzero(char *src, int length);

void endpwent();

int fclose(FILE *);

int fflush(FILE *);

#ifdef __GNUC__
int fgetc(FILE *);
#endif

#ifdef __GNUC__
int ungetc(int c, FILE*);
int fgetc(FILE *);
#endif

int fprintf(FILE *, const char *, ...);

#ifdef __GNUC__
int fputc(int, FILE *);
int scanf(char*, ...);
int sscanf(char*, char*, ...);
int fscanf(FILE*, char*, ...);
#endif

int fputs(const char *, FILE *);

size_t fread(void *, size_t, size_t, FILE *);

int fseek(FILE *, long int, int);

#if defined(__sun) && defined(__sparc)
int ftime(struct timeb *tp);
#endif

size_t fwrite(const void *, size_t, size_t, FILE *);

int getopt(int argc, char **argv, char *optstring);

int isatty(int fd);

void mallocmap(void);

int pclose(FILE *);

void perror(const char *s);

int printf(const char *, ...);

long random(void);

void rewind(FILE *);

char *re_comp(char *);

int re_exec(char *);

void setbuf(FILE *, char *buf);

int srandom(unsigned seed);

size_t strftime(char *buf, size_t bufsize, const char *fmt, 
		const struct tm *tm);

long int strtol(const char *str, char **ptr, int base);

int system(const char *cmd);

char tolower(char);

char toupper(char);

void usleep(unsigned);

int vfprintf(FILE *, const char *, va_list);

void *valloc(int);

#ifdef __GNUC__
int vsprintf(char *, const char *, va_list);
#endif

int _filbuf(FILE *);

int _flsbuf(unsigned, FILE *);

#endif


#endif
