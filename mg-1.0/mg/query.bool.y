/**************************************************************************
 *
 * query.bool.y -- Boolean query evaluation
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
 *       @(#)query.bool.y	1.9 16 Mar 1994
 *
 **************************************************************************/

%{

#define PS_TREE 1

#ifndef STANDALONE
#  define STANDALONE 0
#endif

#include "config.h"

#include <ctype.h>
#ifdef SUNOS5
#include <libgen.h>
#endif
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <dirent.h>
#include <alloca.h>

#include "memlib.h"
#include "sysfuncs.h"
#include "messages.h"
#include "filestats.h"
#include "timing.h"
#include "sptree.h"

#if PS_TREE
#include "PS_Tree.h"
#endif

#include "mg.h"
#include "text.h"
#include "invf.h"
#include "lists.h"
#include "backend.h"
#include "words.h"
#include "stemmer.h"
#include "stem_search.h"
#include "invf_get.h"
#include "text_get.h"

typedef enum {N_term, N_and, N_or, N_diff, N_none, N_all, N_not} N_Tag;

typedef struct term_entry
{
  int count;
  uchar *text;
  WordEntry WE;
  long word_num;
  int type;
  struct term_entry *next;
} term_entry;

typedef struct terms_data
{
  int stem_method;
  term_entry *te;
} terms_data;

typedef struct node
{
  N_Tag tag;
  union 
    {
      struct ChildSibling 
	{
	  struct node *Child;
	  struct node *Sibling;
	} CS;
      term_entry *te;
    } fields;
} node;


#define TE(n) ((n)->fields.te)
#define TAG(n) ((n)->tag)
#define CHILD(n) ((n)->fields.CS.Child)
#define SIBLING(n) ((n)->fields.CS.Sibling)



static node *TermNode(terms_data *td, int type, char *text);
static node *TreeNode(N_Tag tag, node *left, node *right);

char *SCCS_Id_query_y = "@(#)query.bool.y	1.9 16 Mar 1994";

static int query_lex();

static int yyerror(char *);

static terms_data td;
static char *ch_buf;
static node *base;

/*#define yyparse() query_parse(terms_data *td, char *ch_buf, node **base)*/

#define yylex(a) query_lex(&ch_buf, a)

%}

%union {
  char *text;
  node *node;
}


%token <text> TERM WILD_TERM 
%type <node> query term not and or


%pure_parser

%%

query: or  { base = $1; }
;


term:	  TERM  { $$ = TermNode(&td, TERM, $1); }
	| WILD_TERM { $$ = TermNode(&td, WILD_TERM, $1); }
  	| '(' or ')' { $$ = $2; }
;

not:	  term
	| '!' not { $$ = TreeNode(N_not, $2, NULL); }
;

and:	  and '&' not { $$ = TreeNode(N_and, $1, $3); }
	| and not { $$ = TreeNode(N_and, $1, $2); }
	| not
;

or:	  or '|' and { $$ = TreeNode(N_or, $1, $3); }
	| and
;

%%

#undef yylex

/* Bison on one mips machine defined "const" to be nothing but
   then did not undef it */
#ifdef const
#undef const
#endif


#define WORDCHAR(p) (isalpha(p) || isdigit(p) || (p) == '?' || (p) == '*')


static int query_lex(char **ptr, YYSTYPE *lvalp)
{
  while (**ptr == ' ' || **ptr == '\t' || **ptr == '\n' || **ptr == '\"')
    (*ptr)++;
  if (WORDCHAR(**ptr))
    {
      int wild = 0;
      char *s = *ptr;
      while (WORDCHAR(**ptr))
	{
	  if (**ptr == '?' || **ptr == '*')
	    wild = 1;
	(*ptr)++;
	}
      lvalp->text = Xmalloc(*ptr - s + 10);
      strncpy(lvalp->text+1, s, *ptr - s);
      lvalp->text[0] = *ptr - s;
      if (!wild)
	stemmer(td.stem_method, (uchar*)lvalp->text);
      return wild ? WILD_TERM : TERM;
    }
  else
    if (**ptr == '\0')
      return 0;
    else
      return *(*ptr)++;
}



static node *NewNode(N_Tag tag)
{
  node *n;
  if (!(n = Xmalloc(sizeof(node))))
    return(NULL);
  n->tag = tag;
  CHILD(n) = NULL;
  SIBLING(n) = NULL;
  return n;
}

