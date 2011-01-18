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
 * @file    pmm_load.c
 * @brief   Code for handling load structure
 *
 * Contains functions for dealing with the load data structures.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>     // for malloc/free
#include <time.h>       // for time_t

#include "pmm_load.h"
#include "pmm_log.h"

/*!
 * Allocates and initialises memory for a new load history structure. This is
 * a circular array arrangement with pointers to the beginning and end. 
 * Iteration over the array is done using the modulus operator to wrap the
 * physically beginning and end addresses.
 *
 * note for a given size we must keep size+1 elements in the array. Thus we
 * store two different sizes. 'size' refers to the number of accessible elements
 * in the circular array. 'size_mod' refers to actual allocated elements in the
 * array and is only to be used when iterating through the accessible elements
 * using i+1%size_mod. 'size_mod' will always be the accessible size 'size' + 1
 *
 * @return pointer to a new loadhistory structure or NULL on failure
 */
struct pmm_loadhistory* new_loadhistory()
{
	struct pmm_loadhistory *h;

	h = malloc(sizeof *h);
    if(h == NULL) {
        ERRPRINTF("Error allocating load history structure.\n");
        return NULL;
    }

	h->load_path = LOCALSTATEDIR"/loadhistory";

	h->write_period = 10;

	h->size = 0;
	h->size_mod = 1;
	h->history = NULL;


	h->start = NULL;
	h->end = NULL;

	h->start_i = -1;
	h->end_i = -1;

	return h;
}

/*!
 * Function initialises the array and indexes that are used to store the load
 * history (in a circular type array)
 *
 * @param   h       pointer to a load history structure
 * @param   size    size of the circular array
 *
 * @return 0 on success, -1 on failure
 *
 * TODO check that history is not already allocated, free in this case first
 */
int
init_loadhistory(struct pmm_loadhistory *h, int size)
{
	h->size = size;
	h->size_mod = size+1;

	h->history = malloc(h->size_mod * sizeof *(h->history));
    if(h->history == NULL) {
        ERRPRINTF("Error allocating load history memory.\n");
        return -1;
    }

	h->start = &h->history[0];
	h->end = &h->history[0];

	h->start_i = 0;
	h->end_i = 0;

	return 0;
}

/*!
 * This function _copies_ the structure l into the load history structure which
 * is a circular array.
 *
 * @param   h   pointer to the load history structure
 * @param   l   pointer to the load to be copied into the next free/expired
 *              element of the circular array
 *
 *
 */
void add_load(struct pmm_loadhistory *h, struct pmm_load *l)
{
#ifdef ENABLE_DEBUG
	print_load(PMM_DBG, l);
	DBGPRINTF("h:%p\n", h);
	DBGPRINTF("h->end_i: %d\n", h->end_i);
#endif

   // end_i always points to a vacant element
	h->history[h->end_i].time = l->time;
	h->history[h->end_i].load[0] = l->load[0];
	h->history[h->end_i].load[1] = l->load[1];
	h->history[h->end_i].load[2] = l->load[2];

    //TODO decide whether to use indexes or pointers
    
	/* Increment the end index and set it to zero if it reaches h->size_mod */
	h->end_i = (h->end_i + 1) % h->size_mod;

	/* if end index and start index are the same rotate the start index by 1 */
	if(h->end_i == h->start_i) {
		h->start_i = (h->start_i + 1) % h->size_mod;
	}

	/* obliterate the history element that end_i points to inorder to avoid
	 * confusion with the elements that are part of the rotating array */
	h->history[h->end_i].time = (time_t)0;
	h->history[h->end_i].load[0] = 0.0;
	h->history[h->end_i].load[1] = 0.0;
	h->history[h->end_i].load[2] = 0.0;

}

/*!
 * Do some sanity checking on the load history structure
 *
 * @param   h   pointer to load history
 *
 * @returns 1 if load history passes check, 0 if it fails
 */
int check_loadhistory(struct pmm_loadhistory *h) {

	if(h->size < 1) {
		LOGPRINTF("Error, loadhistory size is less than 1.\n");
		return 0;
	}
	else if(h->size_mod != h->size+1) {
		LOGPRINTF("Error, loadhistory internal variable is corrupted.\n");
		return 0;
	}
	else if(h->history == NULL) {
		LOGPRINTF("Error, load history array is not allocated\n.");
		return 0;
	}
	else if(h->write_period < 0) {
		LOGPRINTF("Error, load history write to disk period is negative.\n");
		return 0;
	}
	else if(h->load_path == NULL) {
		LOGPRINTF("Error, no history file specified.\n");
		return 0;
	}

	return 1;
}

/*!
 * initialized a new load observation structure
 *
 * @returns pointer to an allocated and intialized load structure
 */
struct pmm_load* new_load() {
	struct pmm_load *l;

	l = malloc(sizeof *l);
    //TODO NULL check

	l->time = (time_t)0;
	l->load[0] = 0.0;
	l->load[1] = 0.0;
	l->load[2] = 0.0;

	return l;
}

/*!
 * prints a load history sequence
 *
 * @param   output      output stream to print to
 * @param   h           pointer to the load history
 */
void print_loadhistory(const char *output, struct pmm_loadhistory *h) {
	int i;

	SWITCHPRINTF(output, "history:%p size: %d start_i:%d end_i:%d\n",
		       	 h->history, h->size, h->start_i, h->end_i);
	SWITCHPRINTF(output, "load_path: %s\n", h->load_path);

	i = h->start_i;
	while(i != h->end_i) {
		print_load(output, &h->history[i]);
		i = (i + 1) % h->size_mod;
	}
	//print_load(&h->history[c]);

}

/*!
 * prints a single load observation
 *
 * @param   output      output stream to print to
 * @param   l           pointer to load structure to print
 */
void print_load(const char *output, struct pmm_load *l) {
	SWITCHPRINTF(output, "time:%d loads:%.2f %.2f %.2f\n", (int)l->time,
            l->load[0], l->load[1], l->load[2]);
}

/*!
 * frees a load history structure and all of its members
 *
 * @param   h    pointer to address of the load history structure
 */
void free_loadhistory(struct pmm_loadhistory **h) {
	free((*h)->load_path);
    (*h)->load_path = NULL;

	free((*h)->history);
    (*h)->history = NULL;

	free(*h);
    *h = NULL;
}

