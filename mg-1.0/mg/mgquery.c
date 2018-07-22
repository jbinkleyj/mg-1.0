/**************************************************************************
 *
 * mgquery.c -- The   M G Q U E R Y   program
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
 *       @(#)mgquery.c	1.12 21 Mar 1994
 *
 **************************************************************************/

char *SCCS_Id_mg_query = "@(#)mgquery.c	1.12 21 Mar 1994";

#include "sysfuncs.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef SUNOS5
#include <libgen.h>
#include <sys/procfs.h>
#endif

#include "messages.h"
#include "timing.h"
#include "memlib.h"

#include "filestats.h"
#include "invf.h"
#include "text.h"
#include "mg.h"
#include "lists.h"
#include "backend.h"
#include "environment.h"
#include "globals.h"
#include "read_line.h"
#include "mg_errors.h"
#include "commands.h"
#include "text_get.h"



FILE *OutFile = NULL, *InFile = NULL;
Bool OutPipe = 0, InPipe = 0;
Bool Quitting = FALSE;

#ifdef TREC_MODE
char *trec_ids = NULL;
long *trec_paras = NULL;
#endif

static volatile int PagerRunning = FALSE;
static volatile int Ctrl_C = FALSE;













/*****************************************************************************/

typedef enum {S_Time, S_Mem, S_Size, S_File} S_Type;

static struct Stat
{
  S_Type typ;
  char *name;
  char *text;
} *Stats = NULL;
static int NumStats = 0;

static void Clear_Stats(void)
{
  if (Stats)
    {
      int i;
      for (i=0; i<NumStats; i++)
	{
	  if (Stats[i].name) Xfree(Stats[i].name);
	  if (Stats[i].text) Xfree(Stats[i].text);
	}
      Xfree(Stats);
      Stats = NULL;
      NumStats = 0;
    }
}

static void Add_Stats(S_Type typ, char *name, char *fmt, ...)
{
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsprintf(buf, fmt, args);
  if (Stats)
    Stats = Xrealloc(Stats, (++NumStats) * sizeof(*Stats));
  else
    Stats = Xmalloc( (++NumStats) * sizeof(*Stats));
  Stats[NumStats-1].typ = typ;
  Stats[NumStats-1].name = Xstrdup(name);
  Stats[NumStats-1].text = Xstrdup(buf);
}

static void Display_Stats(FILE *f)
{
  static char *sep = "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-="
    "-=-=-=-=-=-=-=-=-=-=-";
  char *names[] = {"Time:  ", "Memory:", "Sizes: ", "Disk:  ", "       "};
  int i, last_typ = -1;
  size_t len = 0;
  if (NumStats == 0) return;
  fprintf(f,"%s\n", sep);
  for (i=0; i<NumStats; i++)
    if (strlen(Stats[i].name) > len)
      len = strlen(Stats[i].name);
  for (i=0; i<NumStats; i++)
    {
      int typ = 4;
      if (Stats[i].typ != last_typ)
	typ = last_typ = Stats[i].typ;
      fprintf(f,"%s %-*s %s\n", names[typ], (int)len, Stats[i].name, Stats[i].text);
    }
  fprintf(f,"%s\n", sep);
}

/*****************************************************************************/


static void QueryTimeStats(ProgTime *Start, ProgTime *invf, ProgTime *text)
{
  if (!BooleanEnv(GetEnv("briefstats"),FALSE))
    {
      Add_Stats(S_Time, "invf", ElapsedTime(Start, invf));
      Add_Stats(S_Time, "text", ElapsedTime(invf, text));
    }
  Add_Stats(S_Time, "total", ElapsedTime(Start, text));
}

static void StartUpTimeStats(InitQueryTimes *iqt)
{
  if (!BooleanEnv(GetEnv("briefstats"),FALSE))
    {
      Add_Stats(S_Time, "dict [stem]", ElapsedTime(&iqt->Start, 
						   &iqt->StemDict));
      Add_Stats(S_Time, "weights", ElapsedTime(&iqt->StemDict, 
					       &iqt->ApproxWeights));
      Add_Stats(S_Time, "dict [text]", ElapsedTime(&iqt->ApproxWeights,
						   &iqt->CompDict));
      Add_Stats(S_Time, "Inverted", ElapsedTime(&iqt->CompDict,
						   &iqt->Invf));
      Add_Stats(S_Time, "Compressed", ElapsedTime(&iqt->Invf,
						   &iqt->Text));
    }
  Add_Stats(S_Time, "total", ElapsedTime(&iqt->Start, &iqt->Text));
}




