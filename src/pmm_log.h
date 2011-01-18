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
#ifndef PMM_LOG_H_
#define PMM_LOG_H_
/*!
 * @file pmm_log.h
 *
 * @brief logging macros
 *
 * Header file for logging macros. Inspired by GridSolve/NetSolve
 */


#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>      // for fprintf
#include <time.h>       // for time
#include <sys/types.h>  // for getpid
#include <unistd.h>     // for getpid
#include <string.h>     // for strcmp

/* If debugging/output macros are not yet defined, define them now */


// SWITCHPRINTF macro takes one of these parameters as an argument to describe
// where to print a message (log, debug, error). 
#ifdef HAVE_ISO_VARARGS
#define PMM_DBG "0" //!< defines debug print stream for SWITCHPRINTF(OUTPUT,...)
#define PMM_LOG "1" //!< defines log print stream for SWITCHPRINTF(OUTPUT,...)
#define PMM_ERR "2" //!< defines error print stream for SWITCHPRINTF(OUTPUT,...)
#else
// If we do not have VARARGS a call to SWITCHPRINTF(PMM_DBG, "asdasd\n") must
// be macroed to printf("","asdasd") so we define the descriptors in such a way
#define PMM_DBG ""
#define PMM_LOG ""
#define PMM_ERR ""
#endif


/*!
 * \def DBGPRINTF(...)
 *
 * Prints a debug message to \a stderr stream including timestamp, calling
 * file, line and function, and the formatted string passed as argument (same
 * as printf). If debugging is not enabled no message will be emitted.
 *
 */
#ifndef DBGPRINTF
#  ifdef ENABLE_DEBUG
#    ifdef HAVE_ISO_VARARGS
#       define DBGPRINTF(...) do { time_t clock; struct tm now;\
    clock = time(0); localtime_r(&clock, &now); \
    fprintf(stderr,"[%02d/%02d/%04d %02d:%02d:%02d] [%s:%d] [%s] %d DBG: ",\
            now.tm_mon+1,now.tm_mday,now.tm_year+1900,\
            now.tm_hour,now.tm_min,now.tm_sec,\
            __FILE__,__LINE__,__FUNCNAME__, (int)getpid());\
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr); } while (0)
#    else  /* does not know ISO style varargs */
#       define DBGPRINTF printf("%s: %d [] %d DBG: ", __FILE__ , __LINE__ , -1 )+printf
#    endif  /* #ifdef HAVE_ISO_VARARGS */
#  else  /* dont want debugging */
#    ifdef HAVE_ISO_VARARGS
#      define DBGPRINTF(...)
#    else  /* does not know ISO style varargs */
static void DBGPRINTF(/*@unused@*/ const char *format, ...) {}
#    endif /* #ifdef HAVE_ISO_VARARGS */
#  endif /* #ifdef ENABLE_DEBUG  */
#endif /* #ifndef DBGPRINTF */


/*!
 * \def ERRPRINTF(...)
 *
 * prints an error message to \a stderr stream including timestamp, calling
 * file, line and function, and the formatted string passed as argument (same
 * as printf)
 */
#ifndef ERRPRINTF
#  ifdef HAVE_ISO_VARARGS
#    define ERRPRINTF(...) do { time_t clock; struct tm now;\
	clock = time(0); localtime_r(&clock, &now); \
	fprintf(stderr,"[%02d/%02d/%04d %02d:%02d:%02d] [%s:%d] [%s] %d ERR: ",\
			now.tm_mon+1,now.tm_mday,now.tm_year+1900,\
			now.tm_hour,now.tm_min,now.tm_sec,\
			__FILE__,__LINE__,__FUNCNAME__, (int)getpid()); \
			fprintf(stderr, __VA_ARGS__); \
			fflush(stderr); } while (0)
#  else
#       define ERRPRINTF printf
#  endif /* ifdef HAVE_ISO_VARARGS */
#endif /* ERRPRINTF */


/*!
 * \def LOGPRINTF(...)
 *
 * prints a logging message to \a stderr stream including timestamp, calling
 * file, line and function, and the formatted string passed as argument (same
 * as printf)
 */
//TODO change this to stdout ?
#ifndef LOGPRINTF
#  ifdef HAVE_ISO_VARARGS
#    ifdef PMM_DEBUG
#      define LOGPRINTF(...) do { time_t clock; struct tm now;\
	clock = time(0); localtime_r(&clock, &now); \
	fprintf(stderr,"[%02d/%02d/%04d %02d:%02d:%02d] [%s:%d] [%s] %d LOG: ",\
			now.tm_mon+1,now.tm_mday,now.tm_year+1900,\
			now.tm_hour,now.tm_min,now.tm_sec,\
			__FILE__,__LINE__,__FUNCNAME__,\
			(int)getpid());\
			fprintf(stderr, __VA_ARGS__); \
			fflush(stderr); } while (0)
#    else
#      define LOGPRINTF(...) do { time_t clock; struct tm now;\
	clock = time(0); localtime_r(&clock, &now); \
	fprintf(stderr,"[%02d/%02d/%04d %02d:%02d:%02d] [%s] %d LOG: ",\
			now.tm_mon+1,now.tm_mday,now.tm_year+1900,\
			now.tm_hour,now.tm_min,now.tm_sec,__FUNCNAME__,\
			(int)getpid());\
			fprintf(stderr, __VA_ARGS__); \
			fflush(stderr); } while (0)
#    endif
#  else
#       define LOGPRINTF printf
#  endif /* ifdef HAVE_ISO_VARARGS */
#endif /* LOGPRINTF */

/*!
 * \def SWITCHPRINTF(OUTPUT,...)
 *
 * calls LOGPRINTF/ERRPRINTF/DBGPRINTF depending on an argument \a OUTPUT
 * which may by one of PMM_LOG, PMM_ERR, PMM_DBG
 */
#ifndef SWITCHPRINTF
#   ifdef HAVE_ISO_VARARGS
#       define SWITCHPRINTF(OUTPUT,...) do { \
            if(strcmp(OUTPUT,PMM_LOG)==0) {\
                LOGPRINTF(__VA_ARGS__);\
            } else if(strcmp(OUTPUT, PMM_ERR)==0) {\
                ERRPRINTF(__VA_ARGS__);\
            } else if(strcmp(OUTPUT,PMM_DBG)==0) {\
                DBGPRINTF(__VA_ARGS__);\
            } else {\
                LOGPRINTF(__VA_ARGS__);\
            }\
        } while (0)
#   else
#       define SWITCHPRINTF printf
#   endif /* HAVE_ISO_VARARGS */
#endif /* SWITCHPRINTF */
    
#endif /*PMM_LOG_H_*/
