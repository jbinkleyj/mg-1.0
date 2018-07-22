/**************************************************************************
 *
 * mg_weights_build.c -- Program to build the document weights file
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
 *       @(#)mg_weights_build.c	1.6 16 Mar 1994
 *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "sysfuncs.h"
#include "memlib.h"
#include "messages.h"
#include "misc.h"
#include "bitio_gen.h"
#include "bitio_m.h"
#include "bitio_m_stdio.h"
#include "timing.h"

#include "mg_files.h"
#include "locallib.h"
#include "invf.h"
#include "text.h"
#include "words.h"

#define MAXBITS (sizeof(unsigned long) * 8)

char *SCCS_Id_make_weights = "@(#)mg_weights_build.c	1.6 16 Mar 1994";

unsigned char bits = 8;
static char *file_name = "";
static unsigned long NumPara = 0;

unsigned long get_NumPara(void);
void GenerateWeights(void);
void Make_weight_approx(void);
void Make_text_idx_wgt(void);


void main(int argc, char **argv)
{
  ProgTime StartTime;
  int ch;
  opterr = 0;
  msg_prefix = argv[0];
  while ((ch = getopt (argc, argv, "f:d:b:sh")) != -1)
    switch (ch) 
      {
      case 'f' :			/* input file */
	file_name = optarg;
	break;
      case 'd' :
	set_basepath(optarg);
	break;
      case 'b' :
	bits = atoi(optarg);
	if ( bits > 32)
	  {
	    fprintf(stderr,"b may only take values 0-32\n");
	    exit(1);
	  }
	break;
      case 'h' :
      case '?' :
	printf ("usage: %s [-f input_file]"
		"[-d data directory] [-b bits] [-s] [-h]\n", argv [0]);
	exit(1);
      }
  GetTime(&StartTime);

  GenerateWeights();

  Make_weight_approx();

  Make_text_idx_wgt();

  Message("%s", ElapsedTime(&StartTime, NULL));
  exit(0);
}




unsigned long get_NumPara(void)
{
  struct invf_dict_header idh;
  FILE *invf_dict;
  if (NumPara)
    return(NumPara);
  invf_dict = open_file(file_name, INVF_DICT_SUFFIX, "r", MAGIC_STEM_BUILD,
			MG_ABORT);
  fread((char*)&idh, sizeof(idh), 1, invf_dict);
  fclose(invf_dict);
  NumPara = idh.num_of_docs;
  return NumPara;
}







void GenerateWeights(void)
{
  FILE *dict, *invf, *f, *idx;
  struct invf_dict_header idh;
  struct invf_file_header ifh;
  int i;
  double logN;
  float *DocWeights;

  get_NumPara();

  if ((f = open_file(file_name, WEIGHTS_SUFFIX, "r", MAGIC_WGHT,
		     MG_CONTINUE)) != NULL)
    {
      fclose(f);
      return;
    }
	
  logN = log((double)NumPara);

  Message("The file \"%s.weight\" does not exist.", file_name);
  Message("Building the weight data from the file \"%s.invf\".", file_name);

  if (!(DocWeights = Xmalloc(sizeof(float) * (NumPara+1))))
    FatalError(1, "No memory for doc weights");
#ifdef SUNOS5
  memset(DocWeights, 0, sizeof(float) * (NumPara+1));
#else
  bzero((char*)DocWeights, sizeof(float) * (NumPara+1));
#endif

  dict = open_file(file_name, INVF_DICT_SUFFIX, "r", MAGIC_STEM_BUILD,
		   MG_ABORT);
  fread((char*)&idh, sizeof(idh), 1, dict);

  idx = open_file(file_name, INVF_IDX_SUFFIX, "r", MAGIC_INVI,
		   MG_ABORT);

  invf = open_file(file_name, INVF_SUFFIX, "r", MAGIC_INVF,
		   MG_ABORT);
  fread((char*)&ifh, sizeof(ifh), 1, invf);
  if (ifh.skip_mode != 0)
    FatalError(0, "Can\'t make weights file from a skipped inverted file.");
  if (ifh.InvfLevel == 1)
    FatalError(0, "Can\'t make weights file from level 1 inverted file.");

  DECODE_START(invf)

  for (i=0; i<ifh.no_of_words; i++)
    {
      uchar dummy1, dummy2[MAXSTEMLEN+1];
      unsigned long fcnt, wcnt, blk, CurrDoc, p, j;
      float idf;

      if ((i&0xfff) == 0)
	fprintf(stderr, ".");
      /* read an entry for a word, just to get p value */
      dummy1 = fgetc(dict);
      dummy1 = fgetc(dict);
      fread(dummy2, sizeof(uchar), dummy1, dict);
      fread((char*)&fcnt, sizeof(fcnt), 1, dict);
      fread((char*)&wcnt, sizeof(wcnt), 1, dict);

      p = fcnt;

      idf = logN - log((double)fcnt);
      blk = BIO_Bblock_Init (NumPara, p);
      CurrDoc = 0;
      
      {
	unsigned long loc;
	fread((char*)&loc, sizeof(loc), 1, idx);
	if (ftell(invf) != loc)
	  {
	    FatalError(1, "Word %d  %d != %d", i, ftell(invf), loc);
	  }
      }
      for (j=0; j<p; j++) 
	{
	  unsigned long x, tf;
	  BBLOCK_DECODE(x, blk);
	  CurrDoc += x;
	  if (ifh.InvfLevel >= 2)
	    {
	      double weight;
	      GAMMA_DECODE(tf);
	      weight = tf*idf;
	      DocWeights[CurrDoc-1] += weight*weight;
	    }
	}
      while (__btg)
	DECODE_BIT;
    }
  DECODE_DONE
  fclose(dict);
  fclose(invf);
  fprintf(stderr, "\n");

  f = create_file(file_name, WEIGHTS_SUFFIX, "w", MAGIC_WGHT,
		   MG_ABORT);

  fwrite((char*)DocWeights, sizeof(float), NumPara, f);
  fclose(f);
  Xfree(DocWeights);
}