#ifdef SUNOS5
static u_long process_mem(void)
{
  prstatus_t pr;
  static int fd = -1;
  if (fd == -1)
    {
      char buf[128];
      sprintf(buf, "/proc/%d", getpid());
      fd = open(buf, O_RDONLY);
    }
  if (fd == -1 || ioctl(fd, PIOCSTATUS, &pr) == -1)
    return 0;
  return pr.pr_brksize;
}
#endif




static void MemStats(query_data *qd)
{
  if (!BooleanEnv(GetEnv("briefstats"),FALSE))
    {
#ifdef SUNOS4
      struct rusage rusage;
      getrusage(RUSAGE_SELF, &rusage);

      Add_Stats(S_Mem, "process mem", "%7.3f Mb",
		(double)(rusage.ru_maxrss*getpagesize()/1024.0/1024.0));
#endif
#ifdef SUNOS5
      Add_Stats(S_Mem, "process mem", "%7.3f Mb",
		(double)(process_mem()/1024.0/1024.0));
#endif
      Add_Stats(S_Mem, "dict [stem]", "%7.1f kB", 
		(double)qd->sd->MemForStemDict/1024);
      Add_Stats(S_Mem, "dict [text]", "%7.1f kB", 
		(double)qd->cd->MemForCompDict/1024);
      if (qd->awd)
	Add_Stats(S_Mem, "weights",     "%7.1f kB", 
		  (double)qd->awd->MemForWeights/1024);
    }
  if (qd->awd)
    Add_Stats(S_Mem, "total [peak]", "%7.1f kB", 
	      (double)(qd->max_mem_in_use + qd->sd->MemForStemDict +
		       qd->cd->MemForCompDict + qd->awd->MemForWeights)/1024);
  else
    Add_Stats(S_Mem, "total [peak]", "%7.1f kB", 
	      (double)(qd->max_mem_in_use + qd->sd->MemForStemDict +
		       qd->cd->MemForCompDict)/1024);

}



static void SizeStats(query_data *qd)
{
  Add_Stats(S_Size, "skips",        "%7d", qd->hops_taken);
  Add_Stats(S_Size, "pointers",     "%7d", qd->num_of_ptrs);
  Add_Stats(S_Size, "accumulators", "%7d", qd->num_of_accum);
  Add_Stats(S_Size, "terms",        "%7d", qd->num_of_terms);
  Add_Stats(S_Size, "answers",      "%7d", qd->num_of_ans);
  Add_Stats(S_Size, "index lookups","%7d", qd->text_idx_lookups);
}

static void TotalSizeStats(query_data *qd)
{
  Add_Stats(S_Size, "skips",        "%7d", qd->tot_hops_taken);
  Add_Stats(S_Size, "pointers",     "%7d", qd->tot_num_of_ptrs);
  Add_Stats(S_Size, "accumulators", "%7d", qd->tot_num_of_accum);
  Add_Stats(S_Size, "terms",        "%7d", qd->tot_num_of_terms);
  Add_Stats(S_Size, "answers",      "%7d", qd->tot_num_of_ans);
  Add_Stats(S_Size, "index lookups","%7d", qd->tot_text_idx_lookups);
}


static void StatFile(File *F)
{
  static unsigned long NumBytes = 0, NumSeeks = 0, NumReads = 0;
  if (F)
    if ((int)F != -1)
      {
	if (!BooleanEnv(GetEnv("briefstats"),FALSE))
	  Add_Stats(S_File, F->name, "%7.1f kB (%3d seeks, %7d reads)",
		    (double)F->Current.NumBytes/1024, F->Current.NumSeeks, 
		    F->Current.NumReads);
	NumBytes += F->Current.NumBytes;
	NumSeeks += F->Current.NumSeeks;
	NumReads += F->Current.NumReads;
      }
    else
      {
	Add_Stats(S_File, "total", "%7.1f kB (%3d seeks, %7d reads)",
		  (double)NumBytes/1024, NumSeeks, NumReads);
	NumSeeks = NumReads = NumBytes = 0;
      }
	      
}


