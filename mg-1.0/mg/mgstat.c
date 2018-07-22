/**************************************************************************
 *
 * mgstat.c -- Program to generate statistics on a text collection
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
 *       @(#)mgstat.c	1.7 16 Mar 1994
 *
 **************************************************************************/

#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "mg_files.h"
#include "sysfuncs.h"
#include "locallib.h"
#include "mg.h"
#include "words.h"
#include "invf.h"
#include "text.h"


char *SCCS_Id_mg_size = "@(#)mgstat.c	1.7 16 Mar 1994";

char *search_for_collection(char *name);
Bool process_file(char *, char *, int);
void ProcessDict(char *);
void ProcessStem(char *);

static unsigned long inputbytes = 0;
static unsigned long total = 0;





void main(int argc, char **argv)
{
  unsigned long sub_total;
  Bool fast;
  char *file_name = "";
  int ch;
  int exact = 0;
  opterr = 0;
  while ((ch = getopt (argc, argv, "Ehf:d:")) != -1)
    switch (ch) 
      {
      case 'E' :
	exact = 1;
	break;
      case 'f' :			/* input file */
	file_name = optarg;
	break;
      case 'd' :
	set_basepath(optarg);
	break;
      case 'h' :
      case '?' :
	printf ("usage: %s [-f input_file]"
		"[-d data directory] [-h]\n", argv [0]);
	exit(1);
      }

  if (optind < argc)
    file_name = search_for_collection(argv[optind]);

  ProcessDict(file_name);
  ProcessStem(file_name);

  {
    char Name[256];
    char *s;
    sprintf(Name, "%s/%s", get_basepath(), file_name);
    s = strrchr(Name, '/');
    if (s)
      *s = '\0';
    printf("\nThe collection is in \"%s\"\n", Name);
  }


  printf("\n\t\tFiles required by mgquery\n");
  process_file(file_name, TEXT_SUFFIX, exact);
  process_file(file_name, INVF_SUFFIX, exact);
  process_file(file_name, TEXT_IDX_WGT_SUFFIX, exact);
  process_file(file_name, APPROX_WEIGHTS_SUFFIX, exact);
  process_file(file_name, INVF_DICT_BLOCKED_SUFFIX, exact);
  fast = process_file(file_name, TEXT_DICT_FAST_SUFFIX, exact);
  if (!fast)
    {
      process_file(file_name, TEXT_DICT_SUFFIX, exact);
      process_file(file_name, TEXT_DICT_AUX_SUFFIX, exact);
    }


  process_file(NULL, "SUB TOTAL", exact);
  sub_total = total;
  total = 0;

  printf("\n\t\tFiles NOT required by mgquery\n");
  if (fast)
    {
      process_file(file_name, TEXT_DICT_SUFFIX, exact);
      process_file(file_name, TEXT_DICT_AUX_SUFFIX, exact);
    }
  process_file(file_name, INVF_DICT_SUFFIX, exact);
  process_file(file_name, INVF_IDX_SUFFIX, exact);
  process_file(file_name, TEXT_STATS_DICT_SUFFIX, exact);
  process_file(file_name, TEXT_IDX_SUFFIX, exact);
  process_file(file_name, WEIGHTS_SUFFIX, exact);
  process_file(file_name, INVF_CHUNK_SUFFIX, exact);
  process_file(file_name, INVF_CHUNK_TRANS_SUFFIX, exact);
  process_file(file_name, INVF_DICT_HASH_SUFFIX, exact);
  process_file(file_name, INVF_PARAGRAPH_SUFFIX, exact);
  process_file(NULL, "SUB TOTAL", exact);
  total += sub_total;
  printf("\n");
  process_file(NULL, "TOTAL", exact);
  exit(0);
}



