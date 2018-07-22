/**************************************************************************
 *
 * invf.pass2.c -- Memory intensive pass 2 inversion
 * Copyright (C) 1994  Neil Sharman and Alistair Moffat
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
 *       @(#)invf.pass2.c	1.9 16 Mar 1994
 *
 **************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <memory.h>

#include "memlib.h"
#include "sysfuncs.h"
#include "messages.h"
#include "bitio_m.h"
#include "bitio_m_mems.h"
#include "bitio_gen.h"
#include "bitio_stdio.h"

#include "mg_files.h"
#include "invf.h"
#include "locallib.h"
#include "mg.h"
#include "build.h"
#include "words.h"
#include "stemmer.h"
#include "hash.h"


#define COPY(d, s, n)					\
	do {						\
		register uchar *ld=(d), *ls=(s);	\
		register int ln=(n);			\
		while (ln--) *ld++ = *ls++;		\
	} while (0)

#define COMPARE(w1, w2, c)				\
	do {						\
		register uchar *s1 = (w1), *s2 = (w2);	\
		int l1= *s1, l2= *s2;			\
		register int l = (l1<l2) ? l1 : l2;	\
		c = (l1 - l2);				\
		while (l--) {				\
			if (*++s1 != *++s2) {		\
				c = (*s1 - *s2);	\
				break;			\
			}				\
		}					\
	} while (0)

#define BINARY_COMPARE(w1, w2, c)			\
	do {						\
		if ((c = (*w1 != *w2)) == 0)		\
			COMPARE(w1, w2, c);		\
	} while (0)


/* Uncomment this is speed is of the essence and space can be wasted */
/* #define HASHTABLE */

/* Uncomment this for a full index to avoid binary search on strings */
#define FULLINDEX

/* Uncomment this to use two character index array to narrow initial
   binary search region or guide hash function in encode() */
#define INDEXONTWOCHARS

char *SCCS_Id_invert_build = "@(#)invf.pass2.c	1.9 16 Mar 1994";

typedef uchar*	chptr;
typedef	struct	wcode_rec wcode;

struct wcode_rec {
	unsigned long	last, nI;
};

uchar	**word;
uchar	*alllogb;

#define	WORDS		1
#define	NONWORDS	0

#define CNULL		((uchar *) NULL)

	


static unsigned long N, wordarray;
static wcode		*e_array;
static uchar		*MemoryBuffer;
static int		no_of_words; 
static unsigned long	seekwords, lookback;
static int		no_of_ptrs = 0; 

static FILE		*fp, *ip;

static FILE		*invflens;
static unsigned long	totalbitlen=0;

static FILE		*invf_para;

static unsigned long MaxMemInUse = 0;

static struct invf_dict_header idh;

#ifdef INDEXONTWOCHARS
static int c_trans[256] =
/* 0x00 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0x10 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0x20 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 0x30 */   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
/* 0x40 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0x50 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0x60 */   0,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
/* 0x70 */  25,26,27,28,29,30,31,32,33,34,35, 0, 0, 0, 0, 0,
/* 0x80 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0x90 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0xA0 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0xB0 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0xC0 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0xD0 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0xE0 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 0xF0 */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static int short_circuit[36*36+1];

#define CALC_CODE(p) (c_trans[(p)[1]]*36 + ((p)[0]>1 ? c_trans[(p)[2]] : 0))
#endif

#ifdef FULLINDEX
static unsigned long *fourbytes;
#endif
      
#ifdef HASHTABLE
static unsigned long *H, tablesize;
#define HRATIO 2.0
#endif

/* these next two for speed, but might cause an abort if */
/* incorrectly set -- i.e., if lookback turns out to be anything but 4
*/
#define LOOKBACKBITS 2
#define LOOKBACKMASK 0x3