static void File_Stats(query_data *qd)
{
  StatFile(qd->File_comp_dict);
  StatFile(qd->File_fast_comp_dict);
  StatFile(qd->File_text_idx_wgt);
  StatFile(qd->File_text);
  StatFile(qd->File_stem);
  StatFile(qd->File_invf);
  StatFile(qd->File_weight_approx);
  StatFile(qd->File_text_idx);
  StatFile((File *)(-1));
}


char *get_query(void)
{
  char *line, *LinePtr;
  WritePrompt();
  do 
    {
      do
	{
	  line = GetMultiLine();
	  if (line == NULL)
	    {
	      if (stdin == InFile)
		return (NULL);			/* EOF */
	      if (InPipe)
		pclose(InFile);
	      else
		fclose(InFile);
	      InPipe = 0;
	      InFile = stdin;
	    }
	}
      while (line == NULL);
      LinePtr = ProcessCommands(line);
      if (CommandsErrorStr)
	fprintf(stderr, "%s\n", CommandsErrorStr);
    }
  while (*LinePtr == '\0' && !Quitting);
  return(LinePtr);
}



/* This is executed when a SIGPIPE is detected 
   i.e. If some one quits out of the PAGER, this is executed */
#ifdef SUNOS5
static void SIGPIPE_handler(int sig)
#else
static void SIGPIPE_handler(int sig, int code, 
			    struct sigcontext *scp, char *addr)
#endif
{
  signal(SIGPIPE, SIG_IGN);
  PagerRunning = FALSE;
}


/* This is executed when a SIGINT (i.e. CTRL-C) is detected */
#ifdef SUNOS5
static void SIGINT_handler(int sig)
#else
static void SIGINT_handler(int sig, int code, 
			    struct sigcontext *scp, char *addr)
#endif
{
  Ctrl_C = TRUE;
}
  
static char *post_proc = NULL;

#if defined(SUNOS5) || defined(MIPS)
static char *regcmp_pattern;
#endif

  

void GetPostProc(char *line)
{
  char *start, *finish;
  if (post_proc)
    {
      Xfree(post_proc);
      post_proc = NULL;
    }
#if defined(SUNOS5) || defined(MIPS)
  if (regcmp_pattern)
    {
      Xfree(regcmp_pattern);
      regcmp_pattern = NULL;
    }
#endif
  start = strchr(line, '\"');
  finish = strrchr(line, '\"');
  if (start != finish)
    {
      /* found a pattern */
      *finish = '\0';
      post_proc = Xstrdup(start + 1);
      strcpy(start, finish + 1);
      if (BooleanEnv(GetEnv("verbatim"), TRUE) == FALSE)
	{
	  char *s;
#if defined(SUNOS5) || defined(MIPS)
	  s = regcmp(post_proc, NULL);
	  regcmp_pattern = s;
#else
	  s = re_comp(post_proc);
#endif
	  if (!s)
	    {
	      Xfree(post_proc);
	      post_proc = NULL;
	    }
	}
    }
  else
    if (start != NULL)
      {
	/* found a single speech mark. Delete It. */
	strcpy(start, start+1);
      }
}

Bool PostProc(char *UDoc, Bool verbatim)
{
  if (!post_proc) 
    return TRUE;

  if (verbatim)
    return strstr(UDoc, post_proc) != NULL;
#if defined(SUNOS5) || defined(MIPS)
  return regex(regcmp_pattern, (char*)UDoc, NULL) != NULL;
#else
  return re_exec((char*)UDoc) != NULL;
#endif
}



static DocEntry *in_chain(int para, int ip, DocEntry *dc)
{
  while (dc)
    {
      if (dc->DocNum-ip == para)
	return dc;
      dc = dc->Next;
    }
  return NULL;
}

