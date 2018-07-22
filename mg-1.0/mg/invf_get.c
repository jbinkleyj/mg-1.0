/**************************************************************************
 *
 * invf_get.c -- Inverted file reading routines
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
 *       @(#)invf_get.c	1.7 16 Mar 1994
 *
 **************************************************************************/

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include "sysfuncs.h"
#include "memlib.h"
#include "filestats.h"
#include "timing.h"
#include "bitio_gen.h"
#include "bitio_m_mem.h"
#include "bitio_m.h"
#include "sptree.h"
#include "messages.h"

#include "mg.h"
#include "invf.h"
#include "text.h"
#include "lists.h"
#include "backend.h"
#include "invf_get.h"
#include "mg_errors.h"

#define SKIPPING
#define SHORT_CUT

struct pool {
  int length, pos;
  struct pool *next;
  Invf_Doc_Entry ide[1];
};

static Bool skip_header = FALSE;

static char skip_header_text0[] = 
"#######################################################################\n"
"#\n"
"# 1\tWord number.\n"
"# 2\tNumber of time the word occurs in the collection.\n"    
"# 3\tNumber of documents the word occurs in.\n"    
"# 4\tThe word.\n"    
"# 5\tNumber of docnum/word_count entries decoded.\n"    
"# 6\tNumber of hits.\n"    
"# 7\tNumber of entries in the splay tree or hash table.\n"    
"# 8\tAmount added to the accumulators while while processing this word.\n"
"#\n"
"#######################################################################\n";

static char skip_header_text1[] = 
"#######################################################################\n"
"#\n"
"# 1\tWord number.\n"
"# 2\tNumber of time the word occurs in the collection.\n"    
"# 3\tNumber of documents the word occurs in.\n"    
"# 4\tThe word.\n"    
"# 5\tNumber of skips taken in evaluating the inverted file entry.\n"    
"# 6\tNumber of docnum/word_count entries decoded.\n"    
"# 7\tNumber of hits.\n"    
"# 8\tNumber of entries in the splay tree or hash table.\n"    
"# 9\tAmount added to the accumulators while while processing this word.\n"
"#\n"
"#######################################################################\n";


static char skip_header_text2[] = 
"#######################################################################\n"
"#\n"
"#  1\tWord number.\n"
"#  2\tNumber of time the word occurs in the collection.\n"    
"#  3\tNumber of documents the word occurs in.\n"    
"#  4\tThe word.\n"    
"#  5\tSkip size.\n"
"#  6\tNumber of skips taken in evaluating the inverted file entry.\n"    
"#  7\tNumber of docnum/word_count entries decoded.\n"    
"#  8\tNumber of hits.\n"    
"#  9\tNumber of entries in the splay tree or hash table.\n"    
"# 10\tAmount added to the accumulators while while processing this word.\n"
"#\n"
"#######################################################################\n";

static char *skip_header_text[] = {skip_header_text0, 
				   skip_header_text1, 
				   skip_header_text2};








/**************************************************************************
 *
 * Code for the hash table.
 *
 */

static Hash_Table *HT_create(query_data *qd, unsigned max_entries, 
			     unsigned max_size)
{
  unsigned es, hts;
  Hash_Table *HT;
  if (max_entries == 0)
    max_entries = 8192;
  es = sizeof(Hash_Table) + sizeof(Invf_Doc_EntryH) * (max_entries-1);
  hts = sizeof(Invf_Doc_EntryH *) * max_size;

  HT = Xmalloc(es);
  if (!HT) return NULL;
  
  HT->HashTable = Xmalloc(hts);
  if (!(HT->HashTable))
    {
      Xfree(HT);
      return NULL;
    }

  ChangeMemInUse(qd, es+hts); 
#ifdef SUNOS5
  memset(HT->HashTable, 0, hts);
#else
  bzero((char*)(HT->HashTable), hts);
#endif
  HT->max = max_entries;
  HT->num = 0;
  HT->Suplimentary_Entries = NULL;
  HT->Suplimentary_Size = 0;
  HT->Suplimentary_Num = 0;
  HT->Head = NULL;
  HT->size = max_size;
  return HT;
}


static void HT_Sup_create(query_data *qd, Hash_Table *HT, unsigned max_size)
{
  int es = sizeof(Invf_Doc_EntryH) * max_size;
  HT->Suplimentary_Entries = Xmalloc(es);
  if (!HT->Suplimentary_Entries) return;
  HT->Suplimentary_Size = max_size;
  HT->Suplimentary_Num = 0;
  ChangeMemInUse(qd, es); 
}

static void HT_Sup_Add(Hash_Table *HT, unsigned long DN, float Sum)
{
  Invf_Doc_EntryH *IDEH;
  IDEH = &HT->Suplimentary_Entries[HT->Suplimentary_Num++];
  IDEH->next = NULL;
  IDEH->IDE.DocNum = DN;
  IDEH->IDE.Sum = Sum;
}

Invf_Doc_Entry *HT_find(Hash_Table *HT, unsigned long DN)
{
  Invf_Doc_EntryH *IDEH;
  if (!HT->HashTable)
    FatalError(1, "Accessing the hash_table through HT_find after HT_sort");
  IDEH = HT->HashTable[DN % HT->size];
  while (IDEH)
    {
      if (IDEH->IDE.DocNum == DN) return &(IDEH->IDE);
      IDEH = IDEH->next;
    }
  return NULL;
}


static Bool HT_insert(Hash_Table *HT, 
		      unsigned long DN, float Sum)
{
  Invf_Doc_EntryH **IDEH;
  if (HT->num == HT->max)
    return FALSE;
  IDEH = &(HT->HashTable[DN % HT->size]);
  while (*IDEH) IDEH = &((*IDEH)->next);
  *IDEH = &(HT->IDEH[HT->num++]);
  (*IDEH)->next = NULL;
  (*IDEH)->IDE.DocNum = DN;
  (*IDEH)->IDE.Sum = Sum;
  return TRUE;
}


