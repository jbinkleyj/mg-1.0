/**************************************************************************
 *
 * invf.pass1.c -- Memory intensive pass 1 inversion
 * Copyright (C) 1994  Gary Eddy, Alistair Moffat and Neil Sharman
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
 *       @(#)invf.pass1.c	1.8 16 Mar 1994
 *
 **************************************************************************/

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>

#include "sysfuncs.h"
#include "messages.h"
#include "bitio_gen.h"
#include "memlib.h"

#include "mg_files.h"
#include "invf.h"
#include "mg.h"
#include "build.h"
#include "locallib.h"
#include "words.h"
#include "stemmer.h"
#include "hash.h"


char *SCCS_Id_invert_preprocess = "@(#)invf.pass1.c	1.8 16 Mar 1994";

#define LOGLOOKBACK	2
#define POOL_SIZE 1024*1024
#define HASH_POOL_SIZE 8192
#define INITIAL_HASH_SIZE 7927


typedef struct hash_rec 
{
  unsigned long fcnt; /* fragment frequency */
  unsigned long fnum; /* last fragment to use stem */
  unsigned long wcnt; /* stem frequency */
  uchar	*word;
} hash_rec;








static unsigned long words_read = 0, words_diff = 0, bytes_diff = 0;
static unsigned long outputbytes = 0;
static unsigned long inputbytes = 0;
static unsigned long MaxMemInUse = 0;
static unsigned long MemInUse = 0;

static hash_rec **HashTable;
static unsigned long HashSize;
static unsigned long HashUsed;
static uchar *Pool;
static int PoolLeft;

static hash_rec *hr_pool;
static int hr_PoolLeft;


static long	L1_bits=0, L1_ohead=0;
static long	L2_bits=0, L2_ohead=0;
static long	L3_bits=0, L3_ohead=0;
static long	callnum=0, wordnum = 0;




static void ChangeMem(int Change)
{
  MemInUse += Change;
  if (MemInUse > MaxMemInUse)
    MaxMemInUse = MemInUse;
}


/* ARGSUSED */
int init_invf_1(char *file_name)
{
  if (!(Pool = Xmalloc(POOL_SIZE)))
    {
      Message("Unable to allocate memory for pool");
      return(COMPERROR);
    }
  PoolLeft = POOL_SIZE;
  ChangeMem(POOL_SIZE);

  if (!(hr_pool = Xmalloc(HASH_POOL_SIZE * sizeof(hash_rec))))
    {
      Message("Unable to allocate memory for pool");
      return(COMPERROR);
    }
  hr_PoolLeft = HASH_POOL_SIZE;
  ChangeMem(HASH_POOL_SIZE * sizeof(hash_rec));

  HashSize = INITIAL_HASH_SIZE;
  HashUsed = 0;
  if (!(HashTable = Xmalloc(sizeof(hash_rec*)*HashSize)))
    {
      Message("Unable to allocate memory for table");
      return(COMPERROR);
    }
  ChangeMem(sizeof(hash_rec*)*HashSize);
#ifdef SUNOS5
  memset(HashTable, 0, sizeof(hash_rec*)*HashSize);
#else
  bzero((char*)HashTable, sizeof(hash_rec*)*HashSize);
#endif

  return(COMPALLOK);
}