/* num should be greater than or equal to 1 */
int RawDocOutput(query_data *qd, u_long num, FILE *Output)
{
  static last_pos = 0;
  static uchar *c_buffer = 0;
  static int buf_len = -1;
  static uchar *uc_buffer = 0;
  u_long pos, len;
  int ULen;

  FetchDocStart(qd, num, &pos, &len);
  
  if ((int)len > buf_len)
    {
      if (c_buffer)
	{
	  Xfree(c_buffer);
	  Xfree(uc_buffer);
	}
      if (!(c_buffer = Xmalloc(len)))
	return -1;
      if (!(uc_buffer = Xmalloc((int)(qd->td->cth.ratio * 1.01 * 
				      len) + 100)))
	return -1;
      buf_len = len;
    }
  if (last_pos != pos)
    Fseek (qd->td->TextFile, pos, 0);
  Fread (c_buffer, 1, len, qd->td->TextFile);
  last_pos = pos + len;
  DecodeText(qd->cd, c_buffer, len, uc_buffer, &ULen);
  fwrite(uc_buffer, ULen, sizeof(uchar), Output);
  return 0;
}


void StringOut(FILE *Output, char *string, 
	       Bool intvalid, unsigned long intval,
	       Bool floatvalid, double floatval)
{
  char *s;
  for (s = string; *s; s++)
    if (*s == '%' && 
	(*(s+1) == 'n' || *(s+1) == 'w' || *(s+1) == '%'))
      {
	s++;
	switch(*s)
	  {
	  case 'n' :
	    if (intvalid)
	      fprintf(Output, "%lu", intval);
	    else
	      fprintf(Output, "%%n");
	    break;
	  case 'w' :
	    if (floatvalid)
	      fprintf(Output, "%f", floatval);
	    else
	      fprintf(Output, "%%w");
	    break;
	  case '%' :
	    fputc('%', Output);
	  }
      }
    else
      fputc(*s, Output);
}


void HeaderOut(FILE* Output, uchar* UDoc, unsigned long ULen, int heads_length)
{
  int i, space = 1, num = 0;
  for (i=0; i < ULen && num < heads_length; i++)
    {
      char c = UDoc[i];
      if (c == '\02') break;

      if (isspace(c) || c == '\01' || c == '\03')
	{
	  if (!space)
	    {
	      fputc(' ', Output);
	      num++;
	    }
	  space = 1;
	}
      else
	{
	  space = 0;
	  fputc(c, Output);
	  num++;
	}
    }
}


