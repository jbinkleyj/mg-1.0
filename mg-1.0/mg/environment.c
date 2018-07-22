/**************************************************************************
 *
 * environment.c -- mgquery environment functions
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
 *       @(#)environment.c	1.10 21 Mar 1994
 *
 **************************************************************************/

char *SCCS_Id_environment = "@(#)environment.c	1.10 21 Mar 1994";

#include "config.h"


#include <stdio.h>
#ifdef SUNOS5
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>

#include "memlib.h"
#include "sysfuncs.h"
#include "locallib.h"

#include "environment.h"

#if defined(SUNOS4) 
char tolower(char);
char *getenv(const char *);
#endif


typedef struct
{
  char *Name, *Data;
  char *(*Constraint)(char *Old, char *New);
} EEntry;

typedef struct Env
{
  EEntry *Environment;
  int NumEnv;
  struct Env *Next;
} Env;


char *ConstraintErrorStr;

static Env Base = {NULL, 0, NULL};


/*
 * Add name and data to the environment (data may be NULL)
 * All names are converted to lower case
 * returns 0 on success, -1 on memory failure or -2 on constraint failure
 */
int SetEnv(char *name, char *data, char *(*Constraint)(char*,char*))
{
  int i;
  EEntry *newenv, *env = Base.Environment;

  if (data && *data == '\0')
    data = NULL;

  /* search for name in the environment */
  for (i=0; i<Base.NumEnv && strcasecmp(name, env->Name); i++, env++);

  /* if not found the increase the environment size */
  if (i >= Base.NumEnv)
    {
      if (Constraint)
	{
	  data = Constraint(NULL, data);
	  if (!data) return(-2);
	}
      /* allocate memory for the new environment */
      newenv = Base.Environment ? 
	Xrealloc(Base.Environment, sizeof(EEntry)*(Base.NumEnv+1)) :
	Xmalloc(sizeof(EEntry)*(Base.NumEnv+1));
      if (!newenv) return(-1);
      Base.Environment = newenv;
      env = &newenv[Base.NumEnv];
      env->Data = NULL;
      env->Name = NULL;
      env->Constraint = Constraint;
      env->Name = Xstrdup(name);
      if (!env->Name) return(-1);
      Base.NumEnv++;
      for (i=0; env->Name[i]; i++) 
	env->Name[i] = tolower(env->Name[i]);
    }
  if (env->Constraint)
    {
      data = env->Constraint(env->Data, data);
      if (!data) return(-2);
    }
  if (env->Data)
    Xfree(env->Data);
  env->Data = NULL;
  if (data && *data != '\0')
    {
      env->Data = Xstrdup(data);
      if (!env->Data) return(-1);
    }
  return(0);
}



/*
 * Returns the data associated with name. If Data = NULL then this routine
 * returns a pointer to "". If the specified name does not exist then this 
 * routine returns NULL.
 */
char *GetEnv(char *name)
{
  int i;
  EEntry *env = Base.Environment;
  /* search for name in the environment */
  for (i=0; i<Base.NumEnv && strcasecmp(name, env->Name); i++, env++);
  
  if (i >= Base.NumEnv) return(NULL);

  return( env->Data ? env->Data : "" );
}

/*
 * Returns the data associated with name. If Data = NULL then this routine
 * returns a pointer to "". If the specified name does not exist then this 
 * routine returns default.
 */
char *GetDefEnv(char *name, char *def)
{
  char *data = GetEnv(name);
  return ( data ? data : def);
}



/*
 * This function pushes the environment on to a stack and duplicated it in 
 * the current stack.
 *
 */
int PushEnv(void)
{
  Env *env = Xmalloc(sizeof(Env));
  int i;
  *env = Base;
  Base.Next = env;
  Base.Environment = NULL;
  Base.NumEnv = 0;
  for (i = 0; i<env->NumEnv; i++)
    if (SetEnv(env->Environment[i].Name, env->Environment[i].Data,
	       env->Environment[i].Constraint))
      return(-1);
  return(0);
}

int PopEnv(void)
{
  Env *env;
  int i;
  if (!Base.Next) return(-1);
  for (i=0; i<Base.NumEnv; i++)
    {
      Xfree(Base.Environment[i].Name);
      if (Base.Environment[i].Data)
	Xfree(Base.Environment[i].Data);
    }
  if (Base.Environment)
    Xfree(Base.Environment);
  env = Base.Next;
  Base = *env;
  Xfree(env);
  return(0);
}


/*
 * Count the number of environments on the stack, including the current one.
 *
 */
int EnvStackHeight(void)
{
  int i = 1;
  Env *env = &Base;
  while (env->Next)
    {
      i++;
      env = env->Next;
    }
  return(i);
}

/*
 * Delete environment variable name. If name does not exist it returns -1
 * otherwise it returns 0
 */
