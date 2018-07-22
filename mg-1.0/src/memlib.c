/**************************************************************************
 *
 * memlib.c -- Malloc wrappers
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
 *       @(#)memlib.c	1.3 16 Mar 1994
 *
 **************************************************************************/
 
char *SCCS_Id_memlib = "@(#)memlib.c	1.3 16 Mar 1994";


#include <string.h>
#include <stdlib.h>

#include "memlib.h"
#include "sysfuncs.h"

Malloc_func Xmalloc = malloc;

Realloc_func Xrealloc = realloc;

Free_func Xfree = free;

Strdup_func Xstrdup = strdup;