void HT_free(query_data *qd, Hash_Table *HT)
{
  unsigned es;
  es = sizeof(Hash_Table) + sizeof(Invf_Doc_EntryH) * HT->max +
    sizeof(Invf_Doc_EntryH *) * HT->size;
  if (HT->HashTable)
    Xfree(HT->HashTable);
  if (HT->Suplimentary_Entries)
    {
      Xfree(HT->Suplimentary_Entries);
      es += sizeof(Invf_Doc_EntryH) * HT->Suplimentary_Size;
    }
  Xfree(HT);
  ChangeMemInUse(qd, -es); 
}


static int IDEH_comp(const void *a, const void *b)
{
  const Invf_Doc_EntryH *A = a;
  const Invf_Doc_EntryH *B = b;
  return A->IDE.DocNum - B->IDE.DocNum;
}

static void HT_sort(query_data *qd, Hash_Table *HT)
{
  int i;
  Invf_Doc_EntryH *l, *r, **b;
  unsigned es = sizeof(Invf_Doc_EntryH *) * HT->size;
  if (HT->HashTable)
    Xfree(HT->HashTable);
  HT->size = 0;
  HT->HashTable = NULL;
  ChangeMemInUse(qd, -es); 

  qsort(HT->IDEH, HT->num, sizeof(Invf_Doc_EntryH), IDEH_comp);

  for (i = 0; i < HT->num - 1; i++)
    HT->IDEH[i].next = &HT->IDEH[i+1];
  HT->IDEH[i].next = NULL;
  l = HT->IDEH;

  if (HT->Suplimentary_Entries)
    {
      for (i = 0; i < HT->Suplimentary_Num - 1; i++)
	HT->Suplimentary_Entries[i].next = &HT->Suplimentary_Entries[i+1];
      HT->Suplimentary_Entries[i].next = NULL;
    }
  r = HT->Suplimentary_Entries;

  b = &HT->Head;

  while (l || r)
    {
      if (r && (!l || (l->IDE.DocNum > r->IDE.DocNum)))
	{
	  *b = r;
	  b = &r->next;
	  r = *b;
	}
      else
	{
	  *b = l;
	  b = &l->next;
	  l = *b;
	}
    }
  *b = NULL;
}






/**************************************************************************
 *
 * Code for the hash table.
 *
 */

static List_Table *LT_create(query_data *qd, unsigned max_entries)
{
  unsigned es;
  List_Table *LT;
  if (max_entries == 0)
    max_entries = 8192;
  es = sizeof(List_Table) + sizeof(Invf_Doc_Entry) * (max_entries-1);

  LT = Xmalloc(es);
  if (!LT) return NULL;
  
  ChangeMemInUse(qd, es); 
  LT->max = max_entries;
  LT->num = 0;
  return LT;
}



Invf_Doc_Entry *LT_find(List_Table *LT, unsigned long DN)
{
  int l = 0;
  int r = LT->num-1;
  int m, c;
  while (l <= r)
    {
      m = (l+r)/2;
      c = DN - LT->IDE[m].DocNum;
      if (c < 0)
	r = m-1;
      else
	if (c > 0)
	  l = m + 1;
	else
	  return &LT->IDE[m];
    }
  return NULL;
}

static List_Table *LT_add(query_data *qd, List_Table *LT, 
			     unsigned long DN, float Sum)
{
  Invf_Doc_Entry *ide;
  if (LT->num == LT->max) 
    {
      int old, new, max_entries;
      old = sizeof(List_Table) + sizeof(Invf_Doc_Entry) * LT->max;
      max_entries = LT->max + (LT->max>>1);
      new = sizeof(List_Table) + sizeof(Invf_Doc_Entry) * max_entries;

      LT = Xrealloc(LT, new);
      if (!LT) return NULL;
  
      ChangeMemInUse(qd, new-old); 
      LT->max = max_entries;
    }
  ide = &LT->IDE[LT->num];
  ide->DocNum = DN;
  ide->Sum = Sum;
  LT->num++;
  return LT;
}


void LT_free(query_data *qd, List_Table *LT)
{
  unsigned es;
  es = sizeof(List_Table) + sizeof(Invf_Doc_Entry) * LT->max;
  Xfree(LT);
  ChangeMemInUse(qd, -es); 
}


static int IDE_comp(const void *a, const void *b)
{
  const Invf_Doc_Entry *A = a;
  const Invf_Doc_Entry *B = b;
  return A->DocNum - B->DocNum;
}

static void LT_sort(List_Table *LT)
{
  qsort(LT->IDE, LT->num, sizeof(Invf_Doc_Entry), IDE_comp);
}

static void LT_pack(List_Table *LT)
{
  int s, d = 0;
  for (s = 1; s < LT->num; s++)
    {
      if (LT->IDE[s].DocNum == LT->IDE[d].DocNum)
	LT->IDE[d].Sum += LT->IDE[s].Sum;
      else
	{
	  d++;
	  LT->IDE[d] = LT->IDE[s];
	}
    }
  if (LT->num)
    LT->num = d+1;
}
    

/**************************************************************************/



invf_data *InitInvfFile(File *InvfFile, stemmed_dict *sd)
{
  invf_data *id;
  if (!(id = Xmalloc(sizeof(invf_data))))
    { mg_errno = MG_NOMEM; return NULL; }

  id->InvfFile = InvfFile;

  Fread(&id->ifh, sizeof(id->ifh), 1, InvfFile);

  id->N = sd->sdh.num_of_docs;
  
  return(id);
}