static int ProcessDocs(query_data *qd, int num, Bool verbatim,
			char OutputType, FILE *Output)
{
  int max_buf;
  int DocCount = 0;
  char *doc_sepstr;
  char *para_sepstr;
  char *para_start;
  int heads_length = atoi(GetDefEnv("heads_length", "50"));

  char QueryType = toupper(*GetDefEnv("query","B"));
  if (QueryType == 'A' || QueryType == 'R')
    {
      doc_sepstr = de_escape_string(
			Xstrdup(GetDefEnv("ranked_doc_sepstr", 
			      "---------------------------------- %n %w\\n")));
    }
  else
    {
      doc_sepstr = de_escape_string(
			Xstrdup(GetDefEnv("doc_sepstr", 
			      "---------------------------------- %n\\n")));
    }
  para_sepstr = de_escape_string(
	  Xstrdup(GetDefEnv("para_sepstr", 
			    "\\n######## PARAGRAPH %n ########\\n")));

  para_start = de_escape_string(
	  Xstrdup(GetDefEnv("para_start", 
			    "***** Weight = %w *****\\n")));

  max_buf = atoi(GetDefEnv("buffer", "1048576"));

  do
    {
      uchar *UDoc = NULL;
      unsigned long ULen;
      if (OutputType != 'E')
	if (LoadCompressedText(qd, max_buf))
	  {
	    Message("Unable to load compressed text.");
	    FatalError(1, "This is probably due to lack of memory.");
	  }
      if (OutputType != 'D' && OutputType != 'E')
	{
	  UDoc = GetDocText(qd, &ULen);
	  if (UDoc == NULL)
	    FatalError(1, "UDoc is unexpectedly NULL");
	}
    
      if (!UDoc || PostProc((char*)UDoc, verbatim))
	{
	  switch (OutputType)
	    {
	    case 'C' :
	    case 'S' : break;
	    case 'D' : /* This prints out the docnums string */
	      if (PagerRunning) 
		fprintf(Output,"%7d   %6.4f   %7lu\n", GetDocNum(qd), 
			GetDocWeight(qd), GetDocCompLength(qd));
	      break;
	    case 'H' : /* This prints out the headers of the documents */
	      if (PagerRunning)
		fprintf(Output,"%d ", GetDocNum(qd));
	      HeaderOut(Output, UDoc, ULen, heads_length);
	      if (PagerRunning)
		fputc('\n',Output);
	      break;
#if TREC_MODE
	    case 'E' : /* This prints out the docnums string */
	      if (PagerRunning && trec_ids)
		{
		  long DN, PN = GetDocNum(qd)-1;
		  if (trec_paras)
		    DN = trec_paras[PN];
		  else
		    DN = PN;
		  fprintf(Output,"%-14.14s  %8ld  %10.5f\n", 
			  &trec_ids[DN*14], PN+1, GetDocWeight(qd));
		}
	      break;
#endif
	    case 'T' :
	      {
		int j, para = -1, curr_para = 0;
		int init_para = -1;
		DocEntry *de, *doc_chain = NULL;
		int p_on = 0;
		register char ch = ' ';
		register char lch = '\n';
		if (PagerRunning) 
		  {
		    StringOut(Output, doc_sepstr, 
			      TRUE, GetDocNum(qd),
			      QueryType == 'A' || QueryType == 'R',
			      GetDocWeight(qd));
		  }
		if (qd->id->ifh.InvfLevel == 3)
		  {
		    init_para = FetchInitialParagraph(qd->td, GetDocNum(qd));
		    doc_chain = GetDocChain(qd);
		    para = GetDocNum(qd) - init_para;
		    
		    StringOut(Output, para_sepstr, 
			      TRUE, curr_para+1,
			      FALSE, 0);

		    if ((de = in_chain(0, init_para, doc_chain)))
		      StringOut(Output, para_start, 
				FALSE, 0,
				TRUE, de->Weight);
		      
		    if (doc_chain->DocNum - init_para == 0)
		      p_on = 1;
		  }
		for (j=0; j<ULen; j++)
		  {
		    ch = UDoc[j];
		    switch (ch)
		      {
		      case '\02' : break;
		      case '\01' : ch = '\n'; 
		      case '\03' : 
			p_on = 0;
			curr_para++;
			StringOut(Output, para_sepstr, 
				  TRUE, curr_para+1,
				  FALSE, 0);
			lch = *(strchr(para_sepstr, '\0')-1);
			if ((de = in_chain(curr_para, init_para, doc_chain)))
			  StringOut(Output, para_start, 
				    FALSE, 0,
				    TRUE, de->Weight);
			if (doc_chain && 
			    doc_chain->DocNum - init_para == curr_para)
			  p_on = 1;
			break; 
			default  : 
			  {
			    if (PagerRunning) 
			      {
				fputc(ch, Output);
				if (p_on && isprint(ch))
				  {
				    fputc('\b', Output);
				    fputc('_', Output);
				  }
			      }
				
			    lch = ch;
			  }
		      }
		  }
		if (PagerRunning && lch != '\n') 
		  fputc('\n', Output);
		p_on = 0;
	      }
	    }
	  if (PagerRunning) 
	    fflush(Output);
	}
      DocCount++;
      
    }
  while ( NextDoc(qd) && PagerRunning &&(!Ctrl_C));
  
  FreeTextBuffer(qd);
  Xfree(doc_sepstr);
  Xfree(para_sepstr);
  Xfree(para_start);
  return(DocCount);
}


void output_terminator(FILE *out)
{
  char *terminator = Xstrdup(GetDefEnv("terminator", ""));
  de_escape_string(terminator);
  fputs(terminator, out);
  Xfree(terminator);
}




