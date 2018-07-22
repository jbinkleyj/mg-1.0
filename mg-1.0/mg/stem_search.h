/**************************************************************************
 *
 * stem_search.h -- Functions for searching the blocked stemmed dictionary
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
 *       @(#)stem_search.h	1.5 16 Mar 1994
 *
 **************************************************************************/

#ifndef H_STEM_SEARCH
#define H_STEM_SEARCH

stemmed_dict *ReadStemDictBlk(File *stem_file);

int FindWord(stemmed_dict *sd, uchar *Word, unsigned long *count, 
	     unsigned long *doc_count, unsigned long *invf_ptr,
	     unsigned long *invf_len);

void FreeStemDict(stemmed_dict *sd);

#endif
