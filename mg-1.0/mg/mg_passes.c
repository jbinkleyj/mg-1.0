/**************************************************************************
 *
 * mg_passes.c -- Driver for the various passes
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
 *       @(#)mg_passes.c	1.7 16 Mar 1994
 *
 **************************************************************************/

#include "sysfuncs.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <math.h>
#include <malloc.h>

#include "memlib.h"
#include "messages.h"
#include "timing.h"

#include "mg_files.h"
#include "mg.h"
#include "build.h"
#include "text.h"
#include "stemmer.h"


char *SCCS_Id_mg_builder = "@(#)mg_passes.c	1.7 16 Mar 1994";

#define MAX_PASSES 7

#define SPECIAL 1
#define TEXT_PASS_1 2
#define TEXT_PASS_2 4
#define INVF_PASS_1 8 
#define INVF_PASS_2 16
#define IVF_PASS_1 32
#define IVF_PASS_2 64

#define MIN_BUF 8192
#define	TERMRECORD	'\002'

unsigned long buf_size = 3*1024*1024;   /* 3Mb */
unsigned long invf_buffer_size = 5*1024*1024;   /* 5Mb */
unsigned long ChunkLimit = 0;
char InvfLevel = 2;
char SkipSGML = 0;
char MakeWeights = 0;
FILE *Comp_Stats = NULL;
int comp_stat_point = 0;
u_long bytes_processed = 0;
u_long bytes_received = 0;
int stem_method = 0;

static char Passes = 0;
static unsigned long trace = 0;
static int Dump = 0;
static char **files = NULL;
static int num_files = 0;
static char *trace_name = NULL;


typedef struct pass_data
{
  char *name;
  int (*init)(char *);
  int (*process)(uchar *, int);
  int (*done)(char *);
#ifdef SUNOS5
  clock_t init_time;
  clock_t process_time;
  clock_t done_time;
#else
  struct timeval init_time;
  struct timeval process_time;
  struct timeval done_time;
#endif
} pass_data;

static pass_data PassData[MAX_PASSES] = {
  {"special", init_special, process_special, done_special, 
#ifdef SUNOS5
     0, 0, 0}, 
#else
     {0, 0}, {0, 0}, {0, 0}}, 
#endif
  {"text.pass1", init_text_1, process_text_1, done_text_1, 
#ifdef SUNOS5
     0, 0, 0}, 
#else
     {0, 0}, {0, 0}, {0, 0}}, 
#endif
  {"text.pass2", init_text_2, process_text_2, done_text_2, 
#ifdef SUNOS5
     0, 0, 0}, 
#else
     {0, 0}, {0, 0}, {0, 0}}, 
#endif
  {"invf.pass1", init_invf_1, process_invf_1, done_invf_1, 
#ifdef SUNOS5
     0, 0, 0}, 
#else
     {0, 0}, {0, 0}, {0, 0}}, 
#endif
  {"invf.pass2", init_invf_2, process_invf_2, done_invf_2, 
#ifdef SUNOS5
     0, 0, 0}, 
#else
     {0, 0}, {0, 0}, {0, 0}},
#endif
  {"ivf.pass1", init_ivf_1, process_ivf_1, done_ivf_1, 
#ifdef SUNOS5
     0, 0, 0}, 
#else
     {0, 0}, {0, 0}, {0, 0}}, 
#endif
  {"ivf.pass2", init_ivf_2, process_ivf_2, done_ivf_2, 
#ifdef SUNOS5
     0, 0, 0}}; 
#else
     {0, 0}, {0, 0}, {0, 0}}};
#endif


static char *usage_str = "\nUSAGE:\n"
 "  %s [-h] [-G] [-D] [-1|-2|-3] [-T1] [-T2] [-I1] [-I2] [-N1]\n"
"  %*s [-N2] [-W] [-S] [-b buffer-size] [-d dictionary-directory]\n"
"  %*s [-t trace-point Mb] [-m invf-memory] [-c chunk-limit]\n"
"  %*s [-n trace-name] [-C comp-stat-size] [-s stem_method] -f doc-collection-name\n";