/* MoreDocs () */
/* Displays all documents in list  DocList. */
/* Documents are fetched, then decompressed and displayed according to the */
/* format implied in  FormString(). */
static void MoreDocs (query_data *qd, char *Query)
{
  int	DocCount = 0;		/* number of actual matches */
  char  OutputType = toupper(*GetDefEnv("mode","t"));
  FILE  *Output;
  
  Ctrl_C = FALSE;

  qd->num_of_ans = qd->DL->num;

  signal(SIGPIPE, SIGPIPE_handler); 
  signal(SIGINT, SIGINT_handler);

  PagerRunning = TRUE;
  if (isatty(fileno(OutFile)) && GetEnv("pager") && 
      OutputType != 'S' && OutputType != 'C')
    {
      char *Pager = GetEnv("pager");
      if (!(Output = popen(Pager,"w")))
	{
	  fprintf(stderr,"Unable to run \"%s\"\n", Pager);
	  return;
	}
    }
  else
    Output = OutFile;

  if (qd->DL->num > 0) 
    {
      if (OutputType == 'C' && !post_proc)
	DocCount = qd->DL->num;
      else
	DocCount = ProcessDocs(qd, qd->DL->num, 
			       BooleanEnv(GetEnv("verbatim"), TRUE),
			       OutputType, Output);
    }
  if (PagerRunning) 
    {
      output_terminator(Output);
      fflush(Output);
    }

  if (isatty(fileno(OutFile)) && GetEnv("pager") &&
      OutputType != 'S' && OutputType != 'C')
    pclose(Output);

  if (qd->DL->num == 0) 
    fprintf (stderr, "No entries correspond to that query.\n");
  else
    if (OutputType == 'C')
      fprintf(stderr, "%d documents match.\n", DocCount);
    else
      fprintf(stderr, "%d documents retrieved.\n", DocCount);

  signal(SIGINT, SIG_DFL);
}


void start_up_stats(query_data *qd, InitQueryTimes iqt)
{
  Clear_Stats();
  if (BooleanEnv(GetEnv("timestats"), 0) ||
      BooleanEnv(GetEnv("briefstats"), 0)) 
    StartUpTimeStats(&iqt);
  
  if (BooleanEnv(GetEnv("diskstats"), 0) ||
      BooleanEnv(GetEnv("briefstats"), 0))
    File_Stats(qd);

  if (BooleanEnv(GetEnv("memstats"), 0) ||
      BooleanEnv(GetEnv("briefstats"), 0))
    MemStats(qd);
  
}


void shut_down_stats(query_data *qd, ProgTime *start, 
		     ProgTime *invf, ProgTime *text)
{
  Clear_Stats();
  if (BooleanEnv(GetEnv("timestats"), 0) ||
      BooleanEnv(GetEnv("briefstats"), 0)) 
    QueryTimeStats(start, invf, text);

  if (BooleanEnv(GetEnv("diskstats"), 0) ||
      BooleanEnv(GetEnv("briefstats"), 0))
    {
      TransFileStats(qd);
      File_Stats(qd);
    }

  if (BooleanEnv(GetEnv("sizestats"), 0))
    TotalSizeStats(qd);
}


