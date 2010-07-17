#ifndef PMM_LOG_H_
#define PMM_LOG_H_

/*
 *
 *
 * Header file for logging routines and macros.
 *
 * originally from GridSolve
 */


#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <time.h>

#include <sys/types.h> //for getpid
#include <unistd.h>

/* If debugging/output macros are not yet defined, define them now */


// SWITCHPRINTF macro takes one of these parameters as an argument to describe
// where to print a message (log, debug, error). 
#ifdef HAVE_ISO_VARARGS
#define PMM_DBG "0"
#define PMM_LOG "1"
#define PMM_ERR "2"
#else
// If we do not have VARARGS a call to SWITCHPRINTF(PMM_DBG, "asdasd\n") must
// be macroed to printf("","asdasd") so we define the descriptors in such a way
#define PMM_DBG ""
#define PMM_LOG ""
#define PMM_ERR ""
#endif


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