static void usage(char *err)
{
  if (err)
    Message(err);
  fprintf(stderr, usage_str, msg_prefix, strlen(msg_prefix), "", 
	  strlen(msg_prefix), "", strlen(msg_prefix), "");
  exit(1);
}


#if defined(SUNOS4) || defined(MIPS)
static void time_normalise(struct timeval *t)
{
   while (t->tv_usec < 0)
    {
      t->tv_usec += 1000000;
      t->tv_sec--;
    }
  while (t->tv_usec > 1000000)
    {
      t->tv_usec -= 1000000;
      t->tv_sec++;
    }
}
#endif


#ifdef SUNOS5
static char *cputime(clock_t clk)
{
  static char buf[15];
  double CPU, hour, min, sec;
  CPU = (double)clk / (double)CLK_TCK;
  hour = floor(CPU/3600);
  min = floor((CPU-hour*3600)/60);
  sec = CPU-hour*3600-min*60;
  sprintf(buf, "%02.0f:%02.0f:%05.2f", hour, min, sec);
  return(buf);
}
#else
static char *cputime(struct timeval *t)
{
  static char buf[15];
  double CPU, hour, min, sec;
  CPU = (double)t->tv_sec + (double)t->tv_usec/1000000;
  hour = floor(CPU/3600);
  min = floor((CPU-hour*3600)/60);
  sec = CPU-hour*3600-min*60;
  sprintf(buf, "%02.0f:%02.0f:%05.2f", hour, min, sec);
  return(buf);
}
#endif

#if 0
static char *str_comma(unsigned long u)
{
  static char buf[20];
  unsigned long a, b, c, d;
  a = u/1000000000;
  u -= a*1000000000;
  b = u/1000000;
  u -= b*1000000;
  c = u/1000;
  u -= c*1000;
  d = u;
  
  if (a)
    sprintf(buf, "%u,%03u,%03u,%03u", a, b, c, d);
  else
    if (b)
      sprintf(buf, "%u,%03u,%03u", b, c, d);
    else
      if (c)
	sprintf(buf, "%u,%03u", c, d);
      else
	sprintf(buf, "%u", d);
  return(buf);
}
#endif




int open_next_file(int in_fd)
{
  if (in_fd > 0)
    close(in_fd);
  if (num_files == 0)
    return(-1);
  if ((in_fd = open(files[0], O_RDONLY)) == -1)
    FatalError(1, "Cannot open %s", files[0]);
  files++;
  num_files--;
  return(in_fd);
}