void query(void)
{
  ProgTime TotalStartTime, TotalInvfTime, TotalTextTime;
  InitQueryTimes iqt;
  query_data *qd;

  TotalStartTime.RealTime = TotalStartTime.CPUTime = 0;
  TotalInvfTime.RealTime = TotalInvfTime.CPUTime = 0;
  TotalTextTime.RealTime = TotalTextTime.CPUTime = 0;

  qd = InitQuerySystem(GetDefEnv("mgdir","./"), 
		       GetDefEnv("mgname",""),
		       &iqt);
  if (!qd)
    FatalError(1, mg_errorstrs[mg_errno], mg_error_data);
  start_up_stats(qd, iqt);
  

  while(1)
    {
      ProgTime StartTime, InvfTime, TextTime;
      char QueryType;
      char *line;
      ResetFileStats(qd);
      qd->max_mem_in_use = qd->mem_in_use = 0;
      
      qd->tot_hops_taken += qd->hops_taken;
      qd->tot_num_of_ptrs += qd->num_of_ptrs;
      qd->tot_num_of_accum += qd->num_of_accum;
      qd->tot_num_of_terms += qd->num_of_terms;
      qd->tot_num_of_ans += qd->num_of_ans;
      qd->tot_text_idx_lookups += qd->text_idx_lookups;
      qd->hops_taken = qd->num_of_ptrs = 0;
      qd->num_of_accum = qd->num_of_ans = qd->num_of_terms = 0;
      qd->text_idx_lookups = 0;

      Display_Stats(stderr);
      Clear_Stats();
      line = get_query();
      if (!line || Quitting) break;

      GetPostProc(line);

      GetTime(&StartTime);

      FreeQueryDocs(qd);

      QueryType = toupper(*GetDefEnv("query","B"));
      switch (QueryType)
	{
	case 'B' :
	  {
	    char *maxdocs;
	    BooleanQueryInfo bqi;
	    maxdocs = GetDefEnv("maxdocs","all");
	    bqi.MaxDocsToRetrieve = strcmp(maxdocs,"all") ? atoi(maxdocs) : -1;
	    BooleanQuery(qd, line, &bqi);
	    break;
	  }
	case 'A' : 
	case 'R' : 
	  {
	    char *maxdocs;
	    char *maxterms;
	    char *maxaccum;
	    RankedQueryInfo rqi;
	    maxdocs = GetDefEnv("maxdocs","all");
	    maxterms = GetDefEnv("max_terms","all");
	    maxaccum = GetDefEnv("max_accumulators","all");
	    rqi.Sort = BooleanEnv(GetEnv("sorted_terms"), 0);
	    rqi.QueryFreqs = BooleanEnv(GetEnv("qfreq"), 1);
	    rqi.Exact = QueryType == 'R';
	    rqi.MaxDocsToRetrieve = strcmp(maxdocs,"all") ? atoi(maxdocs) : -1;
	    rqi.MaxTerms = strcmp(maxterms,"all") ? atoi(maxterms) : -1;
	    rqi.MaxParasToRetrieve = rqi.MaxDocsToRetrieve;
	    if (qd->id->ifh.InvfLevel == 3 && GetEnv("maxparas"))
	      rqi.MaxParasToRetrieve = atoi(GetEnv("maxparas"));
	    rqi.AccumMethod = toupper(*GetDefEnv("accumulator_method","A"));
	    rqi.MaxAccums = strcmp(maxaccum,"all") ? atoi(maxaccum) : -1;
	    rqi.HashTblSize = IntEnv(GetEnv("hash_tbl_size"), 1000);
	    rqi.StopAtMaxAccum = BooleanEnv(GetEnv("stop_at_max_accum"), 0);
	    rqi.skip_dump = GetEnv("skip_dump");
	    RankedQuery(qd, line , &rqi); 
	    break;
	  }
	case 'D' :
	  {
	    DocnumsQuery(qd, line);
	    break;
	  }
	}
      
      GetTime(&InvfTime);
	  
      if (qd->DL)
	MoreDocs(qd, line);

      GetTime(&TextTime);

      if (BooleanEnv(GetEnv("timestats"), 0) ||
	  BooleanEnv(GetEnv("briefstats"), 0)) 
	QueryTimeStats(&StartTime, &InvfTime, &TextTime);

      if (BooleanEnv(GetEnv("diskstats"), 0) ||
	  BooleanEnv(GetEnv("briefstats"), 0))
	File_Stats(qd);

      if (BooleanEnv(GetEnv("memstats"), 0) ||
	  BooleanEnv(GetEnv("briefstats"), 0))
	MemStats(qd);

      if (BooleanEnv(GetEnv("sizestats"), 0))
	SizeStats(qd);

      TotalInvfTime.RealTime += InvfTime.RealTime - StartTime.RealTime;
      TotalInvfTime.CPUTime += InvfTime.CPUTime - StartTime.CPUTime;
      TotalTextTime.RealTime += TextTime.RealTime - StartTime.RealTime;
      TotalTextTime.CPUTime += TextTime.CPUTime - StartTime.CPUTime;
    }

  if (isatty(fileno(InFile)) && !Quitting)  
    fprintf(stderr, "\n");

  shut_down_stats(qd, &TotalStartTime, &TotalInvfTime, &TotalTextTime);

  Display_Stats(stderr);

}


