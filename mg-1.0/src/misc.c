/**************************************************************************
 *
 * misc.c -- Misc functions
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
 *       @(#)misc.c	1.3 16 Mar 1994
 *
 **************************************************************************/


char *SCCS_Id_misc = "@(#)misc.c	1.3 16 Mar 1994";

#include "misc.h"

/*
 * Compare two strings, return -ve if first is <, 0 if =, +ve if >.
 * Strings are binary, start with a byte indicating how many data bytes
 * follow.
 */

int compare(register uchar *s1, register uchar *s2 )
{
  register int l1= *s1, l2= *s2;
  register int l = (l1<l2) ? l1 : l2;
  
  while (l--)
    if (*++s1 != *++s2)
      return( *s1 - *s2);
  return( l1 - l2 );
}



int prefixlen(register uchar *s1, register uchar *s2)
{
  register int l = (*s1<*s2) ? *s1 : *s2;
  register int i=0;
  
  while (i<l && *++s1 == *++s2)
    i++;
  return( i );
}