static node *TermNode(terms_data *td, int type, char *text)
{
  node *n = NewNode(N_term);
  term_entry *te;
  for (te = td->te; te; te = te->next)
    if (te->type == type && compare(te->text, (uchar*)text) == 0)
      {
	te->count++;
	Xfree(text);
	TE(n) = te;
	return(n);
      }
  te = Xmalloc(sizeof(term_entry));
#ifdef SUNOS5
  memset(te, 0, sizeof(term_entry));
#else
  bzero((char*)te, sizeof(term_entry));
#endif
  te->next = td->te;
  td->te = te;
  te->text = (uchar*)text;
  te->type = type;
  te->count = 1;
  TE(n) = te;
  return n;
  
}




static node *TreeNode(N_Tag tag, node *left, node *right)
{
  if (TAG(left) == tag && (tag == N_and || tag == N_or))
    {
      node *n = CHILD(left);
      while (SIBLING(n))
	n = SIBLING(n);
      SIBLING(n) = right;
      return(left);
    }
  else
    {
      node *n = NewNode(tag);
      CHILD(n) = left;
      SIBLING(CHILD(n)) = right;
      return(n);
    }
}


static int yyerror(char *s)
{
  Message("%s", s);
  return(1);
}



#if PS_TREE
/*   -=-=-=-   -=-=-=-   -=-=-=-   -=-=-=-   -=-=-=-   -=-=-=-   -=-=-=- */

typedef struct ps_node
{
  PSTree_node PSnode;
  node *n;
} ps_node;

static ps_node *MakePSNode(node *q, node *special, ps_node *parent)
{
  ps_node *pn = NULL;
  if (q)
    {
      pn = Xmalloc(sizeof(ps_node));
      pn->PSnode.Parent = (PSTree_node*)parent;
      pn->PSnode.Child = pn->PSnode.Sibling = NULL;
      pn->PSnode.shade = q == special ? 0.75 : 1.0;
      pn->n = q;
      if (TAG(q) == N_and || TAG(q) == N_or || TAG(q) == N_diff)
	{
	  PSTree_node **p;
	  pn->PSnode.shape = PSTree_Circle;
	  pn->PSnode.width = 2.0;
	  pn->n = q;
	  
	  p = &pn->PSnode.Child;
	  for (q = CHILD(q); q ; q = SIBLING(q))
	    {
	      *p = (PSTree_node *)MakePSNode(q, special, pn);
	      p = &((*p)->Sibling);
	    }
	}
      else
	if (TAG(q) == N_not)
	  {
	    pn->PSnode.shape = PSTree_Circle;
	    pn->PSnode.width = 1.5;
	    pn->n = q;
	    pn->PSnode.Child = (PSTree_node *)MakePSNode(CHILD(q), 
							 special, pn);
	  }
	else
	  {
	    pn->PSnode.shape = PSTree_Box;
	    pn->PSnode.width = 1.5;
	    pn->n = q;
	  }
    }
  return(pn);
}

static void node_func(FILE *f, void *pn)
{
  node *n = ((ps_node*)pn)->n;
  char *s = "????";
  switch (TAG(n))
    {
    case N_term : s = "Term"; break;
    case N_and  : s = "And"; break;
    case N_or   : s = "Or"; break;
    case N_diff : s = "Diff"; break;
    case N_none : s = "None"; break;
    case N_all  : s = "All"; break;
    case N_not  : s = "Not"; break;
    }
  if (TAG(n) == N_term)
    {
      fprintf(f, "h8 setfont 0  5 moveto (%s) cshow\n", s);
      fprintf(f, "tb10 setfont 0 -4.5 moveto (%.*s) cshow\n",
	      TE(n)->text[0], TE(n)->text+1);
    }
  else
    fprintf(f, "h8 setfont 0 0 moveto (%s) cshow\n", s);
}



struct matching_files
{
  char *name;
  struct matching_files *next;
};


static struct matching_files *match_files(char *dir, char *pattern)
{
  struct matching_files *base = NULL;
  DIR *dirp = opendir(dir);
  struct dirent *dp;

  if (!dirp) return NULL;
#ifdef SUNOS5
  pattern = regcmp(pattern, NULL);
#else
  re_comp(pattern);
#endif
  for (dp = readdir(dirp); dp; dp = readdir(dirp))
    {
#ifdef SUNOS5
      if (regex(pattern, dp->d_name, NULL))
#else
      if (re_exec(dp->d_name))
#endif
	{
	  struct matching_files *new;
	  new = Xmalloc(sizeof(struct matching_files));
	  if (new)
	    {
	      new->next = base;
	      new->name = Xstrdup(dp->d_name);
	      base = new;
	    }
	}
    }
  closedir(dirp);
  return base;
}


