/**************************************************************************
 *
 * PS_Tree.c -- Generates a PostScript drawing of a n-way tree
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
 *       @(#)PS_Tree.c	1.3 16 Mar 1994
 *
 **************************************************************************/

char *SCCS_Id_PSTree = "@(#)PS_Tree.c	1.3 16 Mar 1994";

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pwd.h>

#include "sysfuncs.h"
#include "messages.h"
#include "memlib.h"
#include "PS_Tree.h"

#define SCALE 10
#define MIN_X 1.5
#define MAX_Y 25
#define hgap 1.00


static float max_x, max_y, min_x;

static char ps_prolog[] =
  "\n"
  "5000 dict begin\n"
  "/cshow\n"
  "   { gsave newpath 0 0 moveto dup true charpath\n"
  "     flattenpath pathbbox grestore\n"
  "     exch 4 1 roll sub 2.0 div 3 1 roll exch sub 2.0 div exch\n"
  "     rmoveto show } def\n"
  "\n"
  "/linelink\n"
  "   { gsave setlinewidth setgray moveto lineto stroke grestore } def\n"
  "\n"
  "/bezierlink\n"
  "   { gsave setlinewidth setgray \n"
  "     /y2 exch def /x2 exch def /y1 exch def /x1 exch def\n"
  "     /dy y2 y1 sub 6 div def /dx x2 x1 sub 6 div def\n"
  "     x1 y1 moveto x1 dy sub y1 dx add x2 dy sub y2 dx add x2 y2 curveto\n"
  "     stroke grestore } def\n"
  "\n"
  "/box\n"
  "   { gsave dup scale 0.1 setlinewidth\n"
  "     /wid exch def\n"
  "     wid neg -1 moveto wid -1 lineto\n"
  "     wid 1 lineto wid neg 1 lineto closepath\n"
  "     gsave setgray fill grestore 0 setgray stroke grestore } def\n"
  "\n"
  "/circle\n"
  "   { gsave dup scale 0.1 setlinewidth\n"
  "     1.0 sub 2.0 div /wid exch def\n" 
  "     wid neg -1 moveto wid 0 1 270 90 arc\n"
  "     wid neg 0 1 90 270 arc closepath\n"
  "     gsave setgray fill grestore 0 setgray stroke grestore } def\n"
  "\n"
  "/makenode\n"
  "   { ITM setmatrix translate cvx exec } def\n"
  "\n"
  "0 setgray\n"
  "/c12 /Times-Roman findfont 12 scalefont def\n"
  "/tr10 /Times-Roman findfont 10 scalefont def\n"
  "/tr8 /Times-Roman findfont 8 scalefont def\n"
  "/tr6 /Times-Roman findfont 6 scalefont def\n"
  "/h12 /Helvetica findfont 12 scalefont def\n"
  "/h10 /Helvetica findfont 10 scalefont def\n"
  "/h8 /Helvetica findfont 8 scalefont def\n"
  "/h6 /Helvetica findfont 6 scalefont def\n"
  "/tb12 /Times-Bold findfont 12 scalefont def\n"
  "/tb10 /Times-Bold findfont 10 scalefont def\n"
  "/tb8 /Times-Bold findfont 8 scalefont def\n"
  "/tb6 /Times-Bold findfont 6 scalefont def\n"
  "/hb12 /Helvetica-Bold findfont 12 scalefont def\n"
  "/hb10 /Helvetica-Bold findfont 10 scalefont def\n"
  "/hb8 /Helvetica-Bold findfont 8 scalefont def\n"
  "/hb6 /Helvetica-Bold findfont 6 scalefont def\n"
  "/ITM matrix currentmatrix def\n"
  "\n";

static char ps_trailer[] =
  "\n"
  "end\n"
  "showpage\n"
  "\n";



static char *PSString(char *s)
{
  static char buf[1024];
  char *d = buf;
  while(*s)
    {
      if (*s == ')' || *s == '(' || *s == '\\')
	*d++ = '\\';
      *d++ = *s++;
    }
  *d = '\0';
  return(buf);
}