static int process_doc(uchar *s_in, int l_in)
{
  uchar	chbuf, *end = s_in + l_in;
  callnum++;
  chbuf = *s_in++;
  inputbytes += l_in;
  if (!INAWORD(chbuf))
    if (SkipSGML)
      PARSE_NON_WORD_OR_SGML_TAG(s_in, end, chbuf);
    else
      PARSE_NON_WORD(s_in, end, chbuf);
  /*
   ** Alternately parse off words and non-words from the input
   ** stream beginning with a non-word. Each token is then
   ** inserted into the set if it does not exist or has it's
   ** frequency count incremented if it does.
   */
  while (s_in <= end) 
    {
      uchar Word[MAXSTEMLEN + 1];
      PARSE_LONG_WORD(Word, s_in, end, chbuf);
      stemmer(stem_method, Word);
      if (SkipSGML)
	PARSE_NON_WORD_OR_SGML_TAG(s_in, end, chbuf);
      else
	PARSE_NON_WORD(s_in, end, chbuf);
      
      wordnum++;
      words_read++;

      /* Search the hash table for Word */
      {
	register unsigned long hashval, step;
	register int hsize = HashSize;
	HASH(hashval, step, Word, hsize);
	for(;;) 
	  {
	    register uchar *s1;
	    register uchar *s2;
	    register int len;
	    register hash_rec *ent;
	    ent = HashTable[hashval];
	    if (!ent)
	      {
		int len = *Word+1;
		if (!hr_PoolLeft)
		  {
		    if (!(hr_pool = Xmalloc(HASH_POOL_SIZE * 
					   sizeof(hash_rec))))
		      {
			Message("Unable to allocate memory for pool");
			return(COMPERROR);
		      }
		    hr_PoolLeft = HASH_POOL_SIZE;
		    ChangeMem(HASH_POOL_SIZE * sizeof(hash_rec));
		  }
		ent = hr_pool++;
		hr_PoolLeft--;
		if (len > PoolLeft)
		  {
		    if (!(Pool = Xmalloc(POOL_SIZE)))
		      {
			Message("Unable to allocate memory for pool");
			return(COMPERROR);
		      }
		    PoolLeft = POOL_SIZE;
		    ChangeMem(POOL_SIZE);
		  }
		ent->wcnt = 1;
		ent->fcnt = 1;
		ent->fnum = callnum;
		ent->word = Pool;
		memcpy(Pool, Word, len);
		Pool += len;
		PoolLeft -= len;
		HashUsed++;
		HashTable[hashval] = ent;
		bytes_diff += Word[0];
		break;
	      }

	    /* Compare the words */
	    s1 = Word;
	    s2 = ent->word;
	    len = *s1+1;
	    for(;len; len--)
	      if (*s1++ != *s2++)
		break;

	    if(len) 
	      {
		hashval = (hashval + step);
		if (hashval >= hsize)
		  hashval -= hsize;
	      }
	    else 
	      {
		ent->wcnt++;
		if (callnum>ent->fnum) 
		  {
		    ent->fcnt++;
		    ent->fnum = callnum;
		  }
		break;
	      }
	  }
      }

      if (HashUsed >= HashSize >> 1)
	{
	  hash_rec **ht;
	  unsigned long size;
	  unsigned long i;
	  size = prime(HashSize*2);
	  if (!(ht = Xmalloc(sizeof(hash_rec*)*size)))
	    {
	      Message("Unable to allocate memory for table");
	      return(COMPERROR);
	    }
#ifdef SUNOS5
	  memset(ht, 0, sizeof(hash_rec*)*size);
#else
	  bzero((char*)ht, sizeof(hash_rec*)*size);
#endif
	  ChangeMem(sizeof(hash_rec*) * size);
	  
	  for (i=0; i<HashSize; i++)
	    if (HashTable[i])
	      {
		register uchar *wptr;
		hash_rec *ent;
		register unsigned long hashval, step;
		
		wptr = HashTable[i]->word;
		HASH(hashval, step, wptr, size);
		ent = ht[hashval];
		while (ent)
		  {
		    hashval += step;
		    if (hashval >= size)
		      hashval -= size;
		    ent = ht[hashval];
		  }
		ht[hashval] = HashTable[i];
	      }
	  Xfree(HashTable);
	  ChangeMem(-sizeof(hash_rec*) * HashSize);
	  HashTable = ht;
	  HashSize = size;
	}
	
    }
  return(COMPALLOK);
} /* encode */

#if 0
static void display(char *data, uchar *s, int l)
{
  int i;
  fprintf(stderr, "%s", data);
  for (i = 0; i < l; i++)
    {
      switch(s[i])
	{
	case '\n' : fputs("\\n",stderr); break;
	case '\003' : fputs("\\003",stderr); break;
	case '\002' : fputs("\\002",stderr); break;
	  default :
	    fprintf(stderr,"%c", s[i]);
	}
    }
  fprintf(stderr, "\n");
}
#endif