static void rm(char *dir, char *pattern)
{
  struct matching_files *base;
  base = match_files(dir, pattern);
  while (base)
    {
      struct matching_files *next = base->next;
      char *buf = (char*)alloca(strlen(dir) + strlen(base->name) + 2);
      sprintf(buf, "%s/%s", dir, base->name);
      unlink(buf);
      Xfree(base);
      base = next;
    }
}

static void DumpTree(node *root, node *q, char *s)
{
  static int num = 0;
  PSTree_data data;
  char filename[128];

  if (access("epsf", W_OK) != 0) return;

  if (num == 0)
    rm("epsf", "parse-tree\\.[0-9][0-9][0-9][0-9][0-9]\\.epsf");
 
  sprintf(filename, "epsf/parse-tree.%05d.epsf", num++);
 
  data.filename = filename;
  data.title = s;
  data.root = (PSTree_node*)MakePSNode(root, q, NULL);
  data.func = node_func;
  data.ParentLinks = 0;
 
  GeneratePSTree(&data);
  FreePSTree(data.root);
}




/*   -=-=-=-   -=-=-=-   -=-=-=-   -=-=-=-   -=-=-=-   -=-=-=-   -=-=-=- */
#endif

/****************************************************************************/

static void FreeNode(node *n);

static void FreeNodes(node *n)
{
  while (n)
    {
      node *next = SIBLING(n);
      FreeNode(n);
      n = next;
    }
}

static void FreeNode(node *n)
{
  if (n)
    {
      if (TAG(n) == N_or || TAG(n) == N_and || 
	  TAG(n) == N_not || TAG(n) == N_diff)
	FreeNodes(CHILD(n));
      Xfree(n);
    }
}



static Bool equal(node *a, node *b)
{
  if (TAG(a) == N_term && TAG(a) == N_term)
    return TE(a) == TE(b);
  return FALSE;
}



/* This gets passed a pointer to an "and", or a "or" node.
   It removes duplicate words.
   If the number of words drops to one it deleted the original node and
   replaces it with the word. */
static node *RemoveDup(node *n)
{
  node *s;
  for (s = CHILD(n); s; s = SIBLING(s))
    {
      node *c, *o;
      for (o = s, c = SIBLING(s); c; c = SIBLING(c))
	if (equal(s, c))
	  {
	    SIBLING(o) = SIBLING(c);
	    FreeNode(c);
	  }
	else
	  o = c;
    }
  if (CHILD(n) && SIBLING(CHILD(n)) == NULL)
    {
      s = CHILD(n);
      Xfree(n);
      n = s;
    }
  return n;
}





static int AndSort_comp(const void *A, const void *B)
{
  const node * const *a = A;
  const node * const *b = B;
  if (TAG(*a) == N_term && TAG(*b) == N_term)
    return TE(*a)->WE.doc_count - TE(*b)->WE.doc_count;
  else
    if (TAG(*a) == N_term && TAG(*b) != N_term)
      return -1;
    else
      if (TAG(*a) != N_term && TAG(*b) == N_term)
	return 1;
      else
	return 0;
}

/* Sort the list of nodes by increasing doc_count */
static node *AndSort(node *n)
{
  node **list;
  node *base = n;
  int i,count = 0;
  while (n)
    {
      count++;
      n = SIBLING(n);
    }
  
  if (!(list = Xmalloc(sizeof(node*)*count)))
    return base;

  n = base;
  for (i=0; i<count; i++)
    {
      list[i] = n;
      n = SIBLING(n);
    }

  qsort(list, count, sizeof(node*), AndSort_comp);

  for (i=0; i<count-1; i++)
    SIBLING(list[i]) = list[i+1];
  SIBLING(list[count-1]) = NULL;
  base = list[0];

  Xfree(list);

  return base;
}


/* Base points to an "and" node. This routine pulls out all then "not" nodes
   from under the "and" node and puts them on the right of a "diff" node.
   This only happens if there is at least one "not" node and at least one
   non-"not" node.
   */