int UnsetEnv(char *name, int Force)
{
  int i;
  EEntry *env = Base.Environment;
  /* search for name in the environment */
  for (i=0; i<Base.NumEnv && strcasecmp(name, env->Name); i++, env++);
  if (i >= Base.NumEnv || 
      (Base.Environment[i].Constraint && !Force)) return(-1);
  Xfree(Base.Environment[i].Name);
  if (Base.Environment[i].Data)
    Xfree(Base.Environment[i].Data);
  for (i++; i<Base.NumEnv; i++)
    Base.Environment[i-1] = Base.Environment[i];
  Base.NumEnv--;
  if (Base.NumEnv == 0)
    {
      Xfree(Base.Environment);
      Base.Environment = NULL;
    }
  return( 0 );
}


/*
 * Turn a string containing \ escapes into real characters
 * Note this destroys the string s in the process
 */
char *de_escape_string(char *s)
{
  char *org = s;
  char *d = s;
  while(*s)
    {
      if (*s == '\\')
	{
	  s++;
	  switch (*s++)
	    {
	    case '\0' : *d++ = '\\'; s--; break;
	    case '\\' : *d++ = '\\'; break;
	    case 'b'  : *d++ = '\b'; break;
	    case 'f'  : *d++ = '\f'; break;
	    case 'n'  : *d++ = '\n'; break;
	    case 'r'  : *d++ = '\r'; break;
	    case 't'  : *d++ = '\r'; break;
	    case '\"' : *d++ = '\"'; break;
	    case '\'' : *d++ = '\''; break;
	    case 'x'  : 
	      {
		int num = 0;
		if (isxdigit(*s))
		  {
		    char ch = toupper(*s++);
		    num = ch <='9' ? ch-'0' : ch - 'A' + 10;
		  }
		if (isxdigit(*s))
		  {
		    char ch = toupper(*s++);
		    num = (num << 4) + (ch <='9' ? ch-'0' : ch - 'A' + 10);
		  }
		*d++ = num;
		break;
	      }
	    case '0' :  case '1' :  case '2' :  case '3' :
	    case '4' :  case '5' :  case '6' :  case '7' :
	      {
		int num = *(s-1) - '0';
		if (*s >= '0' && *s <= '7')
		  num = (num << 3) + *s++ - '0';
		if (*s >= '0' && *s <= '7')
		  num = (num << 3) + *s++ - '0';
		*d++ = num;
		break;
	      }
	    }
	}
      else
	*d++ = *s++;
    }
  *d = '\0';
  return(org);
}






/*
 * Returns the name of environment variable number i or NULL if i is greater 
 * than or equal to the number of environment variables. Names start from
 * zero. 
 */
char *GetEnvName(int i)
{
  return( i<Base.NumEnv ? Base.Environment[i].Name : NULL );
}

static char *BooleanStrs[] = {"false", "true", "no", "yes", "off", "on"};

static char *BooleanCons(char *Old, char *New)
{
  int i;
  int old = -1, new = -1;
  if (Old)
    for (i=0; i<sizeof(BooleanStrs)/sizeof(char*); i++)
      if (!strcasecmp(Old, BooleanStrs[i]))
	{
	  old = i;
	  break;
	}
  if (New)
    for (i=0; i<sizeof(BooleanStrs)/sizeof(char*); i++)
      if (!strcasecmp(New, BooleanStrs[i]))
	{
	  new = i;
	  break;
	}
  if (new >=0 ) return(New);
  if (!New) return(BooleanStrs[Old ? old ^ 1 : 0]);
  ConstraintErrorStr = "Invalid argument [true|false|yes|no|on|off] required";
  return(NULL);
}

/*
 * returns 0 or 1  for a boolean string
 * or default on a error
 */
int BooleanEnv(char *data, int def)
{
  int i;
  if (!data) return(def);
  for (i=0; i<sizeof(BooleanStrs)/sizeof(char*); i++)
    if (!strcasecmp(data, BooleanStrs[i]))
      return(i & 1);
  return(def);
}

/*
 * returns the value for a integer string
 * or default on a error
 */
long IntEnv(char *data, long def)
{
  long val;
  char *ptr;
  if (!data) return(def);
  val = strtol(data, &ptr, 10);
  return *ptr ? def : val;
}

static char *NumberCmp(char *num, int min, int max)
{
  char *err;
  int val;
  static char Err[100];
  ConstraintErrorStr = "Not a valid number";
  if (!num) return(NULL);
  val = strtol(num, &err, 10);
  if (*err) return(NULL);
  sprintf(Err, "Not in legal range [%d <= num <= %d]",min, max);
  ConstraintErrorStr = Err;
  if (val < min || val > max) return(NULL);
  return(num);
}

/*ARGSUSED*/
static char *MaxDocsCons(char *Old, char *New)
{
  if (!strcasecmp(New,"all")) return(New);
  return(NumberCmp(New, 1, ((unsigned)(~0)) >> 1));
}

/*ARGSUSED */
static char *MaxTermsCons(char *Old, char *New)
{
  if (!strcasecmp(New,"all")) return(New);
  return(NumberCmp(New, 1, ((unsigned)(~0)) >> 1));
}

/* ARGSUSED */
static char *BufferCons(char *Old, char *New)
{
  return(NumberCmp(New, 0, 16*1024*1024));
}