int init_invf_2(char *file_name)
{
  long totalPbytes=0, totalDbytes=0, totalFbytes=0, totalIbytes=0,
	totalHbytes=0;
  int i;
  uchar prev[MAXSTEMLEN+1];
  uchar	*allwords;


  
  if (!(fp = open_file(file_name, INVF_DICT_SUFFIX, "r", 
		       MAGIC_STEM_BUILD, MG_MESSAGE)))
    return COMPERROR;

  if (InvfLevel == 3)
    if (!(invf_para = create_file(file_name, INVF_PARAGRAPH_SUFFIX, "w", 
				  MAGIC_PARAGRAPH, MG_MESSAGE)))
      return COMPERROR;

  if (!(ip = create_file(file_name, INVF_SUFFIX, "w", 
			 MAGIC_INVF, MG_MESSAGE)))
    return COMPERROR;



  /* Read in the stemmed dictionary file header */
  fread((char*)&idh, sizeof(idh), 1, fp);
  no_of_words = idh.dict_size;
  lookback = idh.lookback;
  if (lookback != (1<<LOOKBACKBITS))
    {
      Message("oops: pass1 used a `lookback' other than four\n");
      return(COMPERROR);
    }

  N = idh.num_of_docs;

  totalPbytes += no_of_words * 6 * sizeof(long);
  /* counting fields for:
     left,right,parent,count,word
     in a splay tree dynamic dictionary, with an extra
     field for last necessary for fragment level indexing */
  totalPbytes += idh.total_bytes;

  if (!(allwords = Xmalloc(sizeof(uchar) * idh.index_string_bytes)))
    {
      fprintf(stderr, "init_encode: no memory for words\n");
      return(COMPERROR);
    }
  MaxMemInUse += idh.index_string_bytes;
  totalDbytes += idh.index_string_bytes;

  wordarray = (no_of_words+idh.lookback-1)/idh.lookback;
  if (!(word = Xmalloc(sizeof(uchar*) * wordarray))) 
    {
      Message("init_encode: no memory for words\n");
      return(COMPERROR);
    }
  MaxMemInUse += sizeof(*word) * wordarray;
  totalDbytes += sizeof(*word) * wordarray;

  if (!(e_array = Xmalloc(sizeof(wcode) * no_of_words))) 
    {
      Message("init_encode: no more memory\n");
      return(COMPERROR);
    }
  MaxMemInUse += sizeof(*e_array) * no_of_words;
  totalDbytes += sizeof(*e_array) * no_of_words;

  /* separate storage for the log(b) values, one byte each */
  if (!(alllogb = Xmalloc(no_of_words * sizeof(uchar))))
    {
      Message("no more memory\n");
      return(COMPERROR);
    }
  totalDbytes += sizeof(*alllogb) * no_of_words;

  /* make a note of where we are */
  seekwords = ftell(fp);

#ifdef INDEXONTWOCHARS
  for (i=0; i<36*36; i++)
    short_circuit[i] = -1;
#endif

#ifdef FULLINDEX
  if (!(fourbytes = Xmalloc(wordarray*sizeof(unsigned long))))
    {
      Message("no more memory\n");
      return(COMPERROR);
    }
  MaxMemInUse += wordarray*sizeof(unsigned long);
#endif

#ifdef HASHTABLE
  tablesize = no_of_words*HRATIO;
  tablesize = prime(tablesize);
  if (!(H = Xmalloc(tablesize*sizeof(unsigned long))))
    {
      Message("no more memory\n");
      return(COMPERROR);
    }
  for (i=0; i<tablesize; i++)
	H[i] = no_of_words;
  MaxMemInUse += tablesize*sizeof(unsigned long);
#endif
  
  
  for (i = 0; i < no_of_words; i++) 
    {
      register unsigned long copy, suff;
      register wcode *arr;
      unsigned long fcnt, wcnt;
      register unsigned long p;
#ifdef INDEXONTWOCHARS
      register unsigned long code;
#endif

      /* build a new word on top of prev */
      copy = fgetc(fp);
      suff = fgetc(fp);
      *prev = copy+suff;
      fread(prev+copy+1, sizeof(uchar), suff, fp);
      totalFbytes += 1 + suff;

      /* insert other data into table */
      arr = e_array+i;
      fread((char*)&fcnt, sizeof(fcnt), 1, fp);
      fread((char*)&wcnt, sizeof(wcnt), 1, fp);
      p = fcnt;

      totalFbytes += sizeof(unsigned long);
      
      /* allocate space for the inverted file entries.
	 estimate number of bits needed in worst case */
      arr->last = 0;
      arr->nI = totalIbytes; /* really bits at this stage */
      totalIbytes += BIO_Bblock_Bound(N, p);

      if (InvfLevel >= 2)
	totalIbytes += wcnt;

      /* store value of b to be used */
      alllogb[i] = floorlog_2(BIO_Bblock_Init_W(N, p));

#ifdef INDEXONTWOCHARS
      code = CALC_CODE(prev);
      if (short_circuit[code] == -1)
#ifdef HASHTABLE
	short_circuit[code] = i;
#else
	short_circuit[code] = i/lookback;
#endif
#endif

      /* set word pointer if it is due */
      if (i%lookback == 0) 
	{
	  word[i/lookback] = allwords;
	  suff += copy;
	  copy = 0;
	}
      *allwords++ = copy;
      *allwords++ = suff;
      COPY(allwords, prev+copy+1, suff);

#ifdef FULLINDEX
      if (i%lookback == 0) 
	{
		register char *w = (char*)(word[i/lookback]+1),
			      *p = (char *)(fourbytes+i/lookback);
		register int  l = *w<4 ? *w : 4;
		for (; l--; )
			*p++ = *++w;
        }
#endif

#ifdef HASHTABLE
      {
		register unsigned long h, step;
		HASH(h, step, prev, tablesize);
		while (H[h] != no_of_words) {
			h += step;
			if (h >= tablesize)
				h -= tablesize;
		}
		H[h] = i;
      }
#endif
      
      allwords += suff;
    }

#ifdef INDEXONTWOCHARS
#ifdef HASHTABLE
  short_circuit[36*36] = no_of_words;
#else
  short_circuit[36*36] = wordarray-1;
#endif
  for (i=36*36-1; i>=0; i--)
    if (short_circuit[i] == -1)
      short_circuit[i] = short_circuit[i+1];
#endif

  
  /* now convert to bytes, and actually get the space */
  totalIbytes = (totalIbytes+7)/8;
  if (!(MemoryBuffer = Xmalloc( totalIbytes )))
    {
      Message("insufficient memory\n");
      return(COMPERROR);
    }
  MaxMemInUse += totalIbytes;
#ifdef SUNOS5
  memset(MemoryBuffer, 0,totalIbytes);
#else
  bzero((char*)MemoryBuffer, totalIbytes);
#endif

  {
    char *temp_str = msg_prefix;
    msg_prefix = "invf.pass2";
#ifndef SILENT
    Message("Pass one data structures        : %6.3f Mbyte\n",
	    (double)totalPbytes/1024/1024);
    Message("Disk space for words+pvals      : %6.3f Mbyte\n",
	    (double)totalFbytes/1024/1024);
    Message("Pass three data structures      : %6.3f Mbyte\n",
	    (double)totalDbytes/1024/1024);
#ifdef HASHTABLE
    totalHbytes += tablesize*sizeof(unsigned long);
#endif
#ifdef INDEXONTWOCHARS
    totalHbytes += sizeof(c_trans) + sizeof(short_circuit);
#endif
#ifdef FULLINDEX
    totalHbytes += wordarray*sizeof(unsigned long);
#endif
    Message("Pass three search structure(s)  : %6.3f Mbyte\n",
	    (double)totalHbytes/1024/1024);
    Message("Pass three compressed vectors   : %6.3f Mbyte\n",
	    (double)totalIbytes/1024/1024);
    if (InvfLevel >= 2)
      Message("Document weight vector          : %6.3f Mbyte\n",
	      (double)N*sizeof(double)/1024/1024);
#endif
    msg_prefix = temp_str;
  }
  if (!(invflens = create_file(file_name, INVF_IDX_SUFFIX, "w", 
			       MAGIC_INVI, MG_MESSAGE)))
    return COMPERROR;

  return(COMPALLOK);
} /* init_encode() */









