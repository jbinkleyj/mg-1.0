/**************************************************************************
 *
 * words.h -- Macros for parsing out words from the source text
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
 *       @(#)words.h	1.5 16 Mar 1994
 *
 **************************************************************************/

#define MAXWORDLEN	15
	/* Maximum length in bytes of any word or non-word. Note that
	   variations to MAXWORDLEN may have dramatic effects on the rest
	   of the program, as the length and the prefix match are packed
	   together into a four bit nibble, and there is not check that
	   this is possible, i.e., leave MAXWORDLEN alone... */
#define MAXSTEMLEN	255
	/* Maximum length in bytes of any stem. Note that
	   variations to MAXSTEMLEN may have dramatic effects on the rest
	   of the program, , i.e., leave MAXSTEMLEN alone... */
#define MAXLONGWORDLEN	8191
	/* Maximum length in bytes of any word. Note that
	   variations to MAXLONGWORDLEN may have dramatic effects on the rest
	   of the program. i.e., leave MAXLONGWORDLEN alone... */
#define MAXNUMERIC	4
	/* Maximum number of numeric characters permitted in a word.
	   This avoids long sequences of numbers creating just one
	   word occurrence for each number. At most 10,000 all numeric
	   words will be permitted. */

#define	INAWORD(c)	(isascii(c) && isalnum(c))
	/* The definition of what characters are permitted in a word
	*/

#define PARSE_WORD(Word, s_in, end, which, chbuf) 		\
	do {							\
		register uchar  *wptr = (Word)+1;		\
		register int    length = 0;			\
		register int    c = chbuf;			\
		register int	numeric = 0;			\
								\
		for ( ; length < MAXWORDLEN &&			\
			INAWORD(c)==which && s_in<=end;		\
				c = *s_in++ ) {			\
			if ((numeric += (isascii(c) &&		\
				isdigit(c))) > MAXNUMERIC)	\
					break;			\
			*wptr++ = c;				\
			length++;				\
		}						\
		*(Word) = length;				\
		chbuf = c;					\
	} while (0)

#define PARSE_LONG_WORD(Word, s_in, end, chbuf) 	          \
  do                                                              \
    {							          \
      register uchar  *wptr = (Word)+1;			          \
      register int    length = 0;			          \
      register int    c = chbuf;			          \
      register int    numeric = 0;			          \
								  \
      while ( length < MAXLONGWORDLEN && INAWORD(c) && s_in<=end) \
        {                                                         \
 	  if ((numeric += (isascii(c) && isdigit(c)))             \
	      > MAXNUMERIC)	                                  \
		break;			                          \
	  *wptr++ = c;				                  \
	  length++;				                  \
	  c = *s_in++;			                          \
	}						          \
      *(Word) = length;					          \
      chbuf = c;					          \
    } while (0)

#define PARSE_NON_WORD(s_in, end, chbuf) 	                  \
  do                                                              \
    {							          \
      register int    c = chbuf;			          \
								  \
      while (!INAWORD(c) && s_in<=end)                            \
	c = *s_in++;			                          \
      chbuf = c;					          \
    } while (0)

#define PARSE_NON_WORD_OR_SGML_TAG(s_in, end, chbuf) 	          \
  do                                                              \
    {							          \
      register int    c = chbuf;			          \
								  \
      while (!INAWORD(c) && s_in<=end)                            \
        {							  \
	  if (c == '<' && s_in<=end)				  \
	    while (c != '>' && s_in<=end)                         \
	      c = *s_in++;		                          \
	  if (s_in<=end)					  \
	    c = *s_in++;			                  \
	}							  \
      chbuf = c;					          \
    } while (0)

