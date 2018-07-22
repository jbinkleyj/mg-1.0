/**************************************************************************
 *
 * stem.h -- Stemming code
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
 *       @(#)stem.h	1.2 09 Mar 1994
 *
 **************************************************************************/

#ifndef	STEM_H
#define STEM_H


/*
 * This function stems a word.
 * The first character of the uchar* is the words length, the remaining
 * characters form the word. The string need not be NULL terminated.
 */
void stem(uchar *s);


#endif