static void driver(int in_fd, FILE *Trace, char *file_name)
{
  int pass, num=1;
  
  char *buffer = Xmalloc(buf_size);
  unsigned long num_docs = 0;
  unsigned long block_bytes = 0;
  register int buf_left = buf_size;
  register char *look_pos = buffer;
  register char *end_pos = buffer;
  
  ProgTime StartTime, InitTime, ProcTime, DoneTime;

  GetTime(&StartTime);

  for (pass=0; pass<MAX_PASSES; pass++)
    if (Passes & (1<<pass))
	{
	  pass_data *pd = &PassData[pass];
#ifdef SUNOS5
	  struct tms tims;
	  times(&tims);
	  pd->init_time -= tims.tms_utime + tims.tms_stime;
#else
	  struct rusage ru;

	  getrusage(RUSAGE_SELF, &ru);
	  pd->init_time.tv_sec -= ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
	  pd->init_time.tv_usec -= ru.ru_utime.tv_usec + ru.ru_stime.tv_usec;
#endif
	  if (pd->init(file_name) == COMPERROR)
	    FatalError(1, "Error during init of \"%s\"", pd->name);

#ifdef SUNOS5
	  times(&tims);
	  pd->init_time += tims.tms_utime + tims.tms_stime;
#else
	  getrusage(RUSAGE_SELF, &ru);
	  pd->init_time.tv_sec += ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
	  pd->init_time.tv_usec += ru.ru_utime.tv_usec + ru.ru_stime.tv_usec;
	  time_normalise(&pd->init_time);
#endif
	}

  GetTime(&InitTime);
  for (;;)
    {
      int len=0;
      char *base = look_pos;
      
      while (look_pos != end_pos && *look_pos != TERMRECORD)
	look_pos++;
      
      while (look_pos == end_pos)
	{
	  if (buf_left < MIN_BUF)
	    {
#ifdef SUNOS5
	      memmove(buffer, base, end_pos-base);
#else
	      bcopy(base, buffer, end_pos-base);
#endif
	      look_pos = buffer + (end_pos-base);
	      buf_left = buf_size - (end_pos-base);
	      end_pos = look_pos;
	      base = buffer;
	    }
	  if (buf_left)
	    {
	      num = read(in_fd, end_pos, buf_left);
	      if (num == 0)
		if ((in_fd = open_next_file(in_fd)) != -1)
		  num = read(in_fd, end_pos, buf_left);
	      bytes_received += num;
	      buf_left -= num;
	      end_pos += num;
	    }
	  while (look_pos != end_pos && *look_pos != TERMRECORD)
	    look_pos++;
	  if (buf_left == 0 && base == buffer && look_pos == end_pos)
	    {
	      Message("Unable to find document terminator (i.e ^B)"
		      " in the document");
	      FatalError(1, "The document is in excess of %d chars long",
			 look_pos - base);
	    }
	  if (!num) break;
	}
      len = look_pos++ - base;
      
      if (!num && base == end_pos)
	break;
      
      bytes_processed += len;

      if (!len) 
	Message("Warning : Processing zero length document");

      for (pass=0; pass<MAX_PASSES; pass++)
	if (Passes & (1<<pass))
	  {
	    register pass_data *pd = &PassData[pass];
	    
#ifdef SUNOS5
	    struct tms tims;
	    times(&tims);
	    pd->process_time -= tims.tms_utime + tims.tms_stime;
#else
	    struct rusage ru;
	    register struct timeval *tv = &pd->process_time;

	    getrusage(RUSAGE_SELF, &ru);
	    tv->tv_sec -= ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
	    tv->tv_usec -= ru.ru_utime.tv_usec + ru.ru_stime.tv_usec;
#endif	    

	    if (pd->process((uchar*)base, len) == COMPERROR)
	      {
		Message("Error during processing of \"%s\"", pd->name);
		if (Dump || Trace)
		  {
		    int i;
		    FILE *f = Trace ? Trace : stderr;
		    fprintf(f,"-=- * -=- * -=- * -=- * -=- * -=- * -=-\n");
		    for (i=0; i<len; i++)
		      {
			char ch = base[i];
			if (ch == '\1' || ch == '\2')
			  ch = '\n';
			putc(ch, f);
		      }			    
		    fprintf(f,"-=- * -=- * -=- * -=- * -=- * -=- * -=-\n");
		  }
		if (Trace)
		  fprintf(Trace, "%10lu bytes |%7lu docs | %s\n", 
			  bytes_processed, num_docs, 
			  ElapsedTime(&StartTime, NULL));
		exit(1);
	      }
	    
#ifdef SUNOS5
	    times(&tims);
	    pd->process_time += tims.tms_utime + tims.tms_stime;
#else
	    getrusage(RUSAGE_SELF, &ru);
	    tv->tv_sec += ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
	    tv->tv_usec += ru.ru_utime.tv_usec + ru.ru_stime.tv_usec;
#endif
	  }
      num_docs++;
      if (Trace)
	{
	  block_bytes += (look_pos - base);
	  if (block_bytes >= trace)
	    {
#ifdef SUNOS4
	      struct mallinfo mi;
	      mi = mallinfo();
	      block_bytes -= trace;
	      fprintf(Trace, "%10lu bytes |%7lu docs |%7.3f Mb | %s\n", 
		      bytes_processed, num_docs, mi.arena/1024.0/1024.0,
		      ElapsedTime(&StartTime, NULL));
#else
	      block_bytes -= trace;
	      fprintf(Trace, "%10u bytes |%7u docs | %s\n", 
		      bytes_processed, num_docs, 
		      ElapsedTime(&StartTime, NULL));
#endif
	    }
	} 
      if (!num && look_pos-1 == end_pos)
	break;
    }

#ifdef SUNOS4
  for (pass=0; pass<MAX_PASSES; pass++)
    if (Passes & (1<<pass))
      time_normalise(&PassData[pass].process_time);
#endif
  
  GetTime(&ProcTime);
  
  for (pass=0; pass<MAX_PASSES; pass++)
    if (Passes & (1<<pass))
      {
	pass_data *pd = &PassData[pass];
#ifdef SUNOS5
	struct tms tims;
	times(&tims);
	pd->done_time -= tims.tms_utime + tims.tms_stime;
#else
	struct rusage ru;
	
	getrusage(RUSAGE_SELF, &ru);
	pd->done_time.tv_sec -= ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
	pd->done_time.tv_usec -= ru.ru_utime.tv_usec + ru.ru_stime.tv_usec;
#endif	
	if (pd->done(file_name) == COMPERROR)
	  FatalError(1, "Error during done of \"%s\"", pd->name);
	
#ifdef SUNOS5
	times(&tims);
	pd->done_time += tims.tms_utime + tims.tms_stime;
#else
	getrusage(RUSAGE_SELF, &ru);
	pd->done_time.tv_sec += ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
	pd->done_time.tv_usec += ru.ru_utime.tv_usec + ru.ru_stime.tv_usec;
	time_normalise(&pd->done_time);
#endif	
      }
  if (Trace)
    {
#ifdef SUNOS4
      struct mallinfo mi;
      mi = mallinfo();
      fprintf(Trace, "%10lu bytes |%7lu docs |%7.3f Mb | %s\n", 
	      bytes_processed, num_docs, mi.arena/1024.0/1024.0,
	      ElapsedTime(&StartTime, NULL));
#else
      fprintf(Trace, "%10u bytes |%7u docs | %s\n", 
	      bytes_processed, num_docs, 
	      ElapsedTime(&StartTime, NULL));
#endif	
    }
  
  GetTime(&DoneTime);
  
  Message("");
  Message("%10s      :    init         process        done","");    
  for (pass=0; pass<MAX_PASSES; pass++)
    if (Passes & (1<<pass))
      {
	pass_data *pd = &PassData[pass];
        char it[15], pt[15], dt[15];
#ifdef SUNOS5
	strcpy(it, cputime(pd->init_time));
	strcpy(pt, cputime(pd->process_time));
	strcpy(dt, cputime(pd->done_time));
#else
	strcpy(it, cputime(&pd->init_time));
	strcpy(pt, cputime(&pd->process_time));
	strcpy(dt, cputime(&pd->done_time));
#endif
	Message(  "%-10s      : %s   %s   %s",pd->name, it, pt, dt);
      }
  Message("");
  Message("Init time       : %s", ElapsedTime(&StartTime, &InitTime));
  Message("Process time    : %s", ElapsedTime(&InitTime, &ProcTime));
  Message("Done time       : %s", ElapsedTime(&ProcTime, &DoneTime));
  Message("Total time      : %s", ElapsedTime(&StartTime, &DoneTime));
  Message("Documents       : %u", num_docs);
  Message("Bytes received  : %u", bytes_received);
  Message("Bytes processed : %u", bytes_processed);
  Message("Process Rate    : %.1f kB per cpu second", 
	  (double)bytes_processed/(ProcTime.CPUTime - InitTime.CPUTime)/1024);
  free(buffer);
}