void FreeInvfData(invf_data *id)
{
  Xfree(id);
}

/****************************************************************************/


static void CalcBlks(query_data *qd, WordEntry *we, int *blk, 
		     int *dn_blk, int *len_blk, int *k, int *p)
{
  *p = we->doc_count;

  *blk = BIO_Bblock_Init (qd->id->N, *p);
  switch (qd->id->ifh.skip_mode)
    {
    case 1 :
      {
	unsigned long len;
	if (*p <= qd->id->ifh.params[0])
	  *k = 0;
	else
	  {
	    *k = qd->id->ifh.params[0];
	    len = BIO_Bblock_Bound(qd->id->N, *p);
	    if (qd->id->ifh.InvfLevel >= 2)
	      len += we->count;
	    *dn_blk = BIO_Bblock_Init(qd->sd->sdh.num_of_docs, (*p+*k-1) / *k);
	    *len_blk = BIO_Bblock_Init(len, (*p+*k-1) / *k);
	  }
	break;
      }
    case 2 :
      {
	unsigned long len;
	if (*p <= qd->id->ifh.params[1])
	  *k = 0;
	else
	  {
	    *k = (int)(2*sqrt(((double)(*p)) / qd->id->ifh.params[0]));
	    if (*k <= qd->id->ifh.params[1])
	      *k = qd->id->ifh.params[1];
	    len = BIO_Bblock_Bound(qd->id->N, *p);
	    if (qd->id->ifh.InvfLevel >= 2)
	      len += we->count;
	    *dn_blk = BIO_Bblock_Init(qd->sd->sdh.num_of_docs, 
				      (*p+*k-1) / *k);
	    *len_blk = BIO_Bblock_Init(len, (*p+*k-1) / *k);
	  }
	break;
      }
    default:
      *k = 0;
    }
}



/*
 * if op = 0 then getdocs(term)
 * if op = 1 then getdocs( L and term)
 * if op = 2 then getdocs( L - term)    >>>> NOT tested <<<<
 *
 */
DocList *GetDocsOp(query_data *qd, WordEntry *we, int op, DocList *L)
{
  uchar *buffer;
  DocEntry *src = NULL, *dest = NULL;
  int i;
  unsigned long pos;
  int next_start, next_doc_num, next_i, kd;
  int dn_blk=0, len_blk=0, k;
  int blk; /* bblock control parameter */
  int p; /* The number of documents the word occurs in */
  unsigned long CurrDoc = 0; /* NOTE: Document numbers start at 1 */
  float Weight;

  /* ... AND empty */
  if (op == 1 && (!L || !L->num))
    {
      if (L)
	Xfree(L);
      return MakeDocList(0);
    }
  
  if (!op)
    L = MakeDocList(we->doc_count);
  
  CalcBlks(qd, we, &blk, &dn_blk, &len_blk, &k, &p);
  if (!(buffer = Xmalloc(we->invf_len)))
    return MakeDocList(0);

  ChangeMemInUse(qd, we->invf_len); 

  qd->num_of_terms++;

  Fseek(qd->File_invf, we->invf_ptr, 0);
  Fread(buffer, sizeof(char), we->invf_len, qd->File_invf);

  /* Read from the buffer */
  DECODE_START(buffer, we->invf_len);

  Weight = qd->sd->sdh.num_of_words / (100 * log2 ((float)we->count + 1));

  if (op)
    dest = src = L->DE;
  i = pos = 0;
  next_i = next_start = next_doc_num = 0;
  kd = k-1;
  while(i<p && (!op || src - L->DE < L->num))
    {
      unsigned long diff, Temp;
      if (k)
	{
	  if (op && i+k < p && src->DocNum > next_doc_num && next_doc_num >= 0)
	    {
#ifdef SHORT_CUT
	      ShortCut:
#endif
	      i = next_i;
	      DECODE_SEEK(next_start);
	      pos = next_start;
	      CurrDoc = next_doc_num;
	      qd->hops_taken++;
	      kd = k-1;
	    }
	  if (kd == k-1)
	    if (i+k<p)
	      {
		int doc_diff, pos_diff;
		BBLOCK_DECODE_L(doc_diff, dn_blk, pos);
		next_doc_num += doc_diff;
		next_i += k;
		BBLOCK_DECODE_L(pos_diff, len_blk, pos);
		next_start = pos + pos_diff;
	      }
	    else
	      next_doc_num = -1;
#ifdef SHORT_CUT
	  if (op && i+k < p && src->DocNum > next_doc_num && next_doc_num >= 0)
	    goto ShortCut;
#else
	  if (op && i+k < p && src->DocNum > next_doc_num && next_doc_num >= 0)
	    continue;
#endif
	}
      if (k && kd == 0 && i != p-1)
	CurrDoc = next_doc_num;
      else
	{
	  BBLOCK_DECODE_L(diff, blk, pos);
	  CurrDoc += diff;
	}
      qd->num_of_ptrs++;
      if (qd->id->ifh.InvfLevel >= 2)
	GAMMA_DECODE_L(Temp, pos);  /* discard word count */
      i++;
      kd--;
      if (kd < 0)
	kd = k-1;

      switch (op)
	{
	case 0:
	  L->DE[L->num].DocNum = CurrDoc;
	  L->DE[L->num].Weight = Weight;
	  L->DE[L->num].SeekPos = 0;
	  L->DE[L->num].Len = 0;
	  L->DE[L->num].CompTextBuffer = NULL;
	  L->DE[L->num].Next = NULL;
	  L->num++;
	  break;
	case 1:
	  while (src-L->DE < L->num && src->DocNum < CurrDoc) 
	    src++;
	  
	  if (src-L->DE < L->num && CurrDoc == src->DocNum)
	    *dest++ = *src;
	  if (src-L->DE < L->num && CurrDoc >= src->DocNum)
	    src++;
	  break;
	case 2:
	  while (src-L->DE < L->num && src->DocNum < CurrDoc) 
	    *dest++ = *src++;
	  if (src-L->DE < L->num && src->DocNum == CurrDoc)
	    src++;
	  break;
	}
    }

  if (op)
    {
      if (op == 2)
	while (src-L->DE < L->num) 
	  *dest++ = *src++;
      L->num = dest - L->DE;
    }
  DECODE_DONE

  Xfree(buffer); 
  ChangeMemInUse(qd, -we->invf_len); 

  return (L);
}







