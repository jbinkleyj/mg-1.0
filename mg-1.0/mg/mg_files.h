/**************************************************************************
 *
 * mg_files.h -- Routines for handling files for the auxillary programs
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
 *       @(#)mg_files.h	1.7 16 Mar 1994
 *
 **************************************************************************/

#ifndef MG_FILES_H
#define MG_FILES_H



#include <sys/types.h>


/* Magic numbers for the different types of files */

#define GEN_MAGIC(a,b,c,d) ((unsigned long)(((a)<<24) + ((b)<<16) + \
					    ((c)<<8) + (d)))

#define MAGIC_XXXX		GEN_MAGIC('M','G', 0 , 0)
#define MAGIC_STATS_DICT	GEN_MAGIC('M','G','S','D')
#define MAGIC_AUX_DICT		GEN_MAGIC('M','G','A','D')
#define MAGIC_FAST_DICT		GEN_MAGIC('M','G','F','D')
#define MAGIC_DICT		GEN_MAGIC('M','G','D', 0 )
#define MAGIC_STEM_BUILD	GEN_MAGIC('M','G','S', 0 )
#define MAGIC_HASH		GEN_MAGIC('M','G','H', 0 )
#define MAGIC_STEM		GEN_MAGIC('M','G','s', 0 )
#define MAGIC_CHUNK		GEN_MAGIC('M','G','C', 0 )
#define MAGIC_CHUNK_TRANS	GEN_MAGIC('M','G','c', 0 )
#define MAGIC_TEXT		GEN_MAGIC('M','G','T', 0 )
#define MAGIC_TEXI		GEN_MAGIC('M','G','t', 0 )
#define MAGIC_TEXI_WGT		GEN_MAGIC('M','G','t','W')
#define MAGIC_INVF		GEN_MAGIC('M','G','I', 0 )
#define MAGIC_INVI		GEN_MAGIC('M','G','i', 0 )
#define MAGIC_WGHT		GEN_MAGIC('M','G','W', 0 )
#define MAGIC_WGHT_APPROX	GEN_MAGIC('M','G','w', 0 )
#define MAGIC_PARAGRAPH		GEN_MAGIC('M','G','P', 0 )
#define MAGIC_FELICS            GEN_MAGIC('M','G','F','X')
#define MAGIC_BILEVEL           GEN_MAGIC('M','G','B','I')
#define MAGIC_TIC               GEN_MAGIC('M','G','T','C')

#define IS_MAGIC(a) ((((u_long)(a)) & 0xffff0000) == MAGIC_XXXX)


/* err_mode values for open_file and create_file */
#define MG_ABORT 0
#define MG_MESSAGE 1
#define MG_CONTINUE 2





/* File suffixes */


/* The compression dictionary built by txt.pass1 */
#define TEXT_STATS_DICT_SUFFIX	 ".text.stats"

/* The compression dictionary built by text.pass1 and comp_dict.process */
#define TEXT_DICT_SUFFIX	 ".text.dict"

/* The compression dictionary built by mg_make_fast_dict */
#define TEXT_DICT_FAST_SUFFIX	 ".text.dict.fast"

/* The auxilary dictionary built by text.pass2 */
#define TEXT_DICT_AUX_SUFFIX	 ".text.dict.aux"

/* The compressed text build by text.pass2 */
#define TEXT_SUFFIX		 ".text"

/* The combined compressed text index and document weight file */
#define TEXT_IDX_WGT_SUFFIX	 ".text.idx.wgt"

/* The compressed text index file */
#define TEXT_IDX_SUFFIX	 	".text.idx"

/* The dictionary of stemmed words build by invf.pass1 and ivf.pass1 */
#define INVF_DICT_SUFFIX         ".invf.dict"

/* The dictionary of stemmed words build by stem.process */
#define INVF_DICT_BLOCKED_SUFFIX ".invf.dict.blocked"