static void OutputLink(FILE *f, PSTree_node *pn, int ParentLink)
{
  char *linkname = ParentLink ? "bezierlink" : "linelink";
  if (pn)
    {
      PSTree_node *p;
      if (pn->Parent && ParentLink)
	fprintf(f, "%.2f %.2f %.2f %.2f 0.5 2 %s\n",
		(MIN_X+pn->x-min_x)*SCALE*2, 
		(MAX_Y-pn->y)*SCALE*3,
		(MIN_X+pn->Parent->x-min_x)*SCALE*2, 
		(MAX_Y-pn->Parent->y)*SCALE*3, linkname);
	
      for (p = pn->Child; p ; p = p->Sibling)
	{
	  OutputLink(f, p, ParentLink);
	  fprintf(f, "%.2f %.2f %.2f %.2f 0 2 %s\n",
		  (MIN_X+pn->x-min_x)*SCALE*2, 
		  (MAX_Y-pn->y)*SCALE*3,
		  (MIN_X+p->x-min_x)*SCALE*2, 
		  (MAX_Y-p->y)*SCALE*3, linkname);
	}
    }
}

static void OutputNode(FILE *f, PSTree_node *pn, NodeFunc func)
{
  if (pn)
    {
      fprintf(f, "%.2f %.2f %d %s %.2f %.2f makenode\n",
	      pn->shade,  pn->width, SCALE, 
	      pn->shape == PSTree_Box ? "/box" : "/circle", 
	      (MIN_X+pn->x-min_x)*SCALE*2, (MAX_Y-pn->y)*SCALE*3);
      func(f, pn);
      for (pn = pn->Child; pn; pn = pn->Sibling)
	OutputNode(f, pn, func);
    }
}

static void GeneratePostscript(PSTree_data *data)
{
  char *s;
  FILE *f;
  struct passwd *pwent;
  time_t curr_time;
  char buf[256];
  if (!(f = fopen(data->filename,"w")))
    FatalError(1,"Unable to open \"%s\"\n", data->filename);
  fprintf(f,"%%!PS-Adobe-2.0 EPSF-2.0\n");
  if (data->title)
    fprintf(f,"%%%%BoundingBox: %.0f %.0f %.0f %.0f\n", 
	    (MIN_X-min_x-0.1)*SCALE*2.0, (MAX_Y-max_y-0.6)*SCALE*3.0, 
	    (max_x-min_x+MIN_X+0.1)*SCALE*2.0, (MAX_Y+1.2)*SCALE*3.0);
  else
    fprintf(f,"%%%%BoundingBox: %.0f %.0f %.0f %.0f\n", 
	    (MIN_X-min_x-0.1)*SCALE*2.0, (MAX_Y-max_y-0.6)*SCALE*3.0, 
	    (max_x-min_x+MIN_X+0.1)*SCALE*2.0, (MAX_Y+0.6)*SCALE*3.0);

  fprintf(f,"%%%%Title: %s\n",data->title ? data->title : "A N-way tree");
  
  pwent = getpwuid(getuid());
  if ((s = strchr(pwent->pw_gecos, ',')) != NULL)
    *s = '\0';
  fprintf(f,"%%%%Creator: %s <%s>\n",pwent->pw_name, pwent->pw_gecos);
  endpwent();

  time(&curr_time);
  strftime(buf, sizeof(buf), "%B %e, %Y",localtime(&curr_time));
  fprintf(f,"%%%%CreationDate: %s\n", buf);
  fprintf(f,"%%%%EndComments\n");
  fprintf(f,"%%%%BeginProlog\n");
  fprintf(f,"%s",ps_prolog);
  fprintf(f,"%%%%EndProlog\n");
  fprintf(f,"%%%%Page: \"one\" 1\n");
  if (data->title)
    fprintf(f,"tr10 setfont %.2f %.2f moveto (%s) cshow\n", 
	    ((max_x-min_x)/2.0+MIN_X)*SCALE*2.0, (MAX_Y+1)*SCALE*3.0,
	    PSString(data->title)); 
  OutputLink(f, data->root, data->ParentLinks);
  fprintf(f, "\n");
  OutputNode(f, data->root, data->func);
  fprintf(f, "%%%%Trailer\n");
  fprintf(f,"%s",ps_trailer);
  fclose(f);
}



static float *xmins;
static float *xmaxs;

#if 0
static double RightizeNode(PSTree_node *pn, int l, double x, double hsp)
{
  if (pn)
    {
      double lx, rx;
      x = xmaxs[l] < x ? xmaxs[l] : x;
      x = pn->x > x ? pn->x : x;
      rx = RightizeNode(pn->Right, l+1, x + hsp, hsp);
      if (!isnan(rx) && rx-hsp < x)
	x = rx-hsp;
      lx = RightizeNode(pn->Left, l+1, x - hsp, hsp);
      if (!isnan(rx) && !isnan(lx))
	x = (rx+lx)/2;
      pn->x = x;
      xmaxs[l] = x - hsp*2;
      xmaxs[l+1] -= hadd;
      return(pn->x);
    }
  else
    return(quiet_nan());
}