/* ARGSUSED */
static char *MaxNodesCons(char *Old, char *New)
{
  if (!strcasecmp(New,"all")) return(New);
  return(NumberCmp(New, 8, 256*1024*1024));
}

/* ARGSUSED */
static char *MaxHashCons(char *Old, char *New)
{
  return(NumberCmp(New, 8, 256*1024*1024));
}

/* ARGSUSED */
static char *MaxParasCons(char *Old, char *New)
{
  return(NumberCmp(New, 1, 256*1024*1024));
}

/* ARGSUSED */
static char *MaxHeadsCons(char *Old, char *New)
{
  return(NumberCmp(New, 1, 1000));
}


/*
 * Makes sure that New is a valid query type
 *
 */
/* ARGSUSED */
static char *QueryCons(char *Old, char *New)
{
  static char *QueryStrs[] = {
    "boolean", "ranked", 
    "docnums", "approx-ranked"};
  int i;
  int new = -1;
  if (New)
    for (i=0; i<sizeof(QueryStrs)/sizeof(char*); i++)
      if (!strcasecmp(New, QueryStrs[i]))
	{
	  new = i;
	  break;
	}
  if (new >=0 ) return(New);
  ConstraintErrorStr = "Invalid argument [boolean|ranked|docnums|approx-ranked] required";
  return(NULL);
}

/*
 * Makes sure that New is a valid accum type
 *
 */
/* ARGSUSED */
static char *AccumCons(char *Old, char *New)
{
  static char *AccumStrs[] = {
    "array", "splay_tree", "hash_table", "list"};
  int i;
  int new = -1;
  if (New)
    for (i=0; i<sizeof(AccumStrs)/sizeof(char*); i++)
      if (!strcasecmp(New, AccumStrs[i]))
	{
	  new = i;
	  break;
	}
  if (new >=0 ) return(New);
  ConstraintErrorStr = "Invalid argument "
    "[array|splay_tree|hash_table|list] required";
  return(NULL);
}



/*
 * Makes sure that New is a valid output type type
 *
 */
/* ARGSUSED */
static char *OutputTypeCons(char *Old, char *New)
{
  static char *OutputTypeStrs[] = {"text", "silent", "docnums", 
				   "count", "heads"
#ifdef TREC_MODE
, "extras_for_trec"
#endif
};

  int i;
  int new = -1;
  if (New)
    for (i=0; i<sizeof(OutputTypeStrs)/sizeof(char*); i++)
      if (!strcasecmp(New, OutputTypeStrs[i]))
	{
	  new = i;
	  break;
	}
  if (new >=0 ) return(New);
  ConstraintErrorStr = "Invalid argument [text|silent|docnums|heads|count"
#ifdef TREC_MODE
"|extras_for_trec"
#endif
    "] required";
  return(NULL);
}


/*
 * This initialises certain environment variables
 *
 */
void InitEnv(void)
{
  SetEnv("briefstats", "off", BooleanCons);
  SetEnv("diskstats", "off", BooleanCons);
  SetEnv("expert", "false", BooleanCons);
  SetEnv("mgdir", getenv("MGDATA")? getenv("MGDATA") : ".", NULL);
  SetEnv("mgname", "", NULL);
  SetEnv("maxdocs", "all", MaxDocsCons);
  SetEnv("memstats", "off", BooleanCons);
  SetEnv("mode", "text", OutputTypeCons);
  SetEnv("pager", getenv("PAGER") ? getenv("PAGER") : "more", NULL);
  SetEnv("qfreq", "true", BooleanCons);
  SetEnv("query", "boolean", QueryCons);
  SetEnv("sizestats", "off", BooleanCons);
  SetEnv("timestats", "off", BooleanCons);
  SetEnv("verbatim", "off", BooleanCons);
  SetEnv("sorted_terms", "on", BooleanCons);
  SetEnv("accumulator_method", "array", AccumCons);
  SetEnv("stop_at_max_accum", "off", BooleanCons);
  SetEnv("buffer", "1048576", BufferCons);
  SetEnv("max_accumulators", "50000", MaxNodesCons);
  SetEnv("max_terms", "all", MaxTermsCons);
  SetEnv("maxparas", "1000", MaxParasCons);
  SetEnv("hash_tbl_size", "1000", MaxHashCons);
  SetEnv("skip_dump", "skips.%d", NULL);
  SetEnv("ranked_doc_sepstr", "---------------------------------- %n %w\\n", NULL);
  SetEnv("doc_sepstr", "---------------------------------- %n\\n", NULL);
  SetEnv("para_sepstr", "\\n######## PARAGRAPH %n ########\\n", NULL);
  SetEnv("para_start", "***** Weight = %w *****\\n", NULL);
  SetEnv("terminator", "", NULL);
  SetEnv("heads_length", "50", MaxHeadsCons);
}


void UninitEnv(void)
{
  char *name;
  while (PopEnv() == 0);
  while((name = GetEnvName(0)) != 0)
    UnsetEnv(name, 1); 
}