static node *BuildDiffNode(node *base)
{
  int num_nots = 0;
  int num_non_nots = 0;
  node *diff, *curr, *next, **nots, **ands;
  curr = CHILD(base);
  while (curr)
    {
      if (TAG(curr) == N_not)
	num_nots++;
      else
	num_non_nots++;
      curr = SIBLING(curr);
    }
  if (num_nots == 0 || num_non_nots == 0)
    return base;

  diff = NewNode(N_diff);
  CHILD(diff) = base;
  nots = &SIBLING(base);
  ands = &CHILD(base);
  curr = CHILD(base);
  while (curr)
    {
      next = SIBLING(curr);
      if (TAG(curr) == N_not)
	{
	  *nots = CHILD(curr);
	  nots = &SIBLING(*nots);
	  Xfree(curr);
	}
      else
	{
	  *ands = curr;
	  ands = &SIBLING(*ands);
	}
      curr = next;
    }
  *ands = NULL;
  *nots = NULL;
  return diff;
}


static node *OptimizeBooleanQuery(node *base);




static node *OptimizeNoneNode(node *base)
{
  return base;
}



static node *OptimizeAllNode(node *base)
{
  return base;
}



static node *OptimizeNotNode(node *base)
{
  CHILD(base) = OptimizeBooleanQuery(CHILD(base));
  
  switch (TAG(CHILD(base)))
    {
      /* Delete a "not" "not" node */
    case N_not :
      {
	node *n = base;
	base = CHILD(CHILD(base));
	Xfree(CHILD(n));
	Xfree(n);
	break;
      }
      /* An "all" node in a "not" node gets changed to a "none" node */
    case N_all :
      {
	node *n = base;
	base = CHILD(base);
	Xfree(n);
	TAG(base) = N_none;
	break;
      }
      /* A "none" node in a "not" node gets changed to an "all" node */
    case N_none :
      {
	node *n = base;
	base = CHILD(base);
	Xfree(n);
	TAG(base) = N_all;
	break;
      }
    case N_or :
      {
	node **d;
	node *n = CHILD(CHILD(base));
	Xfree(CHILD(base));
	Xfree(base);
	base = NewNode(N_and);
	d = &CHILD(base);
	*d = NULL;
	while (n)
	  {
	    *d = NewNode(N_not);
	    CHILD(*d) = n;
	    d = &SIBLING(*d);
	    *d = NULL;
	    n = SIBLING(n);
	  }
	break;
      }
    case N_term :
    case N_and :
    case N_diff : break;
    }
  return base;
}




static node *OptimizeDiffNode(node *base)
{
  node *curr = CHILD(base);
  node **ptr = &CHILD(base);
  while (curr)
    {
      node *next = SIBLING(curr);
      curr = OptimizeBooleanQuery(curr);
      if (curr)
	{
	  *ptr = curr;
	  ptr = &SIBLING(curr);
	}
      curr = next;
    }
  *ptr = NULL;
  if (CHILD(base))
    {
      /* If the "diff" node has only one child then make the child of the
	 "diff" node the base node. */ 
      if (!SIBLING(CHILD(base)))
	{
	  node *n = base;
	  base = CHILD(base);
	  Xfree(n);
	}
    }
  else
    {
      TAG(base) = N_none;
      CHILD(base) = NULL;
      SIBLING(base) = NULL;
    }
  return base;
}


static node *OptimizeAndNode(node *base)
{
  node *curr = CHILD(base);
  node **ptr = &CHILD(base);
  while (curr)
    {
      node *next = SIBLING(curr);
      curr = OptimizeBooleanQuery(curr);
      switch (TAG(curr))
	{
	case N_all :
	  /* An "all" node in an "and" node may be deleted */
	  FreeNode(curr);
	  curr = NULL;
	  break;
	  
	case N_none :
	  /* A "none" node in an "and" node turns the entire "and"
	     node into a "none" node */
	  FreeNode(curr);
	  FreeNodes(next);
	  FreeNodes(CHILD(base));
	  next = NULL;
	  curr = NULL;
	  TAG(base) = N_none;
	  CHILD(base) = NULL;
	  SIBLING(base) = NULL;
	  break;
	  
	case N_and :
	  /* An "and" node gets incorporated into the current node */
	  {
	    *ptr = CHILD(curr);
	    Xfree(curr);
	    do
	      {
		ptr = &SIBLING(*ptr);
	      }
	    while (*ptr);
	    curr = NULL;
	  }
	  break;
	case N_not :
	case N_term :
	case N_or :
	case N_diff : break;
	}
      if (curr)
	{
	  *ptr = curr;
	  ptr = &SIBLING(curr);
	}
      curr = next;
    }
  *ptr = NULL;
  if (CHILD(base))
    {
      base = RemoveDup(base);
      if (TAG(base) == N_and)
	CHILD(base) = AndSort(CHILD(base));
      if (TAG(base) == N_and)
	base = BuildDiffNode(base);
    }
  else
    {
      TAG(base) = N_none;
      CHILD(base) = NULL;
      SIBLING(base) = NULL;
    }
  return base;
}