static int process_doc(uchar *s_in, int l_in)
{
  static long callnum = 0;
  static long bytenum = 0;
  static long wordnum = 0;

  int		res;
  uchar		chbuf, *end = s_in + l_in;
  unsigned long	tocode;
  callnum++;

  chbuf = *s_in++;
  if (!INAWORD(chbuf))
    if (SkipSGML)
      PARSE_NON_WORD_OR_SGML_TAG(s_in, end, chbuf);
    else
      PARSE_NON_WORD(s_in, end, chbuf);

  while ( s_in <= end) 
    {
      uchar Word[MAXSTEMLEN + 1];
      PARSE_LONG_WORD(Word, s_in, end, chbuf);
      stemmer(idh.stem_method, Word);
      if (SkipSGML)
	PARSE_NON_WORD_OR_SGML_TAG(s_in, end, chbuf);
      else
	PARSE_NON_WORD(s_in, end, chbuf);

      bytenum += *Word;
      

      if (*Word == 0) continue;

      res = COMPERROR;

#ifdef HASHTABLE
      {
	register unsigned long h, step;
	register int copy, suff;
	uchar prev[MAXSTEMLEN+1], *w;
	int loc, mod;
#ifdef INDEXONTWOCHARS
	/* use two byte prefix as filter on hash location */
	unsigned long c = CALC_CODE(Word),
		lo = short_circuit[c],
		hi = short_circuit[c+1];
#endif

	HASH(h, step, Word, tablesize);
	/* just to make the loop work out sensibly... */
	h -= step;
	for ( ;; ) {
		h += step;
		if (h >= tablesize)
			h -= tablesize;
		loc = H[h];
#ifdef INDEXONTWOCHARS
		if (loc<lo || hi<=loc) {
			/* cannot be the right one */
			continue;
		}
#endif
		/* want to achieve
			mod = 1 + loc % lookback;
			loc = loc / lookback;
		   but to run quickly. So pray that lookback is 4...
		*/
		mod = 1 + (loc & LOOKBACKMASK);
		loc = loc >> LOOKBACKBITS;
		w = word[loc];
		while (mod--) {
			copy = *w++;
			suff = *w++;
			*prev = copy+suff;
			COPY(prev+copy+1, w, suff);
			w += suff;
		}
		BINARY_COMPARE(prev, Word, c);
		if (c==0) {
			res = H[h];
			break;
		}
	}
      }
#else
      {
	/* binary search looking for the word */
	int lo=0;
	int hi=wordarray-1;
	int c;
#ifdef FULLINDEX
	unsigned long firstfour=0;
#endif


#ifdef INDEXONTWOCHARS
	/* use two byte prefix to set initial search bounds */
	c = CALC_CODE(Word);
	lo = short_circuit[c];
	hi = short_circuit[c+1];
#endif

#ifdef FULLINDEX
	/* extract the first four bytes of the search word */
	{
		register char *w = (char*)Word,
			      *p = (char *)(&firstfour);
		register int  l = *w<4 ? *w : 4;
		for (; l--; )
			*p++ = *++w;
	}
	/* binary search on four byte prefixes
	   to find the bottom of the range */
	{
		register unsigned long ff=firstfour;
		register int l=lo, h=hi, mid;

		while (l<h) {
			mid = l + ((h-l+1)>>1);
			if (fourbytes[mid] < ff)
				l = mid;
			else
				h = mid-1;
		}
		/* then linear search to find the top of the range */
		while (h < wordarray-1 && fourbytes[h+1] == ff) {
			h++;
		}
		/* foil the binary search if only one block in the range */
		l++;
		hi = h;
		lo = l;
	}
#endif

	/* binary search to find the exact block of words */
	{
		register int l=lo, h=hi, mid, c;
		while (l <= h) 
		  {
		    mid = l + ((h-l)>>1);
#ifdef INDEXONTWOCHARS
		    COMPARE(Word, word[mid]+1, c);
#else
		    c = Word[1] - word[mid][2];
		    if (word[mid][1]==0 || c==0) {
			COMPARE(Word, word[mid]+1, c);
		    }
#endif
		    if (c<0) 
		      h = mid-1;
		    else 
		      if (c>0) 
			l = mid+1;
		      else 
			{
			  res = mid<<LOOKBACKBITS;
			  break;
			}
		  }
		lo = l;
		hi = h;
	}

	if (res == COMPERROR) 
	  {
	    /* linear search within the block to find the word */
	    register uchar *w = word[hi];
	    uchar prev[MAXSTEMLEN+1];
	    register unsigned copy, suff;
	    res = hi<<LOOKBACKBITS;
	    /* first word in group is checked again */
	    for (;;) 
	      {
		copy = *w++;
		suff = *w++;
		COPY(prev+copy+1, w, suff);
		*prev = copy+suff;
		BINARY_COMPARE(Word, prev, c);
		if (c == 0) 
			break;
		else {
			res++;
			w += suff;
		}
	      }
	    
	  }
      }
#endif

      /* if (res == COMPERROR) 
	{
	  Message("invert.build: unknown word \"%.*s\"\n",
		  Word[0], Word+1);
	  return(COMPERROR);
	} */
      {
	wcode *arr=e_array+res;
	int logb=alllogb[res], b=1<<logb;
	wordnum++;

	tocode = callnum;

	ENCODE_START(MemoryBuffer, arr->nI)
	if (tocode>arr->last) 
	  {
	    register int x;
	    x = tocode - arr->last - 1;
	    BBLOCK_ENCODE(x+1, b);
	    if (InvfLevel >= 2)
	      ENCODE_BIT(1);
	    no_of_ptrs++;
	    arr->last = tocode;
	  }
	else
	  if (InvfLevel >= 2)
	  {
	    __pos--;
	    ENCODE_BIT(0);
	    ENCODE_BIT(1);
	  }
      arr->nI = __pos;
      ENCODE_DONE	
      }
    }
  return(COMPALLOK);
} /* encode() */