void Make_weight_approx(void)
{
  int i, pos, max;
  unsigned long buf;
  double U, L, B;
  FILE *approx, *exact;

  exact = open_file(file_name, WEIGHTS_SUFFIX, "r", MAGIC_WGHT,
		    MG_ABORT);

  /* calculate U and L */
  L = 1e300;
  U = 0;
  for (i=0; i<NumPara; i++)
    {
      float wgt;
      fread((char*)&wgt, sizeof(wgt), 1, exact);
      wgt = sqrt(wgt);
      if (wgt > U) U = wgt;
      if (wgt > 0 && wgt < L) L = wgt;

    }
  fseek(exact, sizeof(u_long), SEEK_SET);

  B = pow(U/L, pow(2.0, - (double)bits));

  fprintf(stderr, "L = %f\n",L);
  fprintf(stderr, "U = %f\n",U);
  fprintf(stderr, "B = %f\n",B);



  approx = create_file(file_name, APPROX_WEIGHTS_SUFFIX, "w", 
		       MAGIC_WGHT_APPROX, MG_ABORT);

  fwrite((char*)&bits, sizeof(bits), 1, approx);
  fwrite((char*)&L, sizeof(L), 1, approx);
  fwrite((char*)&B, sizeof(B), 1, approx);

  max = bits == 32 ? 0xffffffff : (1<<bits)-1;
  for (buf = pos = i = 0; i<NumPara; i++)
    {
      unsigned long fx;
      float wgt;
      fread((char*)&wgt, sizeof(wgt), 1, exact);
      wgt = sqrt(wgt);
      if (wgt == 0)
	{
	  wgt = L;
	  Message("Warning: Document %d had a weight of 0.", i);
	}
      fx = (int)floor( log(wgt/L) / log(B) );
      
      if (fx > max) fx = max;

      buf |= (fx << pos);
      pos += bits;

      if (pos >= MAXBITS)
	{
	  fwrite((char*)&buf, sizeof(buf), 1, approx);
	  buf = fx >> (bits - (pos - MAXBITS));
	  pos = pos - MAXBITS;
	}
    }
  if (pos > 0)
    fwrite((char*)&buf, sizeof(buf), 1, approx);
  
  fclose(approx);
  fclose(exact);
}





void Make_text_idx_wgt(void)
{
  compressed_text_header cth;
  int i;
  FILE *idx_wgt, *idx, *para, *exact;

  idx_wgt = create_file(file_name, TEXT_IDX_WGT_SUFFIX, "w", MAGIC_TEXI_WGT,
			MG_ABORT);

  idx = open_file(file_name, TEXT_IDX_SUFFIX, "r", MAGIC_TEXI,
			MG_ABORT);
  if (fread(&cth, sizeof(cth), 1, idx) != 1)
    FatalError(1, "Unable to read header of index file");

  exact = open_file(file_name, WEIGHTS_SUFFIX, "r", MAGIC_WGHT,
		    MG_ABORT);

  get_NumPara();
  if (cth.num_of_docs != NumPara)
    {
      Message("The number of documents does not equal "
	      "the number of paragraphs.");
      Message("Using the \"%s.invf.paragraph\" file\n", file_name);
      para = open_file(file_name, INVF_PARAGRAPH_SUFFIX, "r", MAGIC_PARAGRAPH,
		    MG_ABORT);
    }
  else
    para = NULL;

  for (i=0; i<= cth.num_of_docs; i++)
    {
      int count;
      struct
	{
	  unsigned long Start;
	  float Weight;
	} data;
      fread((char*)&data.Start, sizeof(unsigned long), 1, idx);
      if (para && i < cth.num_of_docs)
	fread((char*)&count, sizeof(count), 1, para);
      else
	count = 1;
      while (count--)
	{
	  fread((char*)&data.Weight, sizeof(float), 1, exact);
	  data.Weight = sqrt(data.Weight);
	  fwrite((char*)&data, sizeof(data), 1, idx_wgt);
	}
    }
  fclose(idx_wgt);
  fclose(idx);
  fclose(exact);
  if (para)
    fclose(para);
}