void search_for_collection(char *name)
{
  char *dir = GetDefEnv("mgdir","./");
  char buffer[512];
  struct stat stat_buf;
  if (strrchr(dir,'/') && *(strrchr(dir,'/')+1) != '\0')
    {
      sprintf(buffer, "%s/", dir);
      SetEnv("mgdir", buffer, NULL);
      dir = GetEnv("mgdir");
    }

  /* Look in the current directory first */
  if (stat(name, &stat_buf) != -1)
    {
      if ((stat_buf.st_mode&S_IFDIR) != 0)
	{
	  /* The name is a directory */
	  sprintf(buffer, "%s/%s", name, name);
	  SetEnv("mgname", buffer, NULL);
	  SetEnv("mgdir", "./", NULL);
	  return;
	}
    }

  sprintf(buffer, "%s.text", name);
  if (stat(buffer, &stat_buf) != -1)
    {
      if ((stat_buf.st_mode&S_IFREG) != 0)
	{
	  /* The name is a directory */
	  SetEnv("mgname", name, NULL);
	  SetEnv("mgdir", "./", NULL);
	  return;
	}
    }

  sprintf(buffer, "%s%s", dir, name);
  if (stat(buffer, &stat_buf) != -1)
    {
      if ((stat_buf.st_mode&S_IFDIR) != 0)
	{
	  /* The name is a directory */
	  sprintf(buffer, "%s/%s", name, name);
	  SetEnv("mgname", buffer, NULL);
	  return;
	}
    }
  SetEnv("mgname", name, NULL);
}

/* main () */
/* Initialises global variables based on command line switches, and opens */
/* files.  Then calls  query ()  to perform the querying. */
void main (int argc, char **argv)
{
  ProgTime StartTime;
  int decomp = 0;
  int	ch;
  
  msg_prefix = argv[0];
  GetTime(&StartTime);

  /* Initialise the environment with default values */

  InitEnv();

  read_mgrc_file();

  OutFile = stdout;
  InFile = stdin;

  opterr = 0;
  while ((ch = getopt (argc, argv, "Df:d:h")) != -1)
    switch (ch) 
      {
      case 'f' :			
	SetEnv("mgname", optarg, NULL);
	break;
      case 'd' :
	SetEnv("mgdir", optarg, NULL);
	break;
      case 'D' :
	decomp = 1;
	break;
      case 'h' :
      case '?' :
	fprintf (stderr, "usage: %s [-D] [-f base name of collection]"
		 "[-d data directory] [collection]\n", argv [0]);
	exit (1);
      }

  PushEnv();

  if (decomp == 0)
    {

      Init_ReadLine();

      /* write a first prompt, let the user start thinking */
      if (!BooleanEnv(GetEnv("expert"), 0) && isatty(fileno(InFile)))
	{
	  fprintf(stderr,"\n\n\t     FULL TEXT RETRIEVAL QUERY PROGRAM\n");
	  fprintf(stderr,"%24s%s\n\n", "", *"21 Mar 1994"=='%'?__DATE__:"21 Mar 1994");
	  fprintf(stderr,"\n");
	  fprintf(stderr,"  mgquery version " VERSION ", Copyright (C) 1994 Neil Sharman\n");
	  fprintf(stderr,"  mgquery comes with ABSOLUTELY NO WARRANTY; for details type `.warranty'\n");
	  fprintf(stderr,"  This is free software, and you are welcome to redistribute it\n");
	  fprintf(stderr,"  under certain conditions; type `.conditions' for details.\n");
	  fprintf(stderr,"\n");
	}
    }
  if (optind < argc)
    search_for_collection(argv[optind]);

  if (decomp == 0)
    {
      query();
    }
  else
    {
      int i;
      InitQueryTimes iqt;
      query_data *qd;

      qd = InitQuerySystem(GetDefEnv("mgdir","./"), 
			   GetDefEnv("mgname",""),
			   &iqt);
      if (!qd)
	FatalError(1, mg_errorstrs[mg_errno], mg_error_data);
      
      
      start_up_stats(qd, iqt);
  
      Display_Stats(stderr);
      for (i=0; i < qd->td->cth.num_of_docs; i++)
	{
	  RawDocOutput(qd, i+1, stdout);
	  putc('\2', stdout);
	}
      Message("%s", ElapsedTime(&StartTime, NULL));

      FinishQuerySystem(qd);
    }
  
  UninitEnv();
  
}