float *CosineDecode(query_data *qd, TermList *Terms, RankedQueryInfo *rqi)
{
  float *AccumulatedWeights;
  int j, num_docs, num_terms;

  num_docs = qd->sd->sdh.num_of_docs;
  /* Create the array for the documents */
  if (!(AccumulatedWeights = Xmalloc(num_docs * sizeof(float))))
    return(NULL);

#ifdef SUNOS5
  memset(AccumulatedWeights, 0, sizeof(float) * num_docs);
#else
  bzero((char*)AccumulatedWeights, sizeof(float) * num_docs);
#endif
  ChangeMemInUse(qd, sizeof(float) * num_docs);

  if (rqi->MaxTerms == -1)
    num_terms = Terms->num;
  else
    num_terms = rqi->MaxTerms < Terms->num ? rqi->MaxTerms : Terms->num;
  

  /* For each word in the list . . . */
  for (j = 0; j < num_terms; j++)
    {
      uchar *buffer;
      int i, kd;
      unsigned long next_mjr_dn = 0;
      unsigned long CurrDocNum = 0; /* NOTE: Document numbers start at 1 */
      double Wqt, WordLog;
      int dn_blk=0, len_blk=0, k;
      int blk; /* bblock control parameter */
      int p; /* The number of documents the word occurs in */
      WordEntry *we = &Terms->TE[j].WE;
      CalcBlks(qd, we, &blk, &dn_blk, &len_blk, &k, &p);
      if (!(buffer = Xmalloc(we->invf_len)))
	break;

      ChangeMemInUse(qd, we->invf_len); 
      
      Fseek(qd->File_invf, we->invf_ptr, 0);
      Fread(buffer, sizeof(char), we->invf_len, qd->File_invf);

      /* Read from the buffer */
      DECODE_START(buffer, we->invf_len);

      WordLog = log((double)num_docs/(double)we->doc_count);

      Wqt = (rqi->QueryFreqs ? Terms->TE[j].Count : 1) * WordLog;

      qd->num_of_terms++;

      kd = 0;
      for (i = 0; i < p; i++, kd++)
	{
	  int Count; /* The number of times the occurs in a document */
	  double Wdt;
	  unsigned long diff;
	  if (kd == k)
	    {
	      kd = 0;
	    }
	  if (k && kd == 0)
	    if (i+k<p)
	      {
		int temp;
		BBLOCK_DECODE(next_mjr_dn, dn_blk);
		next_mjr_dn += CurrDocNum;
		BBLOCK_DECODE(temp, len_blk);
	      }
	  if (k && kd == k-1 && i != p-1)
	    CurrDocNum = next_mjr_dn;
	  else
	    {
	      BBLOCK_DECODE(diff, blk);
	      CurrDocNum += diff;
	    }
	  if (qd->id->ifh.InvfLevel >= 2)
	    GAMMA_DECODE(Count); 
	  else
	    Count = (double)we->count/p;
	  Wdt = Count * WordLog;
	  AccumulatedWeights[CurrDocNum-1] += Wqt * Wdt;
	  qd->num_of_ptrs++;
	}

      DECODE_DONE
 
      Xfree(buffer);
      ChangeMemInUse(qd, -we->invf_len); 
    }
  return AccumulatedWeights;
}



static int doc_comp(void *a, void *b)
{
  return ((Invf_Doc_Entry*)a)->DocNum - ((Invf_Doc_Entry*)b)->DocNum;
}


static Invf_Doc_Entry *get_ide(query_data *qd, Invf_Doc_Entry_Pool *pool)
{
  struct pool *p = pool->pool;
  if (!p || p->pos == p->length)
    {
      register unsigned size = sizeof(struct pool) + 
	(pool->pool_chunks-1) * sizeof(Invf_Doc_Entry);
      p = malloc(size);
      if (!p) return NULL;
      ChangeMemInUse(qd, size); 
      p->length = pool->pool_chunks;
      p->pos = 0;
      p->next = pool->pool;
      pool->pool = p;
    }
  return &(p->ide[p->pos++]);
}


void free_ide_pool(query_data *qd, Invf_Doc_Entry_Pool *pool)
{
  while (pool->pool)
    {
      struct pool *p = pool->pool;
      pool->pool = p->next;
      ChangeMemInUse(qd, -(sizeof(struct pool) + (p->length-1) * 
		     sizeof(Invf_Doc_Entry)));
      free(p);
    }
  pool->pool = NULL;
}


FILE *open_skip(char *sd)
{
  char buf[256];
  if (sd == NULL || *sd == '\0')
    return NULL;
  sprintf(buf, sd, getpid());
  return fopen(buf, "a");
}



