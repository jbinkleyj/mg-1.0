/**************************************************************************
 *
 * timing.c -- Program timing routines
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
 *       @(#)timing.c	1.3 16 Mar 1994
 *
 **************************************************************************/

char *SCCS_Id_timing = "@(#)timing.c	1.3 16 Mar 1994";

#include <sys/types.h>
#include <math.h>

#include "sysfuncs.h"
#include "timing.h"


/*
 * Return the time in seconds since 00:00:00 GMT, Jan. 1, 1970 to the
 * best precision possible
 *
 */
double RealTime(void)
{
  return((double)time(NULL));
}





/*
 * Return the amount of system and user CPU used by the process to date to 
 * the best precision possible. If user is non-null then it is initialised to 
 * the user time. If sys is non-null then it is initialised to the system time.
 *
 */
double CPUTime(double *user, double *sys)
{
#if defined(SUNOS4) || defined(MIPS)
  struct rusage ruse;
  getrusage(RUSAGE_SELF, &ruse);
  if (user)
    *user = (double)ruse.ru_utime.tv_sec + 
      (double)ruse.ru_utime.tv_usec/1000000;
  if (sys)
    *sys = (double)ruse.ru_stime.tv_sec + 
      (double)ruse.ru_stime.tv_usec/1000000;
  return((double)ruse.ru_utime.tv_sec + ruse.ru_stime.tv_sec +
	 ((double)ruse.ru_utime.tv_usec + ruse.ru_stime.tv_usec)/1000000);

#else

#  if defined(SUNOS5) || defined(HP_PA)

  struct tms buffer;
  static double clk_tck = 0;
  double u, s;

  times(&buffer);

  if (clk_tck == 0)
    clk_tck = CLK_TCK;
  
  u = (double)buffer.tms_utime / clk_tck;
  s = (double)buffer.tms_stime / clk_tck;
  if (user)
    *user = u;
  if (sys)
    *sys = s;
  return u+s;

#  else

   -- > The CPUTime body needs to be defined.

#  endif

#endif
}




/*
 * Get the Real and CPU time and store them in a ProgTime structure
 */
void GetTime(ProgTime *StartTime)
{
  StartTime->RealTime = RealTime();
  StartTime->CPUTime = CPUTime(NULL, NULL);
}




/*
 * Display the Real and CPU time elapsed since the StartTime anf FinishTime 
 * structures were initialised. If FinishTime is NULL then FinishTime is
 * Now.
 */
char *ElapsedTime(ProgTime *StartTime, 
		 ProgTime *FinishTime)
{
  static char buf[50];
  double Real, CPU, hour, min, sec;
  if (!FinishTime)
    {
      Real = RealTime() - StartTime->RealTime;
      CPU = CPUTime(NULL, NULL) - StartTime->CPUTime;
    }
  else
    {
      Real = FinishTime->RealTime - StartTime->RealTime;
      CPU = FinishTime->CPUTime - StartTime->CPUTime;
    }
  hour = floor(CPU/3600);
  min = floor((CPU-hour*3600)/60);
  sec = CPU-hour*3600-min*60;
  sprintf(buf, "%02.0f:%02.0f:%05.2f cpu, %02d:%02d:%02d elapsed.", 
	  hour, min, sec,
	  ((int)ceil(Real))/3600, 
	  (((int)ceil(Real)) % 3600)/60, ((int)ceil(Real)) % 60);
  return(buf);
}