char *search_for_collection(char *name)
{
  char *dir = get_basepath();
  static char buffer[512];
  struct stat stat_buf;

  /* Look in the current directory first */
  if (stat(name, &stat_buf) != -1)
    {
      if ((stat_buf.st_mode&S_IFDIR) != 0)
	{
	  /* The name is a directory */
	  sprintf(buffer, "%s/%s", name, name);
	  set_basepath(".");
	  return buffer;
	}
    }

  sprintf(buffer, "%s.text", name);
  if (stat(buffer, &stat_buf) != -1)
    {
      if ((stat_buf.st_mode&S_IFREG) != 0)
	{
	  /* The name is a directory */
	  set_basepath(".");
	  return name;
	}
    }
  sprintf(buffer, "%s/%s", dir, name);
  if (stat(buffer, &stat_buf) != -1)
    {
      if ((stat_buf.st_mode&S_IFDIR) != 0)
	{
	  /* The name is a directory */
	  sprintf(buffer, "%s/%s", name, name);
	  return buffer;
	}
    }
  return name;
}






void ProcessDict(char *name)
{
  FILE *f;
  compression_dict_header cdh;
  compressed_text_header cth;

  if (!(f = open_file(name, TEXT_DICT_SUFFIX, "r", MAGIC_DICT, MG_MESSAGE)))
    return;
  Read_cdh(f, &cdh, NULL, NULL);
  fclose(f);

  if (!(f = open_file(name, TEXT_SUFFIX, "r", MAGIC_TEXT, MG_MESSAGE)))
    return;
  fread((char*)&cth, sizeof(cth), 1, f);
  fclose(f);

  inputbytes = cth.num_of_bytes;
  printf("Input bytes                        : %10lu, %8.2f Mbyte\n",
	 cth.num_of_bytes, (double)cth.num_of_bytes/1024/1024);
  printf("Documents                          : %10lu\n", cth.num_of_docs);
  printf("Words in collection [dict]         : %10lu\n", cth.num_of_words);
  printf("Longest doc in collection [dict]   : %10lu characters\n",
	 cth.length_of_longest_doc);
  printf("Maximum ratio                      : %10.2f\n", cth.ratio);

  printf("Words in dict                      : %10lu\n", cdh.num_words[1]);
  printf("Non-words in dict                  : %10lu\n", cdh.num_words[0]);
  printf("Total chars of distinct words      : %10lu\n", cdh.num_word_chars[1]);
  printf("Total chars of distinct non-words  : %10lu\n", cdh.num_word_chars[0]);
}








void ProcessStem(char *name)
{
  FILE *f;
  struct invf_dict_header idh;

  if (!(f = open_file(name, INVF_DICT_SUFFIX, "r",
		      MAGIC_STEM_BUILD, MG_MESSAGE)))
    return;
  fread((char*)&idh, sizeof(idh), 1, f);
  printf("Words in collection [stem]         : %10ld\n", idh.num_of_words);
  printf("Words in stem                      : %10ld\n", idh.dict_size);
  printf("Indexed fragments                  : %10ld\n",idh.num_of_docs);
  printf("Total chars of stem words          : %10ld\n", idh.total_bytes);
  fclose(f);
}









Bool process_file(char *name, char *ext, int exact)
{
  static double scale = 0;
  static char *units;
  struct stat buf;
  if (scale == 0)
    {
      scale = inputbytes > 10*1000*1000 ? 1024*1024 : 1024;
      units = scale == 1024 ? "Kb" : "Mb";
    }
  if (name)
    {
      char Name[256];
      sprintf(Name, "%s/%s%s", get_basepath(), name, ext);
      if (!stat(Name, &buf))
	{
	  char fname[256];
	  char *nam = strrchr(name, '/');
	  nam = nam ? nam+1 : name;
	  sprintf(fname, "%s%s", nam, ext);
	  
	  if (exact)
	    printf("%s%*s: %10ld bytes   %7.3f%%\n", fname, 
		 35 - (int)strlen(fname), "",
		 buf.st_size,
		 100.0*buf.st_size/inputbytes);
	  else
	    printf("%s%*s: %8.2f %s   %7.3f%%\n", fname, 
		   35 - (int)strlen(fname), "",
		   buf.st_size/scale,
		   units, 
		   100.0*buf.st_size/inputbytes);
	  total += buf.st_size;
	  return TRUE;
	}
      else
	return FALSE;
    }
  else
    if (exact)
      printf("%-34s : %10ld bytes   %7.3f%%\n", ext, 
	     total, 
	     100.0*total/inputbytes);
    else
      printf("%-34s : %8.2f %s   %7.3f%%\n", ext, 
	     total/scale, units,
	     100.0*total/inputbytes);
  return TRUE;
}


