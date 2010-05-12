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
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
//#include <libxml/xmlmemory.h>
//#include <libxml/parser.h>
//
#ifdef HAVE_PAPI
#include <papi.h>
#endif

#include "pmm_util.h"

/* TODO Windows DDL libraries share data and so this library will need to be
 * modified so that these timeval structures are declared in the executing
 * program and not the library
 *
 * TODO return error codes from timer calls
 *
 * TODO determine whether it is necessary to get rusage stats on child processes
 * as well as self
 *
 */

void print_xml_results(struct timeval *walltime, struct timeval *usedtime);
int print_xml_time_element(xmlTextWriterPtr writer, const char *name,
                            struct timeval *time);
void print_plain_results(struct timeval *walltime, struct timeval *usedtime);

// PAPI variables
#ifdef HAVE_PAPI
float papi_realtime;
float papi_usedtime;
long_long papi_complexity;
float papi_mflops;
#else
// real time variables (wall clock)
struct timeval realtime_start;
struct timeval realtime_end;
struct timeval realtime_total;
struct timeval usedtime_total;
#endif

// used time variables (from rusage)
struct rusage usage_start;
struct rusage usage_end;
struct rusage usage_temp;

long long pmm_complexity;

#define XMLENCODING "ISO-8859-1"

void pmm_timer_init(long long complexity)
{
	pmm_complexity = complexity;
	return;
}

void pmm_timer_destroy() {

	return;
}

void pmm_timer_start() {
#ifdef HAVE_PAPI
    int ret;
#endif

	getrusage(RUSAGE_SELF, &usage_start);

#ifdef HAVE_PAPI
    ret = PAPI_flops(&papi_realtime, &papi_usedtime, &papi_complexity,
                     &papi_mflops);

    if(ret != PAPI_OK) {
        printf("Could not initialize PAPI_flops\n");
        printf("Your platform may not support the floating point operation.\n");
        exit(PMM_EXIT_GENFAIL);
    }
#else
	gettimeofday(&realtime_start, NULL);
#endif

	return;
}

void pmm_timer_stop() {

#ifdef HAVE_PAPI
    int ret;

    ret = PAPI_flops(&papi_realtime, &papi_usedtime, &papi_complexity,
                     &papi_mflops);

    if(ret != PAPI_OK) {
        printf("Could not finalize PAPI_flops\n");
        printf("Your platform may not support the floating point operation.\n");
        exit(PMM_EXIT_GENFAIL);
    }
#else
	gettimeofday(&realtime_end, NULL);
#endif

	getrusage(RUSAGE_SELF, &usage_end);

	return;
}

void pmm_timer_result() {


#ifdef HAVE_PAPI
    printf("%d %d\n", (int)papi_realtime, (int)(1000000.0*(papi_realtime-
                                                         (int)papi_realtime)));
    printf("%d %d\n", (int)papi_usedtime, (int)(1000000.0*(papi_usedtime-
                                                         (int)papi_usedtime)));
    printf("%lld\n", papi_complexity);
#else
	timersub(&realtime_end, &realtime_start, &realtime_total);

	// use usage_temp to store the total system and user timevals
	timersub(&usage_end.ru_utime, &usage_start.ru_utime, &usage_temp.ru_utime);
	timersub(&usage_end.ru_stime, &usage_start.ru_stime, &usage_temp.ru_stime);

	//add the total system and user timevals to usedtime
    //TODO rusage time not useful if routine is multithread and host is SMP
	timeradd(&usage_temp.ru_utime, &usage_temp.ru_stime, &usedtime_total);



	//TODO implment return of data via xml
	//print_xml_results(&realtime_total, &usedtime_total);
	print_plain_results(&realtime_total, &usedtime_total);
#endif


	return;
}

/*
 * TODO pass up return conditions instead of terminating
 */
void print_xml_results(struct timeval *walltime, struct timeval *usedtime) {

	int rc;
	xmlTextWriterPtr writer;
	xmlBufferPtr buf;

	buf = xmlBufferCreate();
	if(buf == NULL) {
		printf("pmm_timer_result: Error creating xml buffer\n");
		exit(-1);
	}

	writer = xmlNewTextWriterMemory(buf, 0);
	if(writer == NULL) {
		printf("pmm_timer_result: Error creating xml writer\n");
		exit(-1);
	}

	rc = xmlTextWriterSetIndent(writer, 1);
	if(rc < 0) {
		printf("pmm_timer_result: Error setting indent\n");
		exit(-1);
	}

	rc = xmlTextWriterStartDocument(writer, NULL, XMLENCODING, NULL);
	if(rc < 0) {
		printf("pmm_timer_result: Error at xmlTextWriterStartDocument\n");
		exit(-1);
	}

	rc = xmlTextWriterStartElement(writer, BAD_CAST "result");
	if(rc < 0) {
		printf("pmm_timer_result: Error creating \'result\' element\n");
		exit(-1);
	}

	rc = print_xml_time_element(writer, "walltime", walltime);
	if(rc < 0) {
		printf("print_xml_results: Error printing walltime result\n");
		exit(rc);
	}

	rc = print_xml_time_element(writer, "usedtime", usedtime);
	if(rc < 0) {
		printf("print_xml_results: Error printing usedtime result\n");
		exit(rc);
	}


	rc = xmlTextWriterEndDocument(writer);
	if(rc < 0) {
		printf("print_xml_results: Error closing xml document\n");
		exit(rc);
	}

	xmlFreeTextWriter(writer);

	printf("%s", (const char*)buf->content);

	xmlBufferFree(buf);

}

int print_xml_time_element(xmlTextWriterPtr writer, const char *name,
                            struct timeval *time) {
	int rc;

	rc = xmlTextWriterStartElement(writer, BAD_CAST name);
	if(rc < 0) {
		printf("print_xml_time_element: Error opening time element\n");
		return rc;
	}

	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "seconds", "%ld",
                                         (long)time->tv_sec);
	if(rc < 0) {
		printf("print_xml_time_element: Error writing seconds element\n");
		return rc;
	}

	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "useconds", "%ld",
                                         (long)time->tv_usec);
	if(rc < 0) {
		printf("print_xml_time_element: Error writing useconds\n");
		return rc;
	}

	rc = xmlTextWriterEndElement(writer);
	if(rc < 0) {
		printf("print_xml_time_element: Error closing time element\n");
		return rc;
	}

	return rc;

}

void print_plain_results(struct timeval *walltime, struct timeval *usedtime) {
	FILE *fp;

	printf("%ld %ld\n", walltime->tv_sec, walltime->tv_usec);
	printf("%ld %ld\n", usedtime->tv_sec, usedtime->tv_usec);
	printf("%lld\n", pmm_complexity);

	fp = fopen("/tmp/pmm_results", "w");
	fprintf(fp, "%ld %ld\n", walltime->tv_sec, walltime->tv_usec);
	fprintf(fp, "%ld %ld\n", usedtime->tv_sec, usedtime->tv_usec);
	fclose(fp);
}


