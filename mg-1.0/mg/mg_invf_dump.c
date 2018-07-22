/**************************************************************************
 *
 * mg_invf_dump.c -- Program to dump uot an inverted fil
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
 *       @(#)mg_invf_dump.c	1.6 16 Mar 1994
 *
 **************************************************************************/

#include <stdio.h>
#include <string.h>

#include "sysfuncs.h"
#include "messages.h"
#include "timing.h"
#include "bitio_m.h"
#include "bitio_m_stdio.h"
#include "bitio_gen.h"

#include "mg_files.h"
#include "locallib.h"
#include "words.h"
#include "invf.h"

extern unsigned long S_btg;

char *SCCS_Id_invf_dump = "@(#)mg_invf_dump.c	1.6 16 Mar 1994";


static char pathname [256];
 
static void process_files(char *filename);

int binary = 0;
int word_counts = 0;


void main(int argc, char **argv)
{
  ProgTime start;
  char *dir_name, *file_name = "";
  int ch;
  msg_prefix = argv[0];
  dir_name = getenv("MGDATA");
  strcpy(pathname, dir_name ? dir_name : ".");
  opterr = 0;
  msg_prefix = argv[0];
  while ((ch = getopt (argc, argv, "hbwf:d:")) != -1)
    switch (ch) 
      {
      case 'f' :                        /* input file */
        file_name = optarg;
        break;
      case 'd' :
        strcpy(pathname, optarg);
        break;
      case 'b':
	binary = 1;
	break;
      case 'w':
	word_counts = 1;
	break;
      case 'h':
      case '?':
        printf ("usage: %s [-h] [-b] [-f input_file]"
                "[-d data directory]\n", argv [0]);
        exit(1);
      }
  GetTime(&start);
  process_files(file_name);
  Message("%s\n", ElapsedTime(&start, NULL));
  exit(0);
}

static void process_files(char *name)
{
  unsigned long N, k;
  FILE *dict;
  FILE *invf;
  struct invf_dict_header idh;
  struct invf_file_header ifh;


  dict = open_file(name, INVF_DICT_SUFFIX, "r", MAGIC_STEM_BUILD, MG_ABORT);

  fread((char*)&idh, sizeof(idh), 1, dict);

  if (!(invf = open_file(name, INVF_SUFFIX ".ORG", "r", MAGIC_INVF, MG_CONTINUE)))
    invf = open_file(name, INVF_SUFFIX , "r", MAGIC_INVF, MG_ABORT);
    
  fread((char*)&ifh, sizeof(ifh), 1, invf);
  if (ifh.skip_mode != 0)
    FatalError(1,"The invf file contains skips. Unable to dump.");

  DECODE_START(invf)
  N = idh.num_of_docs;
  if (binary)
    {
      fwrite((char*)&N, sizeof(N), 1, stdout);
      fwrite((char*)&ifh.no_of_words, sizeof(ifh.no_of_words), 1, stdout);
    }
  else
    printf("%ld %ld\n", N, ifh.no_of_words);
  for (k=0; k < ifh.no_of_words; k++)
    {
      int i, blk, doc;
      register unsigned long suff;
      unsigned long fcnt, wcnt;
      char dummy2[MAXSTEMLEN+1];

      fgetc(dict);
      suff = fgetc(dict);
      fread(dummy2, sizeof(char), suff, dict);
      fread((char*)&fcnt, sizeof(fcnt), 1, dict);
      fread((char*)&wcnt, sizeof(wcnt), 1, dict);


      if (binary)
	fwrite((char*)&fcnt, sizeof(fcnt), 1, stdout);
      else
	printf("%ld\n", fcnt);
      blk = BIO_Bblock_Init (N, fcnt);
      for (doc=i=0; i<fcnt; i++)
	{
	  int num;
          BBLOCK_DECODE(num, blk);
	  doc += num;
	  if (binary)
	    fwrite((char*)&doc, sizeof(doc), 1, stdout);
	  else
	    printf(" %d", doc);
          if (ifh.InvfLevel >= 2)
	    {
	      int count;
              GAMMA_DECODE(count);
	      if (word_counts)
		if (binary)
		  fwrite((char*)&count, sizeof(count), 1, stdout);
		else
		  printf(" %d", count);
	    }
	  if (!binary)
	    putchar('\n');
	}
      while (__btg)
	DECODE_BIT;
    }
  DECODE_DONE
}