Splay_Tree *CosineDecodeSplay(query_data *qd, TermList *Terms, 
			      RankedQueryInfo *rqi, Invf_Doc_Entry_Pool *pool)
{
  Splay_Tree *Doc_Set;
  int j, num_docs, num_terms, MoreAccum;
  Bool Anding = FALSE;
  FILE *sk = open_skip(rqi->skip_dump);
  uchar indent = 0;
  if (sk)
    {
      if (!skip_header)
	{
	  skip_header = TRUE;
	  fprintf(sk, "%s", skip_header_text[qd->id->ifh.skip_mode]);
	  fprintf(sk, "\nSkipping method %ld\n", qd->id->ifh.skip_mode);
	  switch (qd->id->ifh.skip_mode)
	    {
	    case 1 :
	      fprintf(sk, "Skipping is every %ld docnums\n", 
		      qd->id->ifh.params[0]);
	      break;
	    case 2 :
	      fprintf(sk, "Max nodes = %ld\nNo skips smaller or equal to %ld\n",
		      qd->id->ifh.params[0], qd->id->ifh.params[1]);
	      break;
	    }
	}
      fprintf(sk, "\n\t\t\t-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n");
      for (j=0; j < Terms->num; j++)
	if (Terms->TE[j].Word[0] > indent) 
	  indent = Terms->TE[j].Word[0];
    }

  num_docs = qd->sd->sdh.num_of_docs;
  /* Create the array for the documents */
  Doc_Set = SP_createset(doc_comp);

  pool->pool = NULL;
  pool->pool_chunks = 1024;

  if (rqi->MaxTerms == -1)
    num_terms = Terms->num;
  else
    num_terms = rqi->MaxTerms < Terms->num ? rqi->MaxTerms : Terms->num;

  MoreAccum = 1;

  /* For each word in the list . . . */
  for (j = 0; j < num_terms; j++)
    {
      Bool Abort = FALSE;
      uchar *buffer;
      int pos, i;
      unsigned long CurrDocNum = 0; /* NOTE: Document numbers start at 1 */
      double Wqt, WordLog;
      int dn_blk=0, len_blk=0, k, kd;
      int blk; /* bblock control parameter */
      int p; /* The number of documents the word occurs in */
      WordEntry *we = &Terms->TE[j].WE;
      Invf_Doc_Entry *next = NULL;
      int next_i, next_doc_num, next_start;
      int skips_taken = 0, ptrs_dec = 0;
      double total_added = 0.0;
      int hits = 0;
      if (sk)
	{
	  fprintf(sk, "%3d : %8ld %6ld \"%.*s\"%*s", j, we->count,
		  we->doc_count, Terms->TE[j].Word[0], Terms->TE[j].Word+1,
		  indent - Terms->TE[j].Word[0], "");
	}

      CalcBlks(qd, we, &blk, &dn_blk, &len_blk, &k, &p);
      if (!(buffer = Xmalloc(we->invf_len)))
	break;

      ChangeMemInUse(qd, we->invf_len); 
      
      if (sk && qd->id->ifh.skip_mode == 2)
	fprintf(sk, "%4d", k);

      Fseek(qd->File_invf, we->invf_ptr, 0);
      Fread(buffer, sizeof(char), we->invf_len, qd->File_invf);

      /* Read from the buffer */
      DECODE_START(buffer, we->invf_len);

      WordLog = log((double)num_docs/(double)we->doc_count);

      Wqt = (rqi->QueryFreqs ? Terms->TE[j].Count : 1) * WordLog;

      qd->num_of_terms++;

      if (rqi->MaxAccums != -1 && Doc_Set->no_of_items >= rqi->MaxAccums)
	{
	  MoreAccum = 0;
	  Anding = TRUE;
	  next = SP_get_first(Doc_Set);
	}
      i = pos = 0;
      next_i = next_start = next_doc_num = 0;
      kd = k-1;
      while(i<p && (!Anding || next))
	{
	  int Count; /* The number of times the occurs in a document */
	  double Wdt;
	  unsigned long diff;
	  Invf_Doc_Entry ent, *mem;
	  if (k)
	    {
	      if (Anding && i+k < p && next->DocNum > next_doc_num &&
		  next_doc_num >= 0)
		{
		  i = next_i;
		  DECODE_SEEK(next_start);
		  pos = next_start;
		  CurrDocNum = next_doc_num;
		  qd->hops_taken++;
		  skips_taken++;
		  kd = k-1;
		}
	      if (kd == k-1)
		if (i+k<p)
		  {
		    int doc_diff, pos_diff;
		    BBLOCK_DECODE_L(doc_diff, dn_blk, pos);
		    next_doc_num += doc_diff;
		    next_i += k;
		    BBLOCK_DECODE_L(pos_diff, len_blk, pos);
		    next_start = pos + pos_diff;
		  }
		else
		  next_doc_num = -1;
	      if (Anding && i+k < p && next->DocNum > next_doc_num && 
		  next_doc_num >= 0)
		continue;
	    }
	  if (k && kd == 0 && i != p-1)
	    CurrDocNum = next_doc_num;
	  else
	    {
	      BBLOCK_DECODE_L(diff, blk, pos);
	      CurrDocNum += diff;
	    }
	  qd->num_of_ptrs++;
	  ptrs_dec++;
	  if (qd->id->ifh.InvfLevel >= 2)
	    GAMMA_DECODE_L(Count, pos); 
	  else
	    Count = (double)we->count/p;
	  i++;
	  kd--;
	  if (kd < 0)
	    kd = k-1;
	  Wdt = Count * WordLog;

	  
	  if (Anding)
	    {
	      while (next && next->DocNum < CurrDocNum-1)
		next = SP_get_next(Doc_Set);
	      if (next && next->DocNum == CurrDocNum-1)
		{
		  next->Sum += Wqt * Wdt;
		  total_added += Wqt * Wdt;
		  hits++;
		}
	      if (next && next->DocNum <= CurrDocNum-1)
		next = SP_get_next(Doc_Set);
	      goto NextDocNum;
	    }
	  ent.DocNum = CurrDocNum-1;
	  if ((mem = SP_member(&ent, Doc_Set)) == NULL)
	    {
	      hits++;
	      if (rqi->MaxAccums != -1 && 
		  Doc_Set->no_of_items >= rqi->MaxAccums &&
		  !MoreAccum)
		goto NextDocNum;
	      if ((mem = get_ide(qd, pool)) == NULL)
		return NULL;
	      ChangeMemInUse(qd, sizeof(*mem)); 
	      mem->DocNum = CurrDocNum-1;
	      mem->Sum = 0;
	      ChangeMemInUse(qd, -Doc_Set->mem_in_use); 
	      if (SP_insert(mem, Doc_Set)  == NULL)
		{
		  Message("Unable to allocate memory for a splay node");
		  Abort = TRUE;
		  goto FastAbort;
		}
	      ChangeMemInUse(qd, Doc_Set->mem_in_use); 
	      if (rqi->MaxAccums != -1 && 
		  Doc_Set->no_of_items >= rqi->MaxAccums &&
		  rqi->StopAtMaxAccum)
		Abort = TRUE;
	    }
	  
	  mem->Sum += Wqt * Wdt;
	  total_added += Wqt * Wdt;

	NextDocNum:
	  ;
	}

    FastAbort:

      DECODE_DONE
 
      Xfree(buffer);
      ChangeMemInUse(qd, -we->invf_len); 
      if (sk)
	{
	  if (qd->id->ifh.skip_mode != 0)
	    fprintf(sk, " %6d", skips_taken);
	  fprintf(sk, " %6d %6d %6d%12.2f\n", 
		  ptrs_dec, hits, Doc_Set->no_of_items, total_added);
	}
      if (Abort) break;
    }
  if (sk)
    fclose(sk);
  return Doc_Set;
}




