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
/*!
 * @file   pmm_load.h
 * @brief  Load data structures
 *
 * Data structures representing the load and load history of a system
 *
 */

#ifndef PMM_LOAD_H_
#define PMM_LOAD_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>    // for pthread_rwlock_t

/*!
 * this is a circular array of load history, size determined at run time
 */
typedef struct pmm_loadhistory {
    int write_period; /*!< how often to write load history to disk */

    struct pmm_load *history;   /*!< pointer to the circular array */
    int size;                   /*!< size of circular array */
    int size_mod;

    struct pmm_load *start;   /*!< pointer to starting element of c.array */
    struct pmm_load *end;     /*!< pointer to ending element of c.array */
    int start_i;              /*!< ending element of the circular array */
    int end_i;                /*!< starting element in the circular array */

    char *load_path;          /*!< path to load history file */

    pthread_rwlock_t history_rwlock;    /*!< mutex for accessing history */
} PMM_Loadhistory;

/*!
 * this is a record of system load
 */
typedef struct pmm_load {
    time_t time;    /*!< time at which load was recorded */
    double load[3]; /*!< 1, 5 & 15 minute load averages TODO use float? */
} PMM_Load;


struct pmm_load* new_load();
struct pmm_loadhistory* new_loadhistory();
int init_loadhistory(struct pmm_loadhistory *h, int size);
void add_load(struct pmm_loadhistory *h, struct pmm_load *l);
int check_loadhistory(struct pmm_loadhistory *h);
void print_loadhistory(const char *output, struct pmm_loadhistory *h);
void print_load(const char *output, struct pmm_load *l);


#endif /*PMM_LOAD_H_*/