static node *OptimizeOrNode(node *base)
{
  node *curr = CHILD(base);
  node **ptr = &CHILD(base);
  while (curr)
    {
      node *next = SIBLING(curr);
      curr = OptimizeBooleanQuery(curr);
      switch (TAG(curr))
	{
	case N_none :
	  FreeNode(curr);
	  curr = NULL;
	  break;
	case N_all :
	  FreeNode(curr);
	  FreeNodes(next);
	  FreeNodes(CHILD(base));
	  next = NULL;
	  curr = NULL;
	  TAG(base) = N_all;
	  CHILD(base) = NULL;
	  SIBLING(base) = NULL;
	  break;
	case N_or :
	  /* An "or" node gets incorporated into the current node */
	  {
	    *ptr = CHILD(curr);
	    Xfree(curr);
	    do
	      {
		ptr = &SIBLING(*ptr);
	      }
	    while (*ptr);
	    curr = NULL;
	  }
	  break;
	case N_and :
	case N_term :
	case N_not :
	case N_diff : break;
	}
      if (curr)
	{
	  *ptr = curr;
	  ptr = &SIBLING(curr);
	}
      curr = next;
    }
  *ptr = NULL;
  if (CHILD(base))
    base = RemoveDup(base);
  else
    {
      TAG(base) = N_none;
      CHILD(base) = NULL;
      SIBLING(base) = NULL;
    }
  return base;
}


static node *OptimizeTermNode(node *base)
{
  /* If a word doesn't exist then turn it into a none node */
  if (TE(base)->WE.word_num == -1)
    {
      TE(base) = NULL;
      TAG(base) = N_none;
    }
  return base;
}


static node *OptimizeBooleanQuery(node *base)
{
  int last_tag = -1;
  while (base && last_tag != TAG(base))
    {
      last_tag = TAG(base);
      switch (last_tag)
	{
	  
	  /* Handle the "none" node */
	case N_none :
	  base = OptimizeNoneNode(base);
	  break;
	  
	  /* Handle the "all" node */
	case N_all :
	  base = OptimizeAllNode(base);
	  break;
	  
	  /* Handle the "not" node */
	case N_not : 
	  base = OptimizeNotNode(base);
	  break;
	  
	  /* Handle the "diff" node */
	case N_diff :
	  base = OptimizeDiffNode(base);
	  break;
	  
	  /* Handle the "and" node */
	case N_and :
	  base = OptimizeAndNode(base);
	  break;
	  
	  /* Handle the "or" node */
	case N_or :
	  base = OptimizeOrNode(base);
	  break;
	  
	  /* Handle the "term" node */
	case N_term :
	  base = OptimizeTermNode(base);
	  break;
	}
    }
  return base;
}

#if !STANDALONE
/****************************************************************************/

static void ReadInTermInfo(query_data *qd, term_entry *te)
{
  while(te)
    {
      switch (te->type)
	{
	case TERM :
	  {
	    WordEntry *we = &te->WE;
	    we->word_num = FindWord(qd->sd, te->text, &we->count, 
				    &we->doc_count, &we->invf_ptr, 
				    &we->invf_len);
	    if (we->word_num == -1)
	      we->count = we->doc_count = 0;
	    break;
	  }
	case WILD_TERM :
	  {
	    Message("Wildcard terms are not permitted yet.");
	    break;
	  }
	default :
	  FatalError(1, "Unexpected term type");
	}
      te = te->next;
    }
}

/****************************************************************************/