int process_invf_1(uchar *s_in, int l_in)
{
  /*display("Record : ", s_in, l_in);*/
  if (InvfLevel <= 2)
    return process_doc(s_in, l_in);
  else
    {
      int pos = 0;
      uchar *start = s_in;
      while (pos < l_in)
	{
	  if (s_in[pos] == TERMPARAGRAPH)
	    {
	      int len = pos + s_in + 1 - start;
	      /*display("Paragraph : ", start, len);*/
	      if (process_doc(start, len) != COMPALLOK)
		return(COMPERROR);
	      start = s_in+pos+1;
	    }
	  pos++;
	}
      if (start < s_in+pos)
	{
	  /*display("Paragraph : ", start, po
	    s + s_in - start);*/
	  return process_doc(start, pos + s_in - start);
	}
    }
  return COMPALLOK;
}

static int PackHashTable(void)
{
  int s,d;
  for (s=d=0; s<HashSize; s++)
    if (HashTable[s])
      HashTable[d++] = HashTable[s];
  ChangeMem(-sizeof(hash_rec*) * HashSize);
  ChangeMem(sizeof(hash_rec*) * HashUsed);
  if (!(HashTable = Xrealloc(HashTable, sizeof(hash_rec*)*HashUsed)))
    {
      Message("Out of memory");
      return COMPERROR;
    }
  HashSize = HashUsed;
  return COMPALLOK;
}





static int ent_comp(const void *A, const void *B)
{
  register int diff;
  uchar *s1 = (*((hash_rec **)A))->word;
  uchar *s2 = (*((hash_rec **)B))->word;
  diff = ((uchar*)s1)[1] - ((uchar*)s2)[1];
  return( diff ? diff : compare(s1, s2) );
} /* stem_comp */





/*
 * void count_text()
 *
 * The maths used in this function is described in the paper "Coding for 
 * Compression in Full-Text Retrieval Systems"
 *
 */
static void count_text()
{
  int i;
  for (i = 0; i < HashUsed; i++)
    {
      hash_rec *wrd = HashTable[i];
      /* estimate size of a level 1 inverted file */
      L1_bits += (int)(0.99 + wrd->fcnt*(1.6+LOG_2(1.0*callnum/wrd->fcnt)));
      L1_ohead += BIO_Gamma_Length(wrd->fcnt);

      L2_bits += BIO_Unary_Length(wrd->wcnt);
      L2_ohead += BIO_Gamma_Length(wrd->wcnt-wrd->fcnt+1);

      L3_bits += (int)(0.99 + wrd->wcnt * 
		       (1.6+LOG_2(1.0*words_read/(wrd->wcnt+callnum))));
      L3_ohead += 0;
    }
  L3_bits = (L3_bits+L2_bits+L1_bits+7)/8;
  L3_ohead = (L3_ohead+L2_ohead+L1_ohead+7)/8;
  L2_bits = (L2_bits+L1_bits+7)/8;
  L2_ohead = (L2_ohead+L1_ohead+7)/8;
  L1_bits = (L1_bits+7)/8;
  L1_ohead = (L1_ohead+7)/8;
} /* count_text */



/*
 *	write_stem_file():
 *		writes out the stemmed dictionary file
 *              in the following format 
 *			lookback value (int)
 *			totalbytes value (int)
 *			indexstringbytes (int)
 *                      for each word 
 *		 	  wordlen (4 bits)
 *			  prefix match (4 bits)
 *			  word (wordlen bytes)
 *                        word frequency (int)
 *                        word count (int)
 *
 *	Accesses outside variables:	
 *
 *	Return value...:		
 */