/* The exact document weights file build by make.weights, invf.pass2,
   or ivf.pass2 */
#define WEIGHTS_SUFFIX           ".weight"

/* The approximate weights file built by make.weights */
#define APPROX_WEIGHTS_SUFFIX    ".weight.approx"

/* The inverted file build by invf.pass2 or ivf.pass2 */
#define INVF_SUFFIX              ".invf"

/* The inverted file index build by invf.pass2 or ivf.pass2 */
#define INVF_IDX_SUFFIX          ".invf.idx"

/* The inverted file chunk descriptor built by ivf.pass1 */
#define INVF_CHUNK_SUFFIX        ".invf.chunk"

/* The word index translation file built by ivf.pass1 */
#define INVF_CHUNK_TRANS_SUFFIX  ".invf.chunk.trans"

/* The hashed stemmed dictionary built by make.perf_hash */
#define INVF_DICT_HASH_SUFFIX    ".invf.dict.hash"

/* The paragraph descriptior file built by invf.pass1 or ivf.pass1 */
#define INVF_PARAGRAPH_SUFFIX    ".invf.paragraph"

/* The trace file build by mg.builder. */
#define TRACE_SUFFIX             ".trace"

/* The compression stats file build by mg.builder. */
#define COMPRESSION_STATS_SUFFIX ".compression.stats"






/* This sets the base path for all file operations */
void set_basepath(const char *bp);


/* return the currently defined basepath */
char *get_basepath(void);




/* This generates the name of a file. It places the name in the buffer
   specified or if that is NULL it uses a static buffer. */
char *make_name(const char *name, const char *suffix, char *buffer);







/* This will open the specified file and check its magic number.
   Mode may take on the following values
      MG_ABORT    : causes an error message to be generated and the
                    program aborted if there is an error.
      MG_MESSAGE  : causes a message to be generated and a NULL value to
                    be returned if there is an error.
      MG_CONTINUE : causes a NULL value to be returned if there is an error.

   On success if returns the FILE *. On failure it will return a NULL value
   and possibly generate an error message, or it will exit the program with
   an error message.   */
FILE *open_named_file(const char *name, const char *mode,
		u_long magic_num, int err_mode);




/* This will open the specified file and check its magic number.

   err_mode may take on the following values
      MG_ABORT    : causes an error message to be generated and the
                     program aborted if there is an error.
      MG_MESSAGE  : causes a message to be generated and a NULL value to
                     be returned if there is an error.
      MG_CONTINUE : causes a NULL value to be returned if there is an error.

   On success if returns the FILE *. On failure it will return a NULL value
   and possibly generate an error message, or it will exit the program with
   an error message.   */
FILE *open_file(const char *name, const char *suffix, const char *mode,
		u_long magic_num, int err_mode);





/* This will create the specified file and set its magic number.

   Mode may take on the following values
      MG_ABORT    : causes an error message to be generated and the
                    program aborted if there is an error.
      MG_MESSAGE  : causes a message to be generated and a NULL value to
                    be returned if there is an error.
      MG_CONTINUE : causes a NULL value to be returned if there is an error.

   On success if returns the FILE *. On failure it will return a NULL value
   and possibly generate an error message, or it will exit the program with
   an error message.   */
FILE *create_named_file(const char *name, const char *mode, 
		  u_long magic_num, int err_mode);



/* This will create the specified file and set its magic number.

   err_mode may take on the following values
      MG_ABORT    : causes an error message to be generated and the
                     program aborted if there is an error.
      MG_MESSAGE  : causes a message to be generated and a NULL value to
                     be returned if there is an error.
      MG_CONTINUE : causes a NULL value to be returned if there is an error.

   On success if returns the FILE *. On failure it will return a NULL value
   and possibly generate an error message, or it will exit the program with
   an error message.   */
FILE *create_file(const char *name, const char *suffix, const char *mode, 
		  u_long magic_num, int err_mode);




#endif