static double PositionNode(PSTree_node *pn, int l, double x, double hsp)
{
  if (pn)
    { 
      double lx, rx;
      if (pn->Left && !pn->Right)
	{
	  pn->Right = pn->Left;
	  pn->Left = NULL;
	}
      pn->y = l;
      if (pn->y > max_y)
	max_y = pn->y;
      x = xmins[l] > x ? xmins[l] : x;
      lx = PositionNode(pn->Left, l+1, x - hsp, hsp);
      if (!isnan(lx) && lx+hsp > x) 
	x = lx + hsp;
      if (pn->Left)
	rx = PositionNode(pn->Right, l+1, x + hsp, hsp);
      else
	rx = PositionNode(pn->Right, l+1, x);
      if (!isnan(rx))
	xmaxs[l+1] = rx;
      lx = RightizeNode(pn->Left, l+1, rx - hsp*2, hsp);
      if (!isnan(rx) && !isnan(lx))
	x = (rx+lx)/2;
      pn->x = x;
      xmins[l] = x + hsp*2;
      xmins[l+1] += hadd;
      if (xmins[l] > max_x)
	max_x = xmins[l];
      
      return(pn->x);
    }
  else
    return(quiet_nan());
}

#elif 0

static double PositionNode(PSTree_node *pn, int l, double x)
{
  if (pn->Right && !pn->Left)
    {
      pn->Left = pn->Right;
      pn->Right = NULL;
    }
  pn->y = l;
  if (pn->y > max_y) max_y = pn->y;
  x = xmins[l]+pn->width/2 > x ? xmins[l]+pn->width/2 : x;
  if (pn->Left)
    {
      double lx, lwx;
      lwx = pn->Right ? (pn->Left->width + hgap)/2.0 : 0.0;
      lx = PositionNode(pn->Left, l+1, x-lwx);
      x = lx+lwx;
      if (pn->Right)
	{
	  double rx, rwx;
	  rwx = (pn->Right->width + hgap)/2.0;
	  rx = PositionNode(pn->Right, l+1, x+rwx);
	  x = (lx+rx)/2.0;
	}
    }
  pn->x = x;
  xmins[l] = x + (pn->width)/2.0 + hgap;
  x = x + pn->width/2.0;
  if (x > max_x)
    max_x = x;
  return(pn->x);
}

#else
 
static double PositionNode(PSTree_node *pn, int l, double x)
{
  pn->y = l;
  if (pn->y > max_y) max_y = pn->y;
  x = xmins[l]+pn->width/2 > x ? xmins[l]+pn->width/2 : x;
  if (pn->Child)
    {
      PSTree_node *p;
      double lx = 0.0, lwx = 0.0;
      int num;
      for (p = pn->Child; p->Sibling; p = p->Sibling)
	lwx += hgap + p->width;
      if (p->Sibling)
	lwx += hgap + (p->Sibling->width + pn->width)/2.0;
      
      for (p = pn->Child, num = 0; p; p = p->Sibling, num++)
	lx += PositionNode(p, l+1, x-lwx/2.0);
      x = lx/num;
    }
  pn->x = x;
  xmins[l] = x + (pn->width)/2.0 + hgap;
  if (x + pn->width/2.0 > max_x)
    max_x = x + pn->width/2.0;
  if (x - pn->width/2.0 < min_x)
    min_x = x - pn->width/2.0;
  return(pn->x);
} 
#endif




static void PositionPSNodes(PSTree_node *root)
{
  xmins = Xmalloc(sizeof(xmins[0])*100);
  xmaxs = Xmalloc(sizeof(xmaxs[0])*100);
  if (!xmins || !xmaxs)
    FatalError(1, "Unable to allocate memory for Postscript tree\n");
  max_x = 0; min_x = 1e10;
  max_y = 0;
  bzero((char*)xmins, sizeof(xmins[0])*100);
  if (root)
    PositionNode(root, 0, 0);
  free(xmaxs);
  free(xmins);
}


void GeneratePSTree(PSTree_data *data)
{
  PositionPSNodes(data->root);
  GeneratePostscript(data);
}

void FreePSTree(void *pn)
{
  if (pn)
    {
      PSTree_node *p = ((PSTree_node*)pn)->Child;
      while (p)
	{
	  PSTree_node *n = p->Sibling;
	  FreePSTree(p);
	  p = n;
	}
      free(pn);
    }
}