void main(int argc, char **argv)
{
  int ch, in_fd;
  char *filename = NULL;
  FILE *Trace = NULL;

  msg_prefix = argv[0];

  opterr = 0;
  while ((ch = getopt (argc, argv, "hC:WGSD123f:d:b:T:I:t:m:N:c:n:s:")) != -1)
    {
      switch (ch)
	{
	case 'G' : SkipSGML = 1; break;
	case 'S' : Passes |= SPECIAL; break;
	case '1' : InvfLevel = 1; break;
	case '2' : InvfLevel = 2; break;
	case '3' : InvfLevel = 3; break;
	case 'f' : filename = optarg; break;
	case 'n' : trace_name = optarg; break;
	case 'D' : Dump = 1; break;
	case 'W' : MakeWeights = 1; break;
	case 'd' : set_basepath(optarg); break;
	case 's' : stem_method = atoi(optarg) & STEMMER_MASK; break;
	case 'b' : buf_size = atoi(optarg)*1024; break;
	case 'C' : comp_stat_point = atoi(optarg)*1024; break;
	case 'c' : ChunkLimit = atoi(optarg); break;
	case 'm' : invf_buffer_size = (int)(atof(optarg)*1024*1024); break;
	case 'I' : 
	  if (*optarg == '1')
	    Passes |= INVF_PASS_1;
	  else
	    if (*optarg == '2')
	      Passes |= INVF_PASS_2;
	    else
	      usage("Invalid pass number");
	  break;
	case 'N' : 
	  if (*optarg == '1')
	    Passes |= IVF_PASS_1;
	  else
	    if (*optarg == '2')
	      Passes |= IVF_PASS_2;
	    else
	      usage("Invalid pass number");
	  break;
	case 'T' : 
	  if (*optarg == '1')
	    Passes |= TEXT_PASS_1;
	  else
	    if (*optarg == '2')
	      Passes |= TEXT_PASS_2;
	    else
	      usage("Invalid pass number");
	   break;
	case 't' :
	  trace = (unsigned long)(atof(optarg)*1024*1024);
	  break;
	case 'h' : 
	case '?' : 
	  usage(NULL);
	}
    }

  if (!filename || *filename == '\0')
    FatalError(1, "A document collection name must be specified.");

  if (buf_size < MIN_BUF)
    FatalError(1, "The buffer size must exceed 1024 bytes.");

  if ((Passes & (INVF_PASS_1|INVF_PASS_2)) == (INVF_PASS_1|INVF_PASS_2))
    FatalError(1, "I1 and I2 cannot be done simultaneously.");

  if ((Passes & (IVF_PASS_1|IVF_PASS_2)) == (IVF_PASS_1|IVF_PASS_2))
    FatalError(1, "N1 and N2 cannot be done simultaneously.");

  if ((Passes & (TEXT_PASS_1|TEXT_PASS_2)) == (TEXT_PASS_1|TEXT_PASS_2))
    FatalError(1, "T1 and T2 cannot be done simultaneously.");

  if (!Passes)
    FatalError(1, "S, T1, T2, N1, N2, I1 or I2 must be specified.");

  if (optind < argc)
    {
      if ((in_fd = open(argv[optind], O_RDONLY)) == -1)
	FatalError(1, "Cannot open %s", argv[optind]);
      files = &argv[optind+1];
      num_files = argc-(optind+1);
    }
  else
    in_fd = 0;  /* stdin */


  if (trace)
    {
      if (!trace_name)
	trace_name = make_name(filename, TRACE_SUFFIX, NULL);
      if (!(Trace = fopen(trace_name, "a")))
	Message("Unable to open \"%s\". No tracing will be done.", trace_name);
      else
	setbuf(Trace, NULL);
    }
  else
    Trace = NULL;

  if (comp_stat_point)
    {
      char *name = make_name(filename, COMPRESSION_STATS_SUFFIX, NULL);
      if (!(Comp_Stats = fopen(name, "w")))
	Message("Unable to open \"%s\". No comp. stats. will be generated.",
		name);
    }


  if (Trace)
    {
      int i;
      fprintf(Trace,"\n\n\t\t-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n");
      for (i=0; i<argc; i++)
	fprintf(Trace, "%s ", argv[i]);
      fprintf(Trace,"\n\n");
    }

  driver(in_fd, Trace, filename);

  if (Trace)
    fclose(Trace);

  if (Comp_Stats)
    fclose(Comp_Stats);

  exit(0);
}




