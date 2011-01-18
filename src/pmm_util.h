/*
    Copyright (C) 2008-2010 Robert Higgins
        Author: Robert Higgins <robert.higgins@ucd.ie>

    This file is part of PMM.

    PMM is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PMM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PMM.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PMM_UTIL_H_
#define PMM_UTIL_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif


/*extern struct timeval tstart;
extern struct timeval tend;
extern struct timeval tret;*/

/*
 * exit codes
 */
#define PMM_EXIT_SUCCESS 0
#define PMM_EXIT_ARGFAIL 1 //TODO rename ARGNFAIL
#define PMM_EXIT_ARGPARSEFAIL 2
#define PMM_EXIT_ALLOCFAIL 3
#define PMM_EXIT_EXEFAIL 4
#define PMM_EXIT_GENFAIL 5

/*
#define pmm_timersub(a, b, result) {                                          \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0)
}
*/
//
//#define pmm_rusagesub(a, b, result)
//  do {
//      pmm_timersub((a)->ru_utime, (b)->ru_utime, (result)->ru_utime);
//      pmm_timersub((a)->ru_stime, (b)->ru_stime, (result)->ru_stime);
//      (result)->ru_maxrss = (a)->ru_maxrss;
//      (result)->ru_ixrss = (a)->ru_ixrss;
//      (result)->ru_isrss = (a)->ru_isrss;
//      ()
//  }

void pmm_timer_init(long long complexity);
void pmm_timer_destroy();
void pmm_timer_start();
void pmm_timer_stop();
void pmm_timer_result();
//void pmm_rusagesub(struct rusage a, struct rusage b, struct rusage result);
//
#endif /*PMM_UTIL_H_*/
