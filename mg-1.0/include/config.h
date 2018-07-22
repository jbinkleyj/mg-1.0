/**************************************************************************
 *
 * config.h -- Macros for detecting the current OS
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
 *       @(#)config.h	1.2 09 Mar 1994
 *
 **************************************************************************/

#ifndef H_CONFIG
#define H_CONFIG



#if defined(sun) && defined(sparc)

#define SPARC
    /* Test for SunOS 5.x */
#include <errno.h>

#ifdef ECHRNG
#  define SUNOS5
#else
#  define SUNOS4
#endif

#endif

#if defined(mips)
#  define MIPS
#  ifdef ultrix
#    define ULTRIX
#  else
#    ifdef _SYSTYPE_SVR4
#      define IRIX5
#    else
#      define RISCOS  /* or IRIX 4.X */
#    endif
#  endif
#endif

#if defined(_PA_RISC1_0) || defined(_PA_RISC1_1)
#  define HP_PA
#endif

#endif
