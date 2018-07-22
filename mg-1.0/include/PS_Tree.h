/**************************************************************************
 *
 * PS_Tree.h -- Generates a PostScript drawing of a n-way tree
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
 *       @(#)PS_Tree.h	1.4 09 Mar 1994
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * typedef struct ps_node
 * {
 *   PSTree_node PSnode;
 *   long alpha;
 * } ps_node;
 *  
 * static ps_node *MakePSNode(node *q, node *special, ps_node *parent)
 * {
 *   ps_node *pn = NULL;
 *   if (q)
 *     {
 *       pn = malloc(sizeof(ps_node));
 *       pn->PSnode.Parent = (PSTree_node*)parent;
 *       pn->PSnode.shade = q == special ? 0.75 : 1.0;
 *       if (q->left == NULL && q->right == NULL)
 *         {
 *           pn->PSnode.shape = PSTree_Box;
 *           pn->alpha = q->value;
 *           pn->PSnode.Left = pn->PSnode.Right = NULL;
 *         }
 *       else
 *         {
 *           pn->PSnode.shape = PSTree_Circle;
 *           pn->alpha = -1;
 *           pn->PSnode.Left = (PSTree_node *)MakePSNode(q->left, 
 *                                                    special, pn);
 *           pn->PSnode.Right = (PSTree_node *)MakePSNode(q->right, 
 *                                                     special, pn);
 *         }
 *     }
 *   return(pn);
 * }
 *  
 *  
 * static void node_func(FILE *f, void *node)
 * {
 *   ps_node *pn = node;
 *   if (pn->PSnode.shape == PSTree_Box)
 *     fprintf(f, "tr12 setfont 0 0 moveto (%d) cshow\n", pn->alpha);
 * }
 *  
 *  
 * static void DumpTree(node *q, char *s)
 * {
 *   static int num = 0;
 *   PSTree_data data;
 *   char filename[128];
 *   if (num == 0)
 *     {
 *       fprintf(stderr, "Erasing old postscript files\n");
 *       system("/usr/bin/rm epsf/splay-tree.[0-9][0-9][0-9][0-9][0-9].epsf");
 *     }
 *  
 *   fprintf(stderr,"Dumping Postscript \"%s\". . .", s);
 *   sprintf(filename, "epsf/splay-tree.%05d", num++);
 *  
 *   data.filename = filename;
 *   data.title = s;
 *   data.root = (PSTree_node*)MakePSNode(root, q, NULL);
 *   data.func = node_func;
 *   data.ParentLinks = 0;
 *  
 *   GeneratePSTree(&data);
 *   FreePSTree(data.root);
 *  
 *   fprintf(stderr,"\n");
 * }
 *  
 *  
 ***************************************************************************/


#ifndef H_PS_TREE
#define H_PS_TREE

typedef enum {PSTree_Box, PSTree_Circle} box_shape;

typedef struct PSTree_node
{
  struct PSTree_node *Child, *Sibling, *Parent;
  float x, y, shade, width;
  box_shape shape;
} PSTree_node;

typedef void (*NodeFunc)(FILE *f, void *node);
typedef void (*EasyNodeFunc)(FILE *f, box_shape shape, void *node);

typedef struct PSTree_data
{
  PSTree_node *root;
  char *filename;
  char *title;
  NodeFunc func;
  int ParentLinks; /* 0 if no parent links */
} PSTree_data;



void GeneratePSTree(PSTree_data *data);



void FreePSTree(void *pn);


void BinaryPSTree(void *root, int left, int right,
		  EasyNodeFunc func, char *filename, char *title);

#endif