static DocList *BooleanGet(query_data *qd, node *n, BooleanQueryInfo *bqi)
{
  if (n)
    {
      DocList *res = NULL;
      switch (TAG(n))
	{
	case N_all :
	  Message("Unexpected \"all\" node in the parse tree.");
	  res = MakeDocList(0);
	  break;
	case N_not : 
	  Message("Unexpected \"not\" node in the parse tree.");
	  res = MakeDocList(0);
	  break;
	case N_none : 
	  {
	    res = MakeDocList(0);
	    break;
	  }
	case N_diff :
	  {
	    res = BooleanGet(qd, CHILD(n), bqi);
	    n = SIBLING(CHILD(n));
	    while (n)
	      {
		node *next = SIBLING(n);
		if (TAG(n) == N_term)
		  {
		    WordEntry *we = &TE(n)->WE;
		    res = GetDocsOp(qd, we, 2, res);
		  }
		else
		  res = DiffLists(res, BooleanGet(qd, n, bqi));
		n = next;
	      }
	    break;
	  }
	case N_and :
	  {
	    res = BooleanGet(qd, CHILD(n), bqi);
	    n = SIBLING(CHILD(n));
	    while (n)
	      {
		node *next = SIBLING(n);
		if (TAG(n) == N_term)
		  {
		    WordEntry *we = &TE(n)->WE;
		    res = GetDocsOp(qd, we, 1, res);
		  }
		else
		  res = IntersectLists(res, BooleanGet(qd, n, bqi));
		n = next;
	      }
	    break;
	  }
	case N_or :
	  {
	    res = BooleanGet(qd, CHILD(n), bqi);
	    n = SIBLING(CHILD(n));
	    while (n)
	      {
		node *next = SIBLING(n);
		res = MergeLists(res, BooleanGet(qd, n, bqi));
		n = next;
	      }
	    break;
	  }
	case N_term :
	  {
	    res = GetDocsOp(qd, &TE(n)->WE, 0, NULL);
	    break;
	  }
	}
      return(res?res:MakeDocList(0));
    }
  else
    return MakeDocList(0);
}


static void FreeTerms(term_entry *te)
{
  term_entry *t;
  while (te)
    {
      t = te;
      te = te->next;
      Xfree(t);
    }
}


static int DE_comp(void *a, void *b)
{
  return ((DocEntry *)a)->SeekPos - ((DocEntry *)b)->SeekPos;
}

void BooleanQuery(query_data *qd, char *Query, BooleanQueryInfo *bqi)
{
  node *n;
  DocList *Docs;
  int res;
  td.te = NULL;
  ch_buf = Query;
  td.stem_method = qd->sd->sdh.stem_method;

  res = yyparse();
  n = base;

  if (res != 0)
    qd->DL = MakeDocList(0);
  
  ReadInTermInfo(qd, td.te);


#if PS_TREE
  DumpTree(n, NULL, "Before Optimization");
#endif

  n = OptimizeBooleanQuery(n);

#if PS_TREE
  DumpTree(n, NULL, "After Optimization");
#endif

  qd->hops_taken = qd->num_of_ptrs = 0;
  qd->num_of_terms = 0;

  Docs = BooleanGet(qd, n, bqi);

  FreeNode(n);

  FreeTerms(td.te);

  if (qd->id->ifh.InvfLevel == 3)
    {
      DocEntry *DE = Docs->DE;
      int s, d;
      Splay_Tree *SP;

      for (s = 0; s<Docs->num; s++)
	FetchDocStart(qd, DE[s].DocNum, &DE[s].SeekPos, &DE[s].Len);
      Message("Original Number = %d\n", Docs->num);
      SP = SP_createset(DE_comp);
      d = 0;
      for (s = 0; s < Docs->num; s++)
	if (!SP_member(&DE[s], SP))
	  {
	    DE[d] = DE[s];
	    SP_insert(&DE[d], SP);
	    d++;
	  }
      SP_freeset(SP);
      Docs->num = d;
      Message("Final Number = %d\n", Docs->num);
    }


  if (bqi->MaxDocsToRetrieve != -1 && Docs->num > bqi->MaxDocsToRetrieve)
    Docs->num = bqi->MaxDocsToRetrieve;

  FreeQueryDocs(qd);

  qd->DL = Docs;
}
#else



void main(void)
{
  char s[1024];
  terms_data td;
  node *n;
  td.te = NULL;
  while (fprintf(stderr,"> "),gets(s))
    {
      term_entry *te;
      Message("res = %d TERM = %d\n",query_parse(&td, s, &n), TERM);
      te = td.te;
      while (te)
	{
	  Message("Term \"%s\"   %d %d", te->text, te->count, te->type);
	  te = te->next;
	}
#if PS_TREE
      DumpTree(n, NULL, "Before Optimization");
#endif

      n = OptimizeBooleanQuery(n);

#if PS_TREE
      if (n)
	DumpTree(n, NULL, "After Optimization");
#endif

    }
  exit(0);
}
#endif
