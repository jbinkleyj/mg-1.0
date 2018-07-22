/**************************************************************************
 *
 * invf_get.h -- Inverted file reading routines
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
 *       @(#)invf_get.h	1.6 16 Mar 1994
 *
 **************************************************************************/


#ifndef H_INVF_GET
#define H_INVF_GET

typedef struct Invf_Doc_Entry
{
  int DocNum;
  float Sum;
} Invf_Doc_Entry;

typedef struct Invf_Doc_Entry_Pool
{
  void *pool;
  unsigned pool_chunks;
} Invf_Doc_Entry_Pool;

typedef struct Invf_Doc_EntryH
{
  Invf_Doc_Entry IDE;
  struct Invf_Doc_EntryH *next;
} Invf_Doc_EntryH;

typedef struct Hash_Table
{
  Invf_Doc_EntryH *Suplimentary_Entries;
  int Suplimentary_Size;
  int Suplimentary_Num;
  unsigned max;
  unsigned num;
  unsigned size;
  Invf_Doc_EntryH **HashTable;
  Invf_Doc_EntryH *Head;
  Invf_Doc_EntryH IDEH[1];
} Hash_Table;

typedef struct List_Table
{
  unsigned max;
  unsigned num;
  Invf_Doc_Entry IDE[1];
} List_Table;


/*Invf_Doc_Entry *HT_find(Hash_Table *HT, unsigned long DN);*/
void HT_free(query_data *qd, Hash_Table *HT);
void LT_free(query_data *qd, List_Table *LT);

invf_data *InitInvfFile(File *InvfFile, stemmed_dict *sd);
void FreeInvfData(invf_data *id);

DocList *GetDocs(query_data *qd, WordEntry *we, BooleanQueryInfo *bqi);

float *CosineDecode(query_data *qd, TermList *Terms, RankedQueryInfo *rqi);

void free_ide_pool(query_data *qd, Invf_Doc_Entry_Pool *pool);

Splay_Tree *CosineDecodeSplay(query_data *qd, TermList *Terms, 
			      RankedQueryInfo *rqi, Invf_Doc_Entry_Pool *pool);

Hash_Table *CosineDecodeHash(query_data *qd, TermList *Terms, 
				      RankedQueryInfo *rqi);
List_Table *CosineDecodeList(query_data *qd, TermList *Terms, 
			     RankedQueryInfo *rqi);


DocList *GetDocsOp(query_data *qd, WordEntry *we, int op, DocList *L);



#endif