Hash_Table *CosineDecodeHash(query_data *qd, TermList *Terms, 
			     RankedQueryInfo *rqi)
{
  int j, num_docs, num_terms, MoreAccum;
  Hash_Table *HT;
  Bool Anding = FALSE;
  Bool Sorted = FALSE;
  FILE *sk = open_skip(rqi->skip_dump);
  uchar indent = 0;
  if (sk)
    {
      if (!skip_header)
	{
	  skip_header = TRUE;
	  fprintf(sk, "%s", skip_header_text[qd->id->ifh.skip_mode]);
	  fprintf(sk, "\nSkipping method %ld\n", qd->id->ifh.skip_mode);
	  switch (qd->id->ifh.skip_mode)
	    {
	    case 1 :
	      fprintf(sk, "Skipping is every %ld docnums\n", 
		      qd->id->ifh.params[0]);
	      break;
	    case 2 :
	      fprintf(sk, "Max nodes = %ld\nNo skips smaller or equal to %ld\n",
		      qd->id->ifh.params[0], qd->id->ifh.params[1]);
	      break;
	    }
	}
      fprintf(sk, "\n\t\t\t-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n");
      for (j=0; j < Terms->num; j++)
	if (Terms->TE[j].Word[0] > indent) 
	  indent = Terms->TE[j].Word[0];
    }

  num_docs = qd->sd->sdh.num_of_docs;
  /* Create the array for the documents */
  HT = HT_create(qd, rqi->MaxAccums, rqi->HashTblSize);
  if (!HT)
    return NULL;

  if (rqi->MaxTerms == -1)
    num_terms = Terms->num;
  else
    num_terms = rqi->MaxTerms < Terms->num ? rqi->MaxTerms : Terms->num;

  MoreAccum = 1;

  /* For each word in the list . . . */
  for (j = 0; j < num_terms; j++)
    {
      Bool Abort = FALSE;
      uchar *buffer;
      int pos;
      int i;
      unsigned long CurrDocNum = 0; /* NOTE: Document numbers start at 1 */
      double Wqt, WordLog;
      int dn_blk=0, len_blk=0, k, kd;
      int blk; /* bblock control parameter */
      int p; /* The number of documents the word occurs in */
      WordEntry *we = &Terms->TE[j].WE;
      Invf_Doc_EntryH *next = NULL;
      int next_i, next_doc_num, next_start;
      int skips_taken = 0, ptrs_dec = 0;
      double total_added = 0.0;
      int hits = 0;
      if (sk)
	{
	  fprintf(sk, "%3d : %8ld %6ld \"%.*s\"%*s", j, we->count,
		  we->doc_count, Terms->TE[j].Word[0], Terms->TE[j].Word+1,
		  indent - Terms->TE[j].Word[0], "");
	}

      CalcBlks(qd, we, &blk, &dn_blk, &len_blk, &k, &p);
      if (!(buffer = Xmalloc(we->invf_len)))
	break;

      ChangeMemInUse(qd, we->invf_len); 
      
      if (sk && qd->id->ifh.skip_mode == 2)
	fprintf(sk, "%4d", k);

      Fseek(qd->File_invf, we->invf_ptr, 0);
      Fread(buffer, sizeof(char), we->invf_len, qd->File_invf);

      /* Read from the buffer */
      DECODE_START(buffer, we->invf_len);

      WordLog = log((double)num_docs/(double)we->doc_count);

      Wqt = (rqi->QueryFreqs ? Terms->TE[j].Count : 1) * WordLog;

      qd->num_of_terms++;

      if (rqi->MaxAccums != -1 && HT->num >= rqi->MaxAccums)
	{
	  MoreAccum = 0;
	  Anding = TRUE;
	  if (!Sorted)
	    {
	      HT_sort(qd, HT);
	      Sorted = TRUE;
	    }
	  next = HT->Head;
	}
      i = pos = 0;
      next_i = next_start = next_doc_num = 0;
      kd = k-1;
      while(i<p && (!Anding || next))
	{
	  int Count; /* The number of times the occurs in a document */
	  double Wdt;
	  unsigned long diff;
	  Invf_Doc_Entry *mem;
	  if (k)
	    {
	      if (Anding && i+k < p && next->IDE.DocNum > next_doc_num &&
		  next_doc_num >= 0)
		{
#ifdef SHORT_CUT
		ShortCut:
#endif
		  i = next_i;
		  DECODE_SEEK(next_start);
		  pos = next_start;
		  CurrDocNum = next_doc_num;
		  qd->hops_taken++;
		  skips_taken++;
		  kd = k-1;
		}
	      if (kd == k-1)
		{
		  if (i+k<p)
		    {
		      int doc_diff, pos_diff;
		      BBLOCK_DECODE_L(doc_diff, dn_blk, pos);
		      next_doc_num += doc_diff;
		      next_i += k;
		      BBLOCK_DECODE_L(pos_diff, len_blk, pos);
		      next_start = pos + pos_diff;
		    }
		  else
		    next_doc_num = -1;
		}
#ifdef SHORT_CUT
	      if (Anding && i+k < p && next->IDE.DocNum > next_doc_num && 
		  next_doc_num >= 0)
		goto ShortCut;
#else
	      if (Anding && i+k < p && next->IDE.DocNum > next_doc_num && 
		  next_doc_num >= 0)
		continue;
#endif
	    }
	  if (k && kd == 0  && i != p-1)
	    {
	      CurrDocNum = next_doc_num;
	    }
	  else
	    {
	      BBLOCK_DECODE_L(diff, blk, pos);
	      CurrDocNum += diff;
	    }
	  qd->num_of_ptrs++;
	  ptrs_dec++;
	  if (qd->id->ifh.InvfLevel >= 2)
	    GAMMA_DECODE_L(Count, pos); 
	  else
	    Count = (double)we->count/p;
	  i++;
	  kd--;
	  if (kd < 0)
	    kd = k-1;

	  
	  if (Anding)
	    {
	      while (next && next->IDE.DocNum<CurrDocNum-1)
		next = next->next;
	      if (next && next->IDE.DocNum==CurrDocNum-1)
		{
		  Wdt = Count * WordLog;
		  next->IDE.Sum += Wqt * Wdt;
		  total_added += Wqt * Wdt;
		  hits++;
		}
	      if (next && next->IDE.DocNum<=CurrDocNum-1)
		next = next->next;
	      goto NextDocNum;
	    }
	  if ((mem = HT_find(HT, CurrDocNum-1)) == NULL)
	    {
	      if (rqi->MaxAccums != -1 && HT->num >= rqi->MaxAccums &&
		  !MoreAccum)
		goto NextDocNum;
	      Wdt = Count * WordLog;
	      if (!HT_insert(HT, CurrDocNum-1, Wqt * Wdt))
		{
		  if (!HT->Suplimentary_Entries)
		    HT_Sup_create(qd, HT, we->doc_count - i + 1);
		  HT_Sup_Add(HT, CurrDocNum-1, Wqt * Wdt);
		}
	      if (rqi->MaxAccums != -1 && HT->num >= rqi->MaxAccums &&
		  rqi->StopAtMaxAccum)
		Abort = TRUE;
	    }
	  else
	    {
	      Wdt = Count * WordLog;
	      mem->Sum += Wqt * Wdt;
	      total_added += Wqt * Wdt;
	      hits++;
	    }

	NextDocNum:
	  ;
	}

      DECODE_DONE
 
      Xfree(buffer);
      ChangeMemInUse(qd, -we->invf_len); 
      if (sk)
	{
	  if (qd->id->ifh.skip_mode != 0)
	    fprintf(sk, " %6d", skips_taken);
	  fprintf(sk, " %6d %6d %6d%12.2f\n", 
		  ptrs_dec, hits, HT->num+HT->Suplimentary_Num, total_added);
	}
      if (Abort) break;
    }
  if (sk)
    fclose(sk);
  if (!HT->Head)
    HT_sort(qd, HT);
  return HT;
}



