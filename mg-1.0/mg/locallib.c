/**************************************************************************
 *
 * locallib.c -- Misc functions
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
 *       @(#)locallib.c	1.6 16 Mar 1994
 *
 **************************************************************************/

#include "sysfuncs.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "memlib.h"
#include "bitio_gen.h"
#include "huffman.h"

#include "locallib.h"
#include "text.h"

char *SCCS_Id_locallib = "@(#)locallib.c	1.6 16 Mar 1994";


int vecentropy(int *A, int n)
{
  int total=0, i;
  double bits=0.0;
  
  for (i=0; i<n; i++)
    total += A[i];
  for (i=0; i<n; i++)
    if (A[i])
      bits += A[i] * LOG_2(1.0*total/A[i]);
  return((int)(bits+0.9999));
	
}

static void siftdown(int *A, int i, int n)
{ 
  int ms, t;
  
  while (2*i+1<=n-1) 
    { 
      if (2*i+1==n-1 || A[2*i+1] < A[2*i+2])
	ms = 2*i+1; 
      else 
	ms = 2*i+2;
      if (A[i] > A[ms]) 
	{ 
	  t = A[i]; 
	  A[i] = A[ms]; 
	  A[ms] = t;
	}
      i = ms;
    }
}

static void buildheap(int *A, int n)
{ 
  int i;
  for (i=n/2-1; i>=0; i--)
    siftdown(A, i, n);
}

int huffcodebits(unsigned long *A, int n)
{ 
  int i, j, tot=0, bits=0, v1, v2, *B;
  
  B = (int *) Xmalloc(n*sizeof(int));
  j=0;
  for (i=0; i<n; i++)
    {
      if (A[i]) 
	{
	  tot += (B[j++] = A[i]);
	}
    }
  n = j;
  
  if (n==0 || tot==0) return(0);
  
  buildheap(B, n);
  while (n>1) 
    { 
      v1 = B[0];
      B[0] = B[n-1];
      n--;
      siftdown(B, 0, n);
      v2 = B[0];
      B[0] = v1+v2;
      siftdown(B, 0, n);
      bits += v1+v2;
    }
  Xfree(B);
  return(bits);
}


int modelbits(unsigned long *A, int n)
{ 
  int i, bits=0, last, N=0;
  
  last = -1;
  for (i=0; i<n; i++) 
    {
      if (A[i]) 
	{
	  bits += BIO_Gamma_Length(i-last) + BIO_Gamma_Length(A[i]);
	  last = i;
	  N++;
	}
    }
  if (N==0) return(0);
  bits += BIO_Gamma_Length(N);
  return(bits);
}





/* Find the next prime number larger than p */
int prime(int p)
{
  int limit;
  p += (p&1) + 1;
  limit = (int) sqrt((double)p) + 1;
  do
    {
      int j;
      while (limit*limit < p) limit++;
      for(j=3; j <= limit && p % j; j += 2)
	; /* NULLBODY */ 
      if (j > limit) break;
      p += 2;
    }
  while(1);
  return(p);
}



int Read_cdh(FILE *f, compression_dict_header *cdh, u_long *mem, 
	     u_long *disk)
{
  if (disk)
    (*disk) += sizeof(*cdh);
  return (fread(cdh, sizeof(*cdh), 1, f) == 1) ? 0 : -1;
}


int F_Read_cdh(File *f, compression_dict_header *cdh, u_long *mem, 
	       u_long *disk)
{
  if (disk)
    (*disk) += sizeof(*cdh);
  return (Fread(cdh, sizeof(*cdh), 1, f) == 1) ? 0 : -1;
}


int Read_cfh(FILE *f, comp_frags_header *cfh, u_long *mem, u_long *disk)
{
  if (Read_Huffman_Data(f, &cfh->hd, mem, disk) == -1)
    return -1;
  if (fread(&cfh->uncompressed_size, 
	    sizeof(cfh->uncompressed_size), 1, f) != 1) 
    return -1;

  if (disk)
    (*disk) += sizeof(cfh->uncompressed_size);

#ifdef SUNOS5
  memset(cfh->huff_words_size, 0, sizeof(cfh->huff_words_size));
#else
  bzero((char*)cfh->huff_words_size, sizeof(cfh->huff_words_size));
#endif

  if (fread(cfh->huff_words_size+cfh->hd.mincodelen, 
	    sizeof(*cfh->huff_words_size),
	    cfh->hd.maxcodelen-cfh->hd.mincodelen+1, f) !=
      cfh->hd.maxcodelen-cfh->hd.mincodelen+1)
    return -1;

  if (disk)
    (*disk) += sizeof(*cfh->huff_words_size) * 
      (cfh->hd.maxcodelen-cfh->hd.mincodelen+1);
  return 0;
}



int F_Read_cfh(File *f, comp_frags_header *cfh, u_long *mem, u_long *disk)
{
  if (F_Read_Huffman_Data(f, &cfh->hd, mem, disk) == -1)
    return -1;
  if (Fread(&cfh->uncompressed_size, 
	    sizeof(cfh->uncompressed_size), 1, f) != 1) 
    return -1;

  if (disk)
    (*disk) += sizeof(cfh->uncompressed_size);

#ifdef SUNOS5
  memset(cfh->huff_words_size, 0, sizeof(cfh->huff_words_size));
#else
  bzero((char*)cfh->huff_words_size, sizeof(cfh->huff_words_size));
#endif

  if (Fread(cfh->huff_words_size+cfh->hd.mincodelen, 
	    sizeof(*cfh->huff_words_size),
	    cfh->hd.maxcodelen-cfh->hd.mincodelen+1, f) !=
      cfh->hd.maxcodelen-cfh->hd.mincodelen+1)
    return -1;

  if (disk)
    (*disk) += sizeof(*cfh->huff_words_size) * 
      (cfh->hd.maxcodelen-cfh->hd.mincodelen+1);

  return 0;
}





char *char2str(uchar c)
{
  static char buf[10];
  switch (c)
    {
    case '\n' :
      sprintf(buf, "\\n");
      break;
    case '\b' :
      sprintf(buf, "\\b");
      break;
    case '\f' :
      sprintf(buf, "\\f");
      break;
    case '\t' :
      sprintf(buf, "\\t");
      break;
    default:
      if (c >= ' ' && c <= 126)
	switch (c)
	  {
	  case '\\' :
	    sprintf(buf, "\\\\");
	    break;
	  case '\"' :
	    sprintf(buf, "\\\"");
	    break;
	  case '\'' :
	    sprintf(buf, "\\\'");
	    break;
	  default:
	    buf[0] = c;
	    buf[1] = '\0';
	    break;
	  }
      else
	sprintf(buf, "\\%03o", c);
      break;
    }
  return buf;
}



char *word2str(uchar* s)
{
  static char buf[1024];
  char *p = buf;
  int i,len = *s++;
#ifdef SUNOS5
  memset(buf, 0, sizeof(buf));
#else
  bzero(buf, sizeof(buf));
#endif
  for (i=0; i < len; i++)
    {
      strcpy(p, char2str(s[i]));
      p = strchr(p, '\0');
    }
  return buf;
}