int process_invf_2(uchar *s_in, int l_in)
{
  if (InvfLevel <= 2)
    return process_doc(s_in, l_in);
  else
    {
      int count = 0;
      int pos = 0;
      uchar *start = s_in;
      while (pos < l_in)
	{
	  if (s_in[pos] == TERMPARAGRAPH)
	    {
	      int len = pos + s_in +1 - start;
	      if (process_doc(start, len) != COMPALLOK)
		return(COMPERROR);
	      start = s_in+pos+1;
	      count++;
	    }
	  pos++;
	}
      if (start < s_in+pos)
	{
	  if (process_doc(start, pos + s_in - start) != COMPALLOK)
	    return (COMPERROR);
	  count++;
	}
      fwrite((char*)&count, sizeof(count), 1, invf_para); 
    }
  return COMPALLOK;
}




int done_invf_2(char *file_name)
{
  FILE *dw = NULL;
  char *temp_str;
  wcode *arr;
  stdio_bitio_state sbs;
  struct invf_file_header ifh;
  long i, j;
  unsigned long nwords=0, totalpoint=0, totalIbits=0;
  unsigned long bits_output, bytes_output;
  double logN = log((double)N);
  float *DocWeightBuffer = NULL;

  if (InvfLevel == 3)
    fclose(invf_para);


  if (InvfLevel >= 2)
    {
      if (!(dw = create_file(file_name, WEIGHTS_SUFFIX, "w", 
			     MAGIC_WGHT, MG_MESSAGE)))
	return COMPERROR;

      if (!(DocWeightBuffer = Xmalloc( N*sizeof(*DocWeightBuffer) )))
	{
	  Message("Insufficient memory\n");
	  return COMPERROR;
	}
      MaxMemInUse += N*sizeof(*DocWeightBuffer);
#ifdef SUNOS5
      memset(DocWeightBuffer, 0, N*sizeof(*DocWeightBuffer));
#else
      bzero((char*)DocWeightBuffer, N*sizeof(*DocWeightBuffer));
#endif
    }

  temp_str = msg_prefix;
  msg_prefix = "invf.pass2";
#ifndef SILENT
  Message("Peak memory usage   : %10.1f Mb\n",
	  (double)MaxMemInUse/1024.0/1024.0);
#endif
  msg_prefix = temp_str;

  ifh.no_of_words = no_of_words;
  ifh.no_of_ptrs = no_of_ptrs;
  ifh.skip_mode = 0;
#ifdef SUNOS5
  memset(ifh.params, 0, sizeof(ifh.params));
#else
  bzero((char*)ifh.params, sizeof(ifh.params));
#endif
  ifh.InvfLevel = InvfLevel;
  fwrite((char*)&ifh, sizeof(ifh), 1, ip);

  bits_output = ftell(ip)*8;

  BIO_Stdio_Encode_Start(ip, &sbs);

  /* find the right place in the file to start reading p values */
  fseek(fp, seekwords, 0);
  for (i=0; i<no_of_words; i++) 
    {
      unsigned long fcnt, wcnt;
      register unsigned long p;
      uchar dummy1, dummy2[MAXSTEMLEN+1];
		/* read an entry for a word, just to get p value */
      dummy1 = fgetc(fp);
      dummy1 = fgetc(fp);
      fread(dummy2, sizeof(uchar), dummy1, fp);
      fread((char*)&fcnt, sizeof(fcnt), 1, fp);
      fread((char*)&wcnt, sizeof(wcnt), 1, fp);
      p = fcnt;

      /* gotcha */
      arr = e_array+i;
      if (p > 0) 
	{
	  int	logb=alllogb[i], b=(1<<logb), nbits;
	  int	blk = BIO_Bblock_Init (N, p);
	  int CurrDoc = 0;
	  double idf = logN - log((double)fcnt);
	  if (b!=BIO_Bblock_Init_W(N,p)) 
	    {
	      fprintf(stderr, "bblock error\n");
	      exit(1);
	    }
	  bytes_output = bits_output >> 3;
	  fwrite((char*)&bytes_output, sizeof(bytes_output), 1, invflens);

	  nbits = arr->nI - totalIbits;
	  fflush(invflens);
	  totalbitlen += nbits;
	  nwords++;

	  arr->nI = totalIbits;
	  DECODE_START(MemoryBuffer, arr->nI)
	  for (j=0; j<p; j++) 
	    {
	      /* decode the next offset */
	      unsigned long x;
	      BBLOCK_DECODE(x, b);
	      if (x==0)
		fprintf(stderr, "x=0\n");
	      BIO_Stdio_Bblock_Encode(x, blk, &sbs, &bits_output);

	      CurrDoc += x;

	      if (InvfLevel >= 2)
		{
		  register double weight;
		  register int tf;
		  UNARY_DECODE(tf);

		  BIO_Stdio_Gamma_Encode(tf, &sbs, &bits_output);
		  weight = tf*idf;
		  DocWeightBuffer[CurrDoc-1] += weight*weight;
		}
	    }
	  arr->nI = __pos;
	  DECODE_DONE
	  while (sbs.Btg != 8)
	    {
	      BIO_Stdio_Encode_Bit(0, &sbs);
	      bits_output++;
	    }
	    
	  totalpoint += p;
	  totalIbits += BIO_Bblock_Bound(N, p);

	  if (InvfLevel >= 2)
	    totalIbits += wcnt;

	}
    }

  BIO_Stdio_Encode_Done(&sbs);
  
  if (InvfLevel >= 2)
    {
      fwrite((char*)DocWeightBuffer, sizeof(*DocWeightBuffer), N, dw);
      fclose(dw);
    }

  bytes_output = bits_output >> 3;
  fwrite((char*)&bytes_output, sizeof(bytes_output), 1, invflens);
  fclose(invflens);

  return(COMPALLOK);
}

