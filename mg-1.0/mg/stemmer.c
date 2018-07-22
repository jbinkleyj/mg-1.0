/**************************************************************************
 *
 * stemmer.c -- The stemmer/case folder
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
 *       @(#)stemmer.c	1.4 16 Mar 1994
 *
 **************************************************************************/


#include "stemmer.h"
#include <ctype.h>
 
char *SCCS_Id_stemmer = "@(#)stemmer.c	1.4 16 Mar 1994";
 
/*
 * Method 0 - Do not stem or case fold.
 * Method 1 - Case fold.
 * Method 2 - Stem.
 * Method 3 - Case fold and stem.
 */
void stemmer(int method, uchar* word)
{
  if (method & 1)
    {
      int len = *word;
      char *w = (char*)word+1;
      for(; len; len--, w++)
	*w = tolower(*w);
    }
  if (method & 2)
    stem(word);
}