static void write_stem_file(FILE *sp)
{
  long lookback=(1<<LOGLOOKBACK); /* ???? */
  long totalbytes=0;       /* The sum of the length of all words, including 
			      the length byte */
  long indexstringbytes=0; /* The amount of space required to store the 
			      words in the diction, this takes into account 
			      the prefixes */
  uchar *lastword = NULL;  /* A pointer to the last word processed */
  long pos, j;
  struct invf_dict_header idh;

  /* Calculate the size of certain things */
  for(j = 0; j<HashSize; j++)
    {
      uchar *word = HashTable[j]->word;
      indexstringbytes += word[0]+2;
      totalbytes += word[0]+1;
      if (j%lookback != 0)
	indexstringbytes -= prefixlen(lastword, word);
      lastword = word;
    }

  lastword = NULL;

  pos = ftell(sp);

  fwrite((char*)&idh, sizeof(idh), 1, sp);
  outputbytes += sizeof(idh);
  
  idh.lookback = lookback;
  idh.dict_size = HashSize;
  idh.total_bytes = totalbytes;
  idh.index_string_bytes = indexstringbytes;

  for(j=0; j<HashSize; j++)
    {
      int i;
      hash_rec *ent = HashTable[j];
      if (lastword != NULL)
	/* look for prefix match with prev string */
	i = prefixlen(lastword, ent->word);
      else
	i = 0;
      fputc(i,sp);
      fputc(ent->word[0]-i, sp);
      fwrite((char*)ent->word+i+1, sizeof(uchar), ent->word[0]-i, sp);
      outputbytes += ent->word[0]-i+1;
      fwrite((char*)&(ent->fcnt), sizeof(ent->fcnt), 1, sp);
      outputbytes += sizeof(ent->fcnt);
      fwrite((char*)&(ent->wcnt), sizeof(ent->wcnt), 1, sp);
      outputbytes += sizeof(ent->wcnt);
      lastword = ent->word;
    }

  idh.input_bytes = inputbytes;
  idh.num_of_docs = callnum;
  idh.num_of_words = words_read;
  idh.stem_method = stem_method;

  fseek(sp, pos, 0);
  fwrite((char*)&idh, sizeof(idh), 1, sp);

} /* write_stem_file() */



/*
 *	write_codes():
 *		calls functions to assign and write out codes and
 *		then prints out stats about execution results.
 *
 *	Accesses outside variables:	z
 *
 *	Return value...:		z
 */

static void write_codes(FILE *sp)
{
  unsigned long dicts=0;
  char *temp_str;
  outputbytes = 0;
  write_stem_file(sp);
  dicts = outputbytes; 
  
  temp_str = msg_prefix;
  msg_prefix = "invf.pass1.c";
#ifndef SILENT
  if (InvfLevel == 3)
    Message("Paragraphs          : %8d\n", callnum); 
  Message("Peak memory usage   : %10.1f Mb\n",
	  (double)MaxMemInUse/1024.0/1024.0);
  Message("Stems size          : %10.1f kB  %5.2f%%\n", 
	  (double)dicts/1024, (double)100*dicts/inputbytes); 
  Message("Lev 1 inverted file : %10.1f kB  %5.2f%%\n",
	  (double)(L1_bits+L1_ohead)/1024, 
	  (double)100*(L1_bits+L1_ohead)/inputbytes);
  Message("Lev 2 inverted file : %10.1f kB  %5.2f%%\n",
	  (double)(L2_bits+L2_ohead)/1024, 
	  (double)100*(L2_bits+L2_ohead)/inputbytes);
  Message("Lev 3 inverted file : %10.1f kB  %5.2f%%\n",
	  (double)(L3_bits+L3_ohead)/1024, 
	  (double)100*(L3_bits+L3_ohead)/inputbytes);
  Message("Record addresses    : %10.1f kB  %5.2f%%\n", 
	  (double)words_diff*4/1024, (double)100*words_diff*4/inputbytes);
#endif
  msg_prefix = temp_str;
} /* write_codes() */






int done_invf_1(char *file_name)
{
  FILE *sp;

  if (!(sp = create_file(file_name, INVF_DICT_SUFFIX, "w+", 
			 MAGIC_STEM_BUILD, MG_MESSAGE)))
    return COMPERROR;

  if (PackHashTable() == COMPERROR)
    return COMPERROR;
  qsort(HashTable, HashUsed, sizeof(hash_rec*), ent_comp);
  
  count_text();
  write_codes(sp);

  fclose(sp);

  return(COMPALLOK);
} /* done_encode */







