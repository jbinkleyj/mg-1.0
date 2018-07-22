/**************************************************************************
 *
 * backend.h -- Underlying routines and datastructures for mgquery
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
 *       @(#)backend.h	1.8 16 Mar 1994
 *
 **************************************************************************/

typedef struct WordEntry
{
  int word_num; /* Unique number for each different word */
  unsigned long count; /* Number of time the word occurs in the text */
  unsigned long doc_count; /* Number of documents that contain the word */
  unsigned long invf_ptr; /* This is a byte position of the  
			     inverted file entry corresponding to the word */
  unsigned long invf_len; /* This is the length of the inverted 
			     file entry in bytes */
} WordEntry;



typedef struct TermEntry
{
  WordEntry WE;
  int Count; /* The number of times the word occurs in the query */
  uchar *Word; /* The word. */
} TermEntry;



typedef struct TermList
{
  int num;
  TermEntry TE[1];
} TermList;



typedef struct invf_data
{
  File *InvfFile;
  unsigned long N;
  struct invf_file_header ifh;
} invf_data;

typedef struct text_data
{
  File *TextFile;
  File *TextIdxFile;
  File *TextIdxWgtFile;
  long current_pos;
  struct
    {
      unsigned long Start;
      float Weight;
    } *idx_data;
  compressed_text_header cth;
} text_data;


typedef struct auxiliary_dict
{
  aux_frags_header afh[2];
  uchar *word_data[2];
  uchar **words[2];
  int blk_start[2][33], blk_end[2][33]; /* blk_start and blk_end are required
					   for the hybrid methods */
} auxiliary_dict;


typedef struct compression_dict
{
  compression_dict_header cdh;
  comp_frags_header *cfh[2];
  unsigned long MemForCompDict;
  uchar ***values[2];
  uchar *escape[2];
  huff_data *chars_huff[2];
  u_long **chars_vals[2];
  huff_data *lens_huff[2];
  u_long **lens_vals[2];
  auxiliary_dict *ad;
  Bool fast_loaded;
} compression_dict;

typedef struct stemmed_dict
{
  File *stem_file;
  struct stem_dict_header sdh;
  uchar **index;
  unsigned long *pos;
  int active;
  uchar *buffer;
  unsigned long MemForStemDict;
} stemmed_dict;



typedef struct approx_weights_data
{
  double L;
  double B;
  unsigned long *DocWeights;
  char bits;
  float *table;
  unsigned long mask;
  unsigned long MemForWeights;
  unsigned long num_of_docs;
} approx_weights_data;



typedef struct RankedQueryInfo
{
  Bool QueryFreqs;
  Bool Exact;
  long MaxDocsToRetrieve;  /* may be -1 for all */
  long MaxParasToRetrieve;
  Bool Sort;
  char AccumMethod; /* 'A' = array,  'S' = splay tree,  'H' = hash_table */
  long MaxAccums; /* may be -1 for all */
  long MaxTerms;  /* may be -1 for all */
  Bool StopAtMaxAccum;
  long HashTblSize;
  char *skip_dump;
} RankedQueryInfo;



typedef struct BooleanQueryInfo
{
  long MaxDocsToRetrieve;
} BooleanQueryInfo;



typedef struct query_data
{
  stemmed_dict *sd;
  compression_dict *cd;
  approx_weights_data *awd;
  invf_data *id;
  text_data *td;
  char *pathname;
  File *File_text;
  File *File_comp_dict;
  File *File_aux_dict;
  File *File_fast_comp_dict;
  File *File_text_idx_wgt;
  File *File_text_idx;
  File *File_stem;
  File *File_invf;
  File *File_weight_approx;
  unsigned long mem_in_use, max_mem_in_use;
  unsigned long num_of_ptrs, tot_num_of_ptrs;
  unsigned long num_of_terms, tot_num_of_terms;
  unsigned long num_of_accum, tot_num_of_accum;
  unsigned long num_of_ans, tot_num_of_ans;
  unsigned long hops_taken, tot_hops_taken;
  unsigned long text_idx_lookups, tot_text_idx_lookups;
  unsigned long max_buffers;
  unsigned doc_pos;
  unsigned buf_in_use;
  DocList *DL;
  uchar *TextBuffer;
  int TextBufferLen;
} query_data;



typedef struct InitQueryTimes
{
  ProgTime Start;
  ProgTime StemDict;
  ProgTime ApproxWeights;
  ProgTime CompDict;
  ProgTime Invf;
  ProgTime Text;
} InitQueryTimes;



query_data *InitQuerySystem(char *dir, char *name, InitQueryTimes *iqt);

void ChangeMemInUse(query_data *qd, long delta);

void FinishQuerySystem(query_data *qd);

void ResetFileStats(query_data *qd);

void TransFileStats(query_data *qd);

void ChangeMemInUse(query_data *qd, long delta);

void RankedQuery(query_data *qd, char *Query, RankedQueryInfo *rqi);

void BooleanQuery(query_data *qd, char *Query, BooleanQueryInfo *bqi);

void DocnumsQuery (query_data *qd, char *QueryLine);

void FreeTextBuffer(query_data *qd);

void FreeQueryDocs(query_data *qd);

int LoadCompressedText(query_data *qd, int max_mem);

int GetDocNum(query_data *qd);

float GetDocWeight(query_data *qd);

long GetDocCompLength(query_data *qd);

uchar *GetDocText(query_data *qd, unsigned long *len);

DocEntry *GetDocChain(query_data *qd);

int NextDoc(query_data *qd);
