/**************************************************************************
 *
 * PS_Tree_easy.c -- Generates a PostScript drawing of a n-way tree
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
 *       @(#)PS_Tree_easy.c	1.3 16 Mar 1994
 *
 **************************************************************************/

char *SCCS_Id_PSTree_easy = "@(#)PS_Tree_easy.c	1.3 16 Mar 1994";

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "sysfuncs.h"
#include "messages.h"
#include "memlib.h"
#include "PS_Tree.h"


typedef struct ps_node
{
    PSTree_node PSnode;
    void *data;
} ps_node;



static EasyNodeFunc easy_func;

static void easy_node_func(FILE *f, void *node)
{
  easy_func(f, ((ps_node *)node)->PSnode.shape, ((ps_node *)node)->data);
}



static PSTree_node *MakeBinaryNode(void *q, int left_off, int right_off, 
				   ps_node *parent)
{
  ps_node *pn = NULL;
  void *left, *right;
  if (q)
    {
      PSTree_node *sib;
      pn = malloc(sizeof(ps_node));
      pn->PSnode.Parent = (PSTree_node*)parent;
      pn->PSnode.shade = 1.0;
      left = *((void **)(((char*)q)+left_off));
      right = *((void **)(((char*)q)+right_off));

      pn->PSnode.shape = (left || right) ? PSTree_Circle : PSTree_Box;
      pn->data = q;
      pn->PSnode.Child = MakeBinaryNode(left, left_off, right_off, pn);
      sib = MakeBinaryNode(right, left_off, right_off, pn);
      if (pn->PSnode.Child)
	pn->PSnode.Child->Sibling = sib;
      else
	pn->PSnode.Child = sib;
    }
  return((PSTree_node *)pn);
}




static PSTree_node *MakeNWayNode(void *q, int child_off, int sib_off, 
				 ps_node *parent)
{
  ps_node *pn = NULL;
  if (q)
    {
      PSTree_node **p;
      pn = malloc(sizeof(ps_node));
      pn->PSnode.Parent = (PSTree_node*)parent;
      pn->PSnode.Child = pn->PSnode.Sibling = NULL;
      pn->PSnode.shade = 1.0;
      pn->PSnode.shape = *((void **)(((char*)q)+child_off)) ? PSTree_Circle :
	PSTree_Box;
      pn->data = q;

      p = &pn->PSnode.Child;
      for (q = *((void **)(((char*)q)+child_off)); q ; q = *((void **)(((char*)q)+sib_off)))
	{
	  *p = MakeNWayNode(q, child_off, sib_off, pn);
	  p = &((*p)->Sibling);
	}
    }
  return((PSTree_node *)pn);
}




void BinaryPSTree(void *root, int left, int right, 
		EasyNodeFunc func, char *filename, char *title)
{
  PSTree_data data;

  easy_func = func;

  data.filename = filename;
  data.title = title;
  data.root = (PSTree_node*)MakeBinaryNode(root, left, right, NULL);
  data.func = easy_node_func;
  data.ParentLinks = 0;
  
  GeneratePSTree(&data);

  FreePSTree(data.root);
}


void NWayPSTree(void *root, int child, int sib, 
		EasyNodeFunc func, char *filename, char *title)
{
  PSTree_data data;

  easy_func = func;

  data.filename = filename;
  data.title = title;
  data.root = (PSTree_node*)MakeNWayNode(root, child, sib, NULL);
  data.func = easy_node_func;
  data.ParentLinks = 0;
  
  GeneratePSTree(&data);

  FreePSTree(data.root);
}