List_Table *CosineDecodeList(query_data *qd, TermList *Terms, 
			     RankedQueryInfo *rqi)
{
  int j, num_docs, num_terms, MoreAccum;
  List_Table *LT;
  Bool Anding = FALSE;
  Bool Sorted = FALSE;
  FILE *sk = open_skip(rqi->skip_dump);
  uchar indent = 0;
  if (sk)
    {
      if (!skip_header)
	{
	  skip_header = TRUE;
	  fprintf(sk, "%s", skip_header_text[qd->id->ifh.skip_mode]);
	  fprintf(sk, "\nSkipping method %ld\n", qd->id->ifh.skip_mode);
	  switch (qd->id->ifh.skip_mode)
	    {
	    case 1 :
	      fprintf(sk, "Skipping is every %ld docnums\n", 
		      qd->id->ifh.params[0]);
	      break;
	    case 2 :
	      fprintf(sk, "Max nodes = %ld\nNo skips smaller or equal to %ld\n",
		      qd->id->ifh.params[0], qd->id->ifh.params[1]);
	      break;
	    }
	}
      fprintf(sk, "\n\t\t\t-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n");
      for (j=0; j < Terms->num; j++)
	if (Terms->TE[j].Word[0] > indent) 
	  indent = Terms->TE[j].Word[0];
    }

  num_docs = qd->sd->sdh.num_of_docs;
  /* Create the array for the documents */
  LT = LT_create(qd, rqi->MaxAccums);
  if (!LT)
    return NULL;

  if (rqi->MaxTerms == -1)
    num_terms = Terms->num;
  else
    num_terms = rqi->MaxTerms < Terms->num ? rqi->MaxTerms : Terms->num;

  MoreAccum = 1;

  /* For each word in the list . . . */
  for (j = 0; j < num_terms; j++)
    {
      Bool Abort = FALSE;
      uchar *buffer;
      int pos;
      int i, n=0;
      unsigned long CurrDocNum = 0; /* NOTE: Document numbers start at 1 */
      double Wqt, WordLog;
      int dn_blk=0, len_blk=0, k, kd;
      int blk; /* bblock control parameter */
      int p; /* The number of documents the word occurs in */
      WordEntry *we = &Terms->TE[j].WE;
      Invf_Doc_Entry *next = NULL;
      int next_i, next_doc_num, next_start;
      int skips_taken = 0, ptrs_dec = 0;
      double total_added = 0.0;
      int hits = 0;
      if (sk)
	{
	  fprintf(sk, "%3d : %8ld %6ld \"%.*s\"%*s", j, we->count,
		  we->doc_count, Terms->TE[j].Word[0], Terms->TE[j].Word+1,
		  indent - Terms->TE[j].Word[0], "");
	}

      CalcBlks(qd, we, &blk, &dn_blk, &len_blk, &k, &p);
      if (!(buffer = Xmalloc(we->invf_len)))
	break;

      ChangeMemInUse(qd, we->invf_len); 
      
      if (sk && qd->id->ifh.skip_mode == 2)
	fprintf(sk, "%4d", k);

      Fseek(qd->File_invf, we->invf_ptr, 0);
      Fread(buffer, sizeof(char), we->invf_len, qd->File_invf);

      /* Read from the buffer */
      DECODE_START(buffer, we->invf_len);

      WordLog = log((double)num_docs/(double)we->doc_count);

      Wqt = (rqi->QueryFreqs ? Terms->TE[j].Count : 1) * WordLog;

      qd->num_of_terms++;

      if (rqi->MaxAccums != -1 && LT->num >= rqi->MaxAccums)
	MoreAccum = 0;

      if ((rqi->MaxAccums != -1 && LT->num >= rqi->MaxAccums) || Anding)
	{
	  Anding = TRUE;
	  if (!Sorted)
	    {
	      LT_sort(LT);
	      LT_pack(LT);
	      Sorted = TRUE;
	    }
	  next = LT->IDE;
	  n = 0;
	}
      i = pos = 0;
      next_i = next_start = next_doc_num = 0;
      kd = k-1;
      while(i<p && (!Anding || n < LT->num))
	{
	  int Count; /* The number of times the occurs in a document */
	  double Wdt;
	  unsigned long diff;
	  Invf_Doc_Entry *mem;
	  if (k)
	    {
	      if (Anding && i+k < p && next->DocNum > next_doc_num &&
		  next_doc_num >= 0)
		{
		  i = next_i;
		  DECODE_SEEK(next_start);
		  pos = next_start;
		  CurrDocNum = next_doc_num;
		  qd->hops_taken++;
		  skips_taken++;
		  kd = k-1;
		}
	      if (kd == k-1)
		if (i+k<p)
		  {
		    int doc_diff, pos_diff;
		    BBLOCK_DECODE_L(doc_diff, dn_blk, pos);
		    next_doc_num += doc_diff;
		    next_i += k;
		    BBLOCK_DECODE_L(pos_diff, len_blk, pos);
		    next_start = pos + pos_diff;
		  }
		else
		  next_doc_num = -1;
	      if (Anding && i+k < p && next->DocNum > next_doc_num && 
		  next_doc_num >= 0)
		continue;
	    }
	  if (k && kd == 0 && i != p-1)
	    CurrDocNum = next_doc_num;
	  else
	    {
	      BBLOCK_DECODE_L(diff, blk, pos);
	      CurrDocNum += diff;
	    }
	  qd->num_of_ptrs++;
	  ptrs_dec++;
	  if (qd->id->ifh.InvfLevel >= 2)
	    GAMMA_DECODE_L(Count, pos); 
	  else
	    Count = (double)we->count/p;
	  i++;
	  kd--;
	  if (kd < 0)
	    kd = k-1;
	  Wdt = Count * WordLog;

	  
	  if (Anding)
	    {
	      while (n < LT->num && next->DocNum<CurrDocNum-1)
		{next++;n++;}
	      if (n < LT->num && next->DocNum==CurrDocNum-1)
		{
		  next->Sum += Wqt * Wdt;
		  total_added += Wqt * Wdt;
		  hits++;
		}
	      if (n < LT->num && next->DocNum<=CurrDocNum-1)
		{next++;n++;}
	      goto NextDocNum;
	    }
	  if (!Anding)
	    {
	      if (rqi->MaxAccums != -1 && LT->num >= rqi->MaxAccums &&
		  !MoreAccum)
		goto NextDocNum;
	      LT = LT_add(qd, LT, CurrDocNum-1, Wqt * Wdt);
	      if (!LT)
		{
		  Abort = TRUE;
		  goto FastAbort;
		}
	      if (rqi->MaxAccums != -1 && LT->num >= rqi->MaxAccums &&
		  rqi->StopAtMaxAccum)
		Abort = TRUE;

		
	    }
	  else
	    if ((mem = LT_find(LT, CurrDocNum-1)))
	      {
		mem->Sum += Wqt * Wdt;
		total_added += Wqt * Wdt;
		hits++;
	      }

	NextDocNum:
	  ;
	}

    FastAbort:

      DECODE_DONE
 
      Xfree(buffer);
      ChangeMemInUse(qd, -we->invf_len); 
      if (sk)
	{
	  if (qd->id->ifh.skip_mode != 0)
	    fprintf(sk, " %6d", skips_taken);
	  fprintf(sk, " %6d %6d %6d%12.2f\n", 
		  ptrs_dec, hits, LT->num, total_added);
	}
      if (Abort) break;
    }
  if (sk)
    fclose(sk);
  return LT;
}


