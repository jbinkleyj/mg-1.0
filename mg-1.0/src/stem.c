/**************************************************************************
 *
 * stem.c -- Stemming code
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
 *       @(#)stem.c	1.5 16 Mar 1994
 *
 **************************************************************************/
 
char *SCCS_Id_stem = "@(#)stem.c	1.5 16 Mar 1994";


#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>

#include	"misc.h"
#include	"stem.h"

/*
 * This function stems a word.
 * The first character of the uchar* is the words length, the remaining
 * characters form the word. The string need not be NULL terminated.
 *
 * Due to Copyright problems the stemmer used here is not the same one
 * as described in the book. The stemmer here is a very simple one,
 * which we hope to replace with something better at a later date.
 *
 */
void stem(uchar *word)
{
  char *w, *last;
  int len;
  len = *word;

  /* The word must be at least 3 characters long. */
  if (len < 3) return;

  w = (char*)word + 1;
  last = w+len-1;

  /* If it dosen't end with a s then don't stem it. */
  if (*last != 's' && *last != 'S') return;

  last--;
  if (*last == 'u' || *last == 'U' || 
      *last == 's' || *last == 'S') return;

  if (*last != 'e' && *last != 'E')
    {
      (*word)--;
      return;
    }
  
  /* The word ends in 'es' */
  /* The word must be at least 4 characters long. */
  if (len < 4) return;

  last--;

  if (*last == 'a' || *last == 'A' || 
      *last == 'e' || *last == 'E' || 
      *last == 'o' || *last == 'O') return;

  if (*last != 'i' && *last != 'I')
    {
      (*word)--;
      return;
    }

  /* The word ends in 'ies' */
  /* The word must be at least 5 characters long. */
  if (len < 5) return;

  last--;

  if (*last == 'e' || *last == 'E' ||
      *last == 'a' || *last == 'A') return;

  last++;
  *last = isupper(*last) ? 'Y' : 'y';
  (*word) -= 2;
  return;
 }



#if 0
void main()
{
  char buf[256];
  while (gets(buf))
    {
      uchar b[256];
      b[0] = strlen(buf);
      strcpy((char*)b+1, buf);
      printf("\"%.*s\"  ->  ", *b, b+1);
      stem(b);
      printf("\"%.*s\"\n", *b, b+1);
    }
}
#endif
