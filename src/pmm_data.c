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
 * @file pmm_data.c
 *
 * @brief functions for operating on PMM model, benchmark and other data
 * structures
 *
 * this file contains the code for operating on model and associated data
 * structures of the pmm applications
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
//#include <sys/time.h>
#include <ctype.h> //for isdigit
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <sys/types.h> //getpid
#include <unistd.h> //getpid
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "pmm_data.h"
#include "pmm_octave.h"
#include "pmm_interval.h"
#include "pmm_param.h"
#include "pmm_load.h"
#include "pmm_log.h"

/*
 * TODO model completion should be a member of the benchmark structure,
 * not the routine structure
 */

/******************************80 columns**************************************/
 
 /*!
 * Allocates memory for the pmm_config structure. Sets some default values
 * in the config.
 *
 * @return pointer to newly allocated pmm_config structure or NULL on failure
 */
struct pmm_config* new_config() {
	struct pmm_config *c;

	c = malloc(sizeof *c);

	c->routines = malloc(5 * sizeof *(c->routines));
	if(c->routines == NULL) {
		ERRPRINTF("allocation of memory to routine array failed.\n");

        free(c);
        c = NULL;

        return NULL;
	}
	c->allocated = 5;
	c->used = 0;

    c->loadhistory = (void *)NULL;

	c->daemon = 0;
    c->build_only = 0;
    c->configfile = SYSCONFDIR"/pmmd.conf";
	c->logfile = "./pmmd.log"; //TODO  set default log file directory

    c->ts_main_sleep_period.tv_sec = 1;
    c->ts_main_sleep_period.tv_nsec = 0;
    c->num_execs_threshold = 20;
    c->time_spend_threshold = 60;

    c->pause = 0;

	return c;
}

/*!
 * Allocates memory for a new routine and sets some default values
 *
 * @return pointer to newly allocated routine structure
 */
struct pmm_routine*
new_routine()
{
	struct pmm_routine *r;

	r = malloc(sizeof *r);

    r->name = NULL;
    r->exe_path = NULL;
    r->exe_args = NULL;

    r->pd_set = new_paramdef_set();

    r->condition = CC_INVALID;
    r->priority = -1;
    r->executable = -1;

    r->construction_method = CM_INVALID;
    r->min_sample_num = -1;
    r->min_sample_time = -1;
    r->max_completion = -1;

	r->model = new_model();

	r->model->parent_routine = r;

	return r;
}

/*!
 * Create a new model structure. Note pmm_bench_list member will not yet be
 * allocated.
 *
 * @return  pointer to newly allocated pmm_model structure
 */
struct pmm_model* new_model()
{
	struct pmm_model *m;

	m = malloc(sizeof *m);

	m->model_path = (void *)NULL;
    m->unwritten_num_execs = 0;
    m->unwritten_time_spend = 0.0;

    m->mtime = 0;

	m->n_p = -1;
	m->completion = 0;
	m->complete = 0;
    m->unique_benches = 0;

	m->bench_list = (void *)NULL; // init when setting n_p

	m->interval_list = new_interval_list();

    m->parent_routine = (void *)NULL;

	return m;
}

/*!
 * Create an empty benchmark list
 *
 * @param   m   pointer to the parent model
 * @param   n_p number of parameters of the benchmarks in the list
 *
 * @return  pointer to newly allocated pmm_bench_list
 */
struct pmm_bench_list*
new_bench_list(struct pmm_model *m,
               int n_p)
{
	struct pmm_bench_list *bl;

	bl = malloc(sizeof *bl);

	if(bl == NULL) {
		ERRPRINTF("Error allocating memory.\n");
		return NULL;
	}

	bl->size = 0;
	bl->n_p = n_p;
	bl->first = NULL;
	bl->last = NULL;
	bl->parent_model = m;

	return bl;
}

/*!
 * Initialise the model benchmark list with zero speed benchmarks at relevant
 * points. For each parameter definition if the end value of a parameter is
 * defined as having a zero speed (i.e. it is not a 'nonzero_end'), then create
 * zero speed benchmarks at the end point on that parameter axis, i.e. for
 * parameter i create a zero speed benchmark at at:
 *
 * start_0,start_1,...,end_i,...,start_n-1,start_n.
 *
 * The, if any parameter has a zero speed end defined, assume we cannot
 * ever benchmark at end_0,end_1,...,end_n and set a zero speed benchmark at
 * this point also.
 *
 * @param   m           pointer to the model that the bench list belongs to 
 * @param   pd_array    pointer to the parameter definition array of the model
 * @param   n_p         number of parameters in the model/pd array
 *
 * @return 0 on success, -1 on failure
 */
int
init_bench_list(struct pmm_model *m, struct pmm_paramdef_set *pd_set)
{
	int i;
    int ret;
    int all_nonzero_end;
	struct pmm_benchmark *b;

	m->bench_list = malloc(sizeof *(m->bench_list));
	if(m->bench_list == NULL) {
		ERRPRINTF("Error allocating memory.\n");
		return -1; //failure;
	}

	m->bench_list->size = 0;
	m->bench_list->n_p = pd_set->n_p;

	m->bench_list->first = NULL;
	m->bench_list->last = NULL;

	m->bench_list->parent_model = m;

    // check if any parameters are set to nonzero end (bench at this point
    // instead of assuming it to have zero speed)
    all_nonzero_end = 1;
    for(i=0; i<pd_set->n_p; i++) {
        if(pd_set->pd_array[i].nonzero_end != 1) {
            b = new_benchmark();

            b->n_p = pd_set->n_p;

            b->p = init_param_array_start(pd_set);
            if(b->p == NULL) {
                ERRPRINTF("Error allocating memory.\n");
                free_benchmark(&b);
                return -1; //failure
            }
        
            b->p[i] = pd_set->pd_array[i].end;

            b->flops = 0.;

            if(insert_bench_into_list(m->bench_list, b) < 0) {
                ERRPRINTF("Error inserting end axial point into list.\n");
                //free_benchmark(&b); not sure about freeing here ....
                return -1;
            }

            all_nonzero_end = 0;

        }
    }

    // if not all parameters have nonzero_end set, then we should not benchmark
    // at the end,end...,end point, so set a zero speed benchmark here.
    if(all_nonzero_end != 1) {

        b = new_benchmark();

        b->n_p = pd_set->n_p;
        b->p = init_param_array_end(pd_set);
        if(b->p == NULL) {
            ERRPRINTF("Error initialising end parameters.\n");
            return -1;
        }

        b->flops = 0.;


        ret = insert_bench_into_list(m->bench_list, b);
        if(ret < 0) {
            ERRPRINTF("Error inserting initial bench into list.\n");
            //free_benchmark(&b) not sure about freeing here ...
            return -1;
        }
    }

	return 0; // success
}



/*!
 * Zero data-fields of a benchmark structure, leaving the parameter array p and
 * the next and previous pointers untouched
 *
 * @param   b   pointer to benchmark to zero
 *
 * @pre b is non-NULL
 */
void
zero_benchmark(struct pmm_benchmark *b)
{
    b->complexity = 0;
    b->flops = 0;
    b->seconds = 0;

    b->wall_t.tv_sec = 0;
    b->wall_t.tv_usec = 0;

    b->used_t.tv_sec = 0;
    b->used_t.tv_usec = 0;

    return;
}

/*!
 * Allocate and return a benchmark structure with values set to invalids,
 * with the parameter array p still unallocated and next/previous pointers
 * set to NULL
 *
 * @return  pointer to a newly allocated benchmark structure
 */
struct pmm_benchmark*
new_benchmark()
{
	struct pmm_benchmark *b;

	b = malloc(sizeof *b);

	b->n_p = -1;
	b->p = (void *)NULL; //init when assigning parameters

	b->complexity = -1;
	b->flops = -1;
	b->seconds = -1;

	b->wall_t.tv_sec = -1;
	b->wall_t.tv_usec = -1;

	b->used_t.tv_sec = -1;
	b->used_t.tv_usec = -1;

	b->next = (void *)NULL; //set when inserting into model
	b->previous = (void *)NULL;

	return b;
}

/*!
 * Initialize and return a benchmark structure with zero speed in a position
 * described by the parameter array argument (params).
 *
 * @param   params      pointer to parameter array describing benchmark position
 * @param   n_p         number of parameters
 *
 * @return  pointer to a newly allocated benchmark structure with zero speed in
 * given position
 */
struct pmm_benchmark*
init_zero_benchmark(int *params, int n_p)
{
    struct pmm_benchmark *b;

    b = new_benchmark();

    if(b == NULL) {
        ERRPRINTF("Error allocating new zero speed benchmark.\n");
        return NULL;
    }

    b->n_p = n_p;
    b->p = init_param_array_copy(params, n_p);
    if(b->p == NULL) {
        ERRPRINTF("Error copy parameter array.\n");

        free_benchmark(&b);

        return NULL;
    }

    b->flops = 0.;

    return b;
}

/*!
 * This function adds a pmm_routine structure to the routines array of
 * the pmm_config structure. The array is reallocated as required, adding
 * space for 5 routines at a time to prevent frequent reallocation.
 *
 * @param   c   pointer to the config structure
 * @param   r   pointer to the routine
 *
 * @return 0 on success, -1 on failure
 */
int
add_routine(struct pmm_config *c, struct pmm_routine *r) {
	// if we still have space in the routines array
	if(c->used < c->allocated) {

		// add routine
		c->routines[c->used] = r;
		c->used++;
	}
	// otherwise ...
	else {
		// reallocate the routines array
		c->allocated+=5;
		c->routines = realloc(c->routines,
		                      sizeof(struct pmm_routine*)*c->allocated);
		if(c->routines == NULL) {
			ERRPRINTF("Reallocation of memory to routine array failed.\n");
            return -1;
		}

		// and add the routine
		c->routines[c->used] = r;
		c->used++;
	}

    r->parent_config = c;

	return 0;
}


/*!
 * Insert benchmark between two benchmarks in a sorted list
 *
 * @param   list_start  pointer to a reference for the beginning of the list
 * @param   list_end    pointer to a reference for the end of the list
 * @param   b           pointer to the benchmark to insert
 *
 * @return 0 on success if benchmark was not unique to the list, 1 on success if
 * the benchmark was unique to the list, -1 on failure
 */
int
insert_bench_into_sorted_list(struct pmm_benchmark **list_start,
                              struct pmm_benchmark **list_end,
                              struct pmm_benchmark *b)
{
    int done;
    int ret;
    int unique = 0;
    struct pmm_benchmark *this;

    done = 0;
    this = *list_start;

    while(this != NULL) {
        if((ret = params_cmp(b->p, this->p, b->n_p)) <= 0) {
            if(insert_bench_into_sorted_list_before(list_start, list_end, 
                                                    this, b) < 0)
            {
                ERRPRINTF("Error inserting bench into list.\n");
                return -1;
            } 

            // if params_cmp did not match the new benchmark, set unique flag
            if(ret != 0)
                unique = 1;

            done = 1;
            break;
        }

        this = this->next;
    }

    if(done != 1) {
        if(insert_bench_into_sorted_list_after(list_start, list_end,*list_end,b)
           < 0)
        {
            ERRPRINTF("Error inserting bench into list.\n");
            return -1;
        }

        // this is a new bench so set unqiue
        unique = 1;
    }

    return unique;
}

/*!
 * @return 0 on success, -1 on failure
 */
int
insert_bench_into_list(struct pmm_bench_list *bl,
                       struct pmm_benchmark *b)
{
    int ret;

    if((ret = insert_bench_into_sorted_list(&(bl->first), &(bl->last), b)) < 0)
    {
        print_bench_list(PMM_ERR, bl);
        print_benchmark(PMM_ERR, b);
        ERRPRINTF("Error inserting bench into list.\n");
        return -1;
    }

	// in any case, if we reach this point, a benchmark has been inserted
	bl->size++;
	bl->parent_model->completion++;
    bl->parent_model->unique_benches += ret;

    return 0;

}


/*!
 * Add benchmark to a model in the appropriate substructure of
 * the model
 *
 * @param   m   pointer to the model
 * @param   b   pointer to the benchmark
 *
 * @post parameter b becomes part of the model and must not be freed until
 * the model itself is freed or it is removed from the model explicitly
 *
 * @return 0 on success, -1 on failure
 */
int
insert_bench(struct pmm_model *m, struct pmm_benchmark *b)
{
	int ret;

    ret = insert_bench_into_list(m->bench_list, b);
    if(ret < 0) {
        ERRPRINTF("Error inserting bench in bench list\n");
        return ret;
    }

	return ret;
}


/*!
 *
 * test if a benchmark is at the starting point of a model (not 0,0)
 *
 * @param   n           number of parameters defintions
 * @param   pd_array    pointer to parameter defintions array
 * @param   b           pointer to benchmark
 *
 * @return 0 if benchmark is not at the origin of the model (all start values)
 * @return 1 if benchmark is at the origin of the model
 * @return -1 if there is a mismatch between number of parameters
 */
int is_benchmark_at_origin(int n, struct pmm_paramdef *pd_array,
                           struct pmm_benchmark *b)
{
	int i;

	if(n!=b->n_p) {
		ERRPRINTF("paramdef array and benchmark param array size mismatch\n");
		return -1;
	}

	for(i=0; i<n; i++) {
        LOGPRINTF("b->p[%d]:%d paramdef[%d].start:%d\n", i, b->p[i], i,
                  pd_array[i].start);
		if(b->p[i] != pd_array[i].start) {
			return 0;
		}
	}

	return 1; // all bench parameters are start, bench is at origin
}

/*!
 * An empty model is defined as one that has no experimentally obtained
 * benchmark points in it. Thus:
 *
 * The bench list may only have: points at the end parameter values on each
 * parameter axis, e.g. [0,0,end] for the 3rd axis of a 3 parameter
 * model, and a single point at the end parameter value of all boundaries,
 * i.e. [end,end,end] of a 3 parameter model
 *
 * This is slightly complicated to test however, another property of non-
 * experimental points in the model is that they all have zero speed. Just test
 * that!
 *
 * @param   m   pointer to the model to test
 *
 * @return 0 if model is not empty, 1 if model is empty
 */
int
isempty_model(struct pmm_model *m)
{
	struct pmm_benchmark *b;


	b = m->bench_list->first;
	if(b == NULL) {
		ERRPRINTF("Mesh model not initialized.\n");
	}

    while(b != NULL) {
        if(b->flops != 0.0) {
            return 0;
        }
        b=b->next; 
    }

	//list is empty of experimental points
	return 1;
}

/*!
 * count the number of benchmarks in a model
 *
 * @param   m   pointer to model
 * @return number of benchmarks
 */
int
count_benchmarks_in_model(struct pmm_model *m)
{
	int c = 0; //counter

	c = count_benchmarks_in_bench_list(m->bench_list);

	return c;
}


/*!
 * Count the number of benchmarks in a list (this manually counts, does not
 * return the size variable of the list).
 *
 * @param   bl  pointer to the bench list to count
 *
 * @return  number of benchmarks in the list
 */
int
count_benchmarks_in_bench_list(struct pmm_bench_list *bl)
{
	int c = 0; //counter
	struct pmm_benchmark *b;

	b = bl->first;
	while(b != NULL) {
		c++;
		b = b->next;
	}

    LOGPRINTF("c:%d\n", c);
	return c;
}

/*!
 * Count the number of unique benchmark points in a sorted list of benchmarks.
 *
 * @param   first   pointer the first benchmark in the list
 *
 * @return number of unqiue benchmarks in the list
 */
int
count_unique_benchmarks_in_sorted_list(struct pmm_benchmark *first)
{
    int count;
    struct pmm_benchmark *b;

    count = 0;
    b = first;

    while(b != NULL) {
        count++;
        b = get_next_different_bench(b);
    }

    return count;
}

/*!
 * Test if a benchmark is on one of the parameter axes of the model (i.e. all
 * but one parameter is at a start),
 *
 * @param   m   pointer to the model the benchmark belongs to
 * @param   b   pointer to the benchmark
 *
 * @return the plane index of the axis the benchmark belongs to or -1 if
 * on all boundaries (i.e. at origin), or -2 if on no boundaries
 */
int
benchmark_on_axis(struct pmm_model *m,
                  struct pmm_benchmark *b)
{
	return param_on_axis(b->p, b->n_p, m->parent_routine->pd_set->pd_array);
}

/*!
 * Copy a benchmark from one pointer to another. next and previous elments
 * of the benchmark structure are not copied however. The copy exists outside
 * of any benchmark list that the source might belong to.
 *
 * @param   dst     pointer to destination benchmark
 * @param   src     pointer to source benchmark
 *
 * @return -1 on failure, 0 on success
 *
 * @pre dst structure is non NULL, but it's p array element is NULL
 *
 */
int copy_benchmark(struct pmm_benchmark *dst, struct pmm_benchmark *src)
{

	dst->n_p = src->n_p;

    dst->p = init_param_array_copy(src->p, src->n_p);
    if(dst->p == NULL) {
        ERRPRINTF("Error copying parameter array.\n");
        return -1;
    }

	dst->complexity = src->complexity;
	dst->flops = src->flops;
	dst->seconds = src->seconds;

    //copy from -> to
	copy_timeval(&dst->wall_t, &src->wall_t);
	copy_timeval(&dst->used_t, &src->used_t);

	dst->next = (void *)NULL;
	dst->previous = (void *)NULL;

	return 0;
}


/*!
 * Insert benchmark into a sorted list in a position directly before a
 * certain benchmark.
 *
 * @param   list_first  pointer to the first benchmark in the list
 * @param   list_last   pointer to the last benchmark in the list
 * @param   before      pointer to the benchmark before which we will insert
 * @param   b           pointer to the benchmark we will insert
 *
 * @pre the benchmark b has parameter value lower than the benchmark 'before'
 * @pre after is NULL pointer only if list is empty
 *
 * @return 0 on success, -1 on failure
 */
int
insert_bench_into_sorted_list_before(struct pmm_benchmark **list_first,
                                     struct pmm_benchmark **list_last,
		                             struct pmm_benchmark *before,
                                     struct pmm_benchmark *b)
{
	//if before is NULL then we may have an empty list
	if(before == NULL) {

		if(*list_first == NULL && *list_last == NULL) { //if empty list
			b->previous = (void *)NULL;
			b->next = (void *)NULL;

			*list_first = b;
			*list_last = b;

		}
		else { //else error, shouldn't have been passed a NULL benchmark
			ERRPRINTF("Insertion before NULL benchmark in non-empty list.\n");
            return -1;
		}
	}
	else if(before == *list_first) { //if inserting before first element in list
		b->previous = (void *)NULL;
		b->next = before;
		before->previous = b;
		*list_first = b;

	}
	else { //inserted midway through the list
		b->previous = before->previous;
		b->next = before;
		before->previous->next = b;
		before->previous = b;

	}

	return 0;
}

/*!
 * Insert benchmark into a sorted list in a position directly after a
 * certain benchmark.
 *
 * @param   list_first  pointer to the first benchmark in the list
 * @param   list_last   pointer to the last benchmark in the list
 * @param   after       pointer to the benchmark after which we will insert
 * @param   b           pointer to the benchmark we will insert
 *
 * @pre the benchmark b has parameter value higher than the benchmark 'after'
 * @pre after is NULL pointer only if list is empty
 *
 * @return 0 on success, -1 on failure
 */
int
insert_bench_into_sorted_list_after(struct pmm_benchmark **list_first,
                                    struct pmm_benchmark **list_last,
		                            struct pmm_benchmark *after,
                                    struct pmm_benchmark *b)
{
	//if after is NULL then we may have an empty list
	if(after == NULL) {

		if(*list_first == NULL && *list_last == NULL) { //if empty list
			b->previous = (void *)NULL;
			b->next = (void *)NULL;

			*list_first = b;
			*list_last = b;

		}
		else { //else error, shouldn't have been passed a NULL benchmark
			ERRPRINTF("Insertion after NULL benchmark in non-empty list.\n");
            return -1;
		}
	}
	else if(after == *list_last) { //if inserting after last element in list
		b->previous = after;
		b->next = (void *)NULL;
	    after->next = b;
		*list_last = b;

	}
	else { //inserted midway through the list
		b->previous = after;
		b->next = after->next;
        after->next->previous = b;
        after->next = b;
	}

	return 0;
}

/*!
 * Remove a benchmark from a sorted benchmark list
 *
 * @param   list_first  pointer to the first bench in the list
 * @param   list_last   pointer to the last bench in the list
 * @param   b   pointer to the benchmark to remove from the list
 *
 * @return 0 on success, -1 on failure
 *
 * @pre b is part of the list
 * @post b is removed from the list but still allocated (having next/previous
 * pointers set to NULL
 */
int
remove_bench_from_sorted_list(struct pmm_benchmark **list_first,
                              struct pmm_benchmark **list_last,
                              struct pmm_benchmark *b)
{

    if(b == *list_first && b == *list_last) { //list only has one point
        if(b->next != NULL || b->previous != NULL) {
            ERRPRINTF("benchmark is both first and last of list, but has "
                      "non-NULL next / previous pointers.\n");
            return -1;
        }

        *list_first = NULL;
        *list_last = NULL;

    }
    else if(b == *list_first) { //b is first in the list
        if(b->next != NULL) {
            ERRPRINTF("benchmark is first in list but has non-NULL next "
                      "pointer.\n");
            return -1;
        }

        *list_first = b->previous;

        if(b->previous != NULL) {
            b->previous->next = NULL;
            b->previous = NULL;
        }
    }
    else if(b == *list_last) { //b is last in list
        if(b->previous != NULL) {
            ERRPRINTF("benchmark is last in list but has non-NULL previous"
                      "pointer.\n");
            return -1;
        }

        *list_last = b->next;

        if(b->next != NULL) {
            b->next->previous = NULL;
            b->next = NULL;
        }
    }
    else { //b is at some point in the middle of the list
        if(b->next == NULL || b->previous == NULL) {
            ERRPRINTF("benchmark not at start / end of list but has NULL next "
                      "/ previous pointers.\n");
            return -1;
        }

        b->next->previous = b->previous;
        b->previous->next = b->next;
        b->next = NULL;
        b->previous = NULL;
    }

    return 0; //success
}

/*!
 * Remove a benchmark from a bench list
 *
 * @param   bl  pointer to the bench list
 * @param   b   pointer to the benchmark to remove from the model
 *
 * @return 0 on success, -1 on failure
 *
 * @pre b is part of the bench list
 * @post b is removed from the list but still allocated (having next/previous
 * pointers set to NULL
 */
int
remove_bench_from_bench_list(struct pmm_bench_list *bl,
                             struct pmm_benchmark *b)
{

    if(remove_bench_from_sorted_list(&(bl->first), &(bl->last), b) < 0) {
        ERRPRINTF("Error removing benchmark from bench list.\n");
        return -1;
    }

    bl->size--;
    bl->parent_model->completion--;

    return 0; //success
}



/*
 * Takes a benchmark structure with flops and size members set and uses them to
 * calculate and set the execution time timeval used_t member of the benchmark
 *
 * @param[in,out] b pointer to benchmark structure
 *
 * @return 0 if benchmark does not have valid data or 1 on success
 */
/*
int
flops_to_time_t(struct pmm_benchmark *b) {
		double d_secs;
		if(b->flops <= 0 || b->size < 0) {
			return 0;
		}

		d_secs = b->size/b->flops;

		b->used_t.tv_sec = floor(d_secs);

		b->used_t.tv_usec = floor(1000*(d_secs - b->used_t.tv_sec));
    return 0;
}
*/


//TODO make some consistency where functions that allocate and return memory do
//TODO so by setting a pointer that is passed into the function as a parameter.
//TODO Functions that do not allocate, but do return pointers do so by simply
//TODO returning the pointer, not by setting a parameter passed into the
//TODO function.
//


#define SEARCH_FORWARDS 0
#define SEARCH_BACKWARDS 1

/*!
 * Searches a sorted bench list for the fastest instance of a benchmark with 
 * matching parameters and returns a pointer to that benchmark
 *
 * @param   start   pointer to start bench to commence search
 * @param   param   the parameter array to search for
 *
 * @return pointer to the fastest benchmark in the list, in place, or NULL on
 * error
 *
 * @pre start must point to a benchmark that is positioned in the sorted list
 * before, or as the very first instance of a benchmark matching the target
 * parameters, otherwise the max returned may not be correct
 *
 */
struct pmm_benchmark*
find_max_bench_in_sorted_bench_list(struct pmm_benchmark *start,
                                     int *param)
{
    int done;
    struct pmm_benchmark *first_match, *last_match, *ret_b, *tmp_b;

    search_sorted_bench_list(SEARCH_FORWARDS, start, param, start->n_p,
                             &first_match, &last_match);

    if(first_match == NULL) { //no match was found
        return NULL;
    }
    else if(last_match == NULL) { //only one match was found
        ret_b = first_match;

        return ret_b;
    }
    else { //multiple matches were found ranging from first to last
        ret_b = first_match;
        tmp_b = first_match->next;

        done = 0;
        while(!done) {
            if(tmp_b->flops > ret_b->flops) {
                ret_b = tmp_b;
            }
            if(tmp_b == last_match) {
                done = 1;
            }
            tmp_b = tmp_b->next;
        }

        return ret_b;
    }
}

/*!
 * Searches a sorted bench list for multiple instances of a benchmark with 
 * matching parameters and returns a newly allocated benchmark that represents 
 * an average of the search hits.
 *
 * @param   start   pointer to start bench to commence search
 * @param   param   the parameter array to search for
 *
 * @return pointer to a newly allocated benchmark that is an average of any
 * benchmarks found in the boundary model with matching parameters or
 * NULL if no benchmark with matching parameters is found
 *
 * @pre start must point to a benchmark that is positioned in the sorted list
 * before, or as the very first instance of a benchmark matching the target
 * parameters, otherwise the average returned will not be accurate
 *
 */
struct pmm_benchmark*
get_avg_bench_from_sorted_bench_list(struct pmm_benchmark *start,
                                     int *param)
{
    struct pmm_benchmark *first_match, *last_match, *ret_b;

    search_sorted_bench_list(SEARCH_FORWARDS, start, param, start->n_p,
                             &first_match, &last_match);

    if(first_match == NULL) { //no match was found
        return NULL;
    }
    else if(last_match == NULL) { //only one match was found
        ret_b = new_benchmark();
        copy_benchmark(ret_b, first_match);

        return ret_b;
    }
    else { //multiple matches were found ranging from first to last
        ret_b = new_benchmark();
        ret_b->n_p = first_match->n_p;

        ret_b->p = init_param_array_copy(param, ret_b->n_p);
        if(ret_b->p == NULL) {
            ERRPRINTF("Error copying parameter array.\n");
            return NULL;
            //TODO this error is not caught as null is not spec'd as error
        }

        ret_b->complexity = first_match->complexity;

        avg_bench_list(first_match, last_match, ret_b);
        return ret_b;
    }
}

/*!
 * Finds the first and last positions of a sequence of benchmarks with matching
 * parameters in a sorted benchmark list
 *
 * @param   bl      pointer to the bench listl to search
 * @param   param   the parameters to search for
 * @param   first   pointer to the first matching benchmark
 * @param   last    pointer to the last matching benchmark
 *
 * @return number of benchmarks found
 *
 * @pre parameter array param has the same number of parameters as the
 * model and the bench list must be sorted
 *
 * @post first and last are NULL if no matching benchmark is found, first is
 * non-NULL and last is NULL if only one matching benchmark is found, finally
 * first and last are non-NULL if a sequence of matching benchmarks are found
 */
int
get_sublist_from_bench_list(struct pmm_bench_list *bl,
                            int *param,
                            struct pmm_benchmark **first,
                            struct pmm_benchmark **last)
{

    return search_sorted_bench_list(SEARCH_FORWARDS, bl->first, param, bl->n_p,
                                    first, last);

}

/*!
 * Finds the first and last positions of a sequence of benchmarks with matching
 * parameters that are part of a sorted benchmark list, search from start
 *
 * @param   direction   use SEARCH_[BACK|FOR]WARDS macros
 * @param   start       pointer to search start point in the list
 * @param   param       pointer to array of parameters to search for
 * @param   n_p         the number of parameters in the param array
 * @param   first       pointer to first matching bench in the model
 * @param   last        pointer to last matching bench in the model
 *
 * @return number of benchmarks found or <0 on error
 *
 * @pre parameter array param has the same number of parameters as the
 * benchmarks in the list and the list is sorted properly
 * @pre benchmark list mst be sorted
 *
 * @post first and last are NULL if no matching benchmark is found, first is
 * non-NULL and last is NULL if only one matching benchmark is found, finally
 * first and last are non-NULL if a sequence of matching benchmarks are found.
 * They do not depend on the search direction, i.e. first and last are not
 * reordered if the search direction is reversed, they will always be presented
 * in the order that occurs in the model.
 * 
 */
int
search_sorted_bench_list(int direction,
                         struct pmm_benchmark *start,
                         int *param,
                         int n_p,
                         struct pmm_benchmark **first,
                         struct pmm_benchmark **last)
{
    struct pmm_benchmark *b;
    int count;

    count = 0;
    *first = NULL;
    *last = NULL;

    if(direction != SEARCH_FORWARDS && direction != SEARCH_BACKWARDS) {
        ERRPRINTF("Direction of search not set correctly.\n");
        return -1;
    }

    b = start;

    switch (direction) {
        case SEARCH_FORWARDS:

            while(b != NULL) {
                if(*first == NULL) {

                    if( params_cmp(b->p, param, n_p) == 0) {
                        *first = b;
                        count++;
                    }
                }
                else { //first match is found

                    //if we still find matching benches, note the position
                    if(params_cmp(b->p, param, n_p) == 0) {
                        *last = b;
                        count++;
                    }
                    else { //else we are finished
                        break;
                    }
                }

                b=b->next;
            }
            
            break;

        case SEARCH_BACKWARDS:

            while(b != NULL) {
                if(*last == NULL) {

                    if( params_cmp(b->p, param, n_p) == 0) {
                        *last = b;
                        count++;
                    }
                }
                else { //first match is found

                    //if we still find matching benches, note the position
                    if(params_cmp(b->p, param, n_p) == 0) {
                        *first = b;
                        count++;
                    }
                    else { //else we are finished

                        // if we only found one bench first will not be
                        // set to anything. The function is specified to
                        // return the list segment without search direction
                        // specificity ... so swap first and last
                        if(*first == NULL) {
                            *first = *last;
                            *last = NULL;
                        }

                        break;
                    }
                }

                b=b->previous;
            }
            
            break;

        default:
            break;
            //never
    }


    return count;
}

/*!
 * find the next benchmark in a sorted list that does not have identical
 * parameters to b. As models may have more than one benchmark for the same
 * point, looking at the ->next element of a benchmark may give us a second
 * benchmark at the exact same point. Hence the need for this function
 *
 * @param   b       pointer to a benchmark that is in a sorted list
 * 
 * @return pointer to the next benchmark in the list (that b belongs to), which
 * has a different set of parameters associated with it. This may be null if
 * the benchmark b is the last benchmark in the list
 */
struct pmm_benchmark*
get_next_different_bench(struct pmm_benchmark *b)
{
    struct pmm_benchmark *next;
    struct pmm_benchmark *first, *last;

    //search the bench list for all benchmarks with params of b, starting
    //the search at b, using the result we know where the next not matching
    //bench will be
    search_sorted_bench_list(SEARCH_FORWARDS, b, b->p, b->n_p, &first, &last);
    
    if(last == NULL) { //there is only 1 matching benchmark in the list 

        if(first != NULL) {
            next = first->next;
        }
        else { //this really should never happen
            ERRPRINTF("Could not find b when searching itself.\n");
            print_benchmark(PMM_ERR, b);
            exit(EXIT_FAILURE);
            next = NULL;
        }
    }
    else {
        next = last->next;
    }


    return next;
}

/*!
 * find the previous benchmark in a sorted list that does not have identical
 * parameters to b. As models may have more than one benchmark for the same
 * point, looking at the ->previous element of a benchmark may give us a second
 * benchmark at the exact same point. Hence the need for this function
 *
 * @param   b       pointer to a benchmark that is in a sorted list
 * 
 * @return pointer to the previous benchmark in the list (that b belongs to),
 * which has a different set of parameters associated with it. This may be NULL
 * if the benchmark b is the first benchmark in the list.
 */
struct pmm_benchmark*
get_previous_different_bench(struct pmm_benchmark *b)
{
    struct pmm_benchmark *previous;
    struct pmm_benchmark *first, *last;

    //search the bench list for all benchmarks with params of b, starting
    //the search at b for expediency and looking backwards
    search_sorted_bench_list(SEARCH_BACKWARDS, b, b->p, b->n_p, &first, &last);
    
    //previous different benchmark to b will be before the first in the list of
    //benchmarks matching b
    if(first != NULL) {
        previous = first->previous;
    }
    else { //this should never happen
        ERRPRINTF("Could not find b when searching itself!\n");
        print_benchmark(PMM_ERR, b);
        exit(EXIT_FAILURE);
        previous = NULL;
    }

    return previous;
}
/*!
 * Averages the flops and execution times of a sequence of benchmarks and
 * copies this information into a benchmark structure pointed to by b
 *
 * @param   start   pointer to the first benchmark in the sequence
 * @param   end     pointer to the last benchmark in the sequence
 * @param   avg_b   pointer to the benchmark where the average is to be stored
 *
 * @pre benchmarks in the sequence defined by start & end are all at the same
 * point, i.e. the values in their p array are identical
 * @pre b is a non-NULL allocated structure
 */
void
avg_bench_list(struct pmm_benchmark *start, struct pmm_benchmark *end,
                   struct pmm_benchmark *avg_b)
{
    double divisor = 0.0;

    zero_benchmark(avg_b); //zero ret_b benchmark data

    divisor = (double)sum_bench_list(start, end, avg_b);

    div_bench(avg_b, divisor, avg_b);

    return;
}

/*!
 * Summates the data of benchmarks in a list marked by start and end into
 * an empty benchmark sum
 *
 * @param   start   pointer to the first benchmark in the list of benchmarks to
 *                  be summed
 * @param   end     pointer to the last benchmark in thet list of benchmarks to
 *                  be summed
 * @param   sum     pointer to the benchmark where the summed data is to
 *                  be stored
 *
 * @return the number of benchmarks added to the sum as an integer
 *
 * @pre all benchmarks are allocated and non NULL. sum is initialized with
 * zero data values
 */
int
sum_bench_list(struct pmm_benchmark *start, struct pmm_benchmark *end,
               struct pmm_benchmark *sum)
{
    struct pmm_benchmark *this;
    int counter = 0;
    int done = 0;

    this = start;

    while(!done) {

        add_bench(sum, this, sum);

        counter++;

        if(this == end) {
            done = 1;
        }
        this = this->next;
    }

    return counter;
}

/*!
 * Adds performance data of one benchmark to another (a+b) storing result in
 * res. Parameters added are: flops, seconds, wall_t, used_t.
 *
 * @param   a       pointer to first benchmark to add
 * @param   b       pointer to second benchmark to add
 * @param   res     pointer to benchmark where result of addition is stored
 *
 * @pre all benchmarks are non-NULL
 * @pre all benchmarks have non-performance data identical (param array, etc).
 */
void
add_bench(struct pmm_benchmark *a, struct pmm_benchmark *b,
          struct pmm_benchmark *res)
{
    res->flops = a->flops + b->flops;
    res->seconds = a->seconds + b->seconds;

    timeval_add(&(a->used_t), &(b->used_t), &(res->used_t));
    timeval_add(&(a->wall_t), &(b->wall_t), &(res->wall_t));

    return;
}


/*!
 * Divides a the performance data fields of a benchmark by some float. Relevant
 * parameters are flops, seconds, wall_t and used_t.
 *
 * @param   b       pointer to the benchmark to be divided
 * @param   d       the devisor as a double
 * @param   res     pointer to the location to store the result
 *
 * @pre all benchmarks are non-NULL
 * @pre divisor is non-zero and positive
 * @pre all benchmarks have identical non-performance data (param array, etc).
 */
void
div_bench(struct pmm_benchmark *b, double d, struct pmm_benchmark *res)
{

    if(d == 0.0) {
        ERRPRINTF("Divide by zero error.\n");
        return;
    }
    else if(d < 0.0) {
        ERRPRINTF("Divide by negative unsupported.\n");
        return;
    }
    else if(d == 1.0) {
        return; // nothing to do
    }

    res->flops = b->flops/d;
    res->seconds = b->seconds/d;

    //only copy the timeval if the result is not self assigned
    //(i.e. when it is not not b = b/d)
    if(b != res) {
        //copy from b -> to res
        copy_timeval(&(res->wall_t), &(b->wall_t));
        copy_timeval(&(res->used_t), &(b->used_t));
    }

    timeval_div(&(res->wall_t), d);
    timeval_div(&(res->used_t), d);

    return;
}

/*!
 * Copies timeval structures
 *
 * @param   dst pointer to the timeval to copy to
 * @param   src pointer to the timeval to copy from
 *
 * @pre src and dst are non-NULL
 *
 */
void
copy_timeval(struct timeval *dst, struct timeval *src)
{
	dst->tv_sec = src->tv_sec;
	dst->tv_usec = src->tv_usec;
}

/*!
 * Converts timeval to a double in seconds units
 *
 * @param   tv  pointer to a timeval structure
 *
 * @return value of the timeval in seconds as a double
 */
double
timeval_to_double(struct timeval *tv)
{
    return tv->tv_sec + tv->tv_usec/1000000.0;
}

/*!
 *
 * Converts a double to a timeval
 *
 * @param   d   double value to convert from
 * @param   tv  pointer to timeval
 */
void
double_to_timeval(double d, struct timeval *tv)
{
    tv->tv_sec = (int)d;
    tv->tv_usec = (int)(100000.0* (d - tv->tv_sec));

    return;
}

/*!
 * Convert a double to a timespec
 *
 * @param   d   double value to convert
 * @param   ts  pointer to timespec
 */
void
double_to_timespec(double d, struct timespec *ts)
{
    ts->tv_sec = (int)d;
    ts->tv_nsec = (long)(1000000000.0 * (d - ts->tv_sec));

    return;
}

/*!
 * Add two timevals, to another, tv_res = tv_a + tv_b
 *
 * @param   tv_a    first timeval to add
 * @param   tv_b    second timeval to add
 * @param   tv_res    pointer to timeval to store result
 */
void
timeval_add(struct timeval *tv_a, struct timeval *tv_b, struct timeval *tv_res)
{
    tv_res->tv_sec = tv_a->tv_sec + tv_b->tv_sec;
    tv_res->tv_usec = tv_a->tv_usec + tv_b->tv_usec;
    if(tv_res->tv_usec >= 1000000) {
        tv_res->tv_sec++;
        tv_res->tv_usec -= 1000000;
    }

    return;
}

/*!
 * Divide timeval by a double
 *
 * @param   tv  the timeval
 * @param   d   the divisor as a double
 *
 * @pre divisor should be non zero
 */
void
timeval_div(struct timeval *tv, double d)
{
    double result_d;

    if(d == 0) {
        ERRPRINTF("Divide by zero error.\n");
        return;
    }
    if(d == 1.0) {
        return;
    }

    result_d = timeval_to_double(tv) / d;

    double_to_timeval(result_d, tv);
}

/*!
 * Compare two timevals
 *
 * @param   a   pointer to first timeval
 * @param   b   pointer to second timeval
 *
 * @return  -1 if a < b, 0 if a = b and 1 if a > b
 */
int
timeval_cmp(struct timeval *a, struct timeval *b)
{
    if (a->tv_sec < b->tv_sec) {
        return -1;
    }
    else if (a->tv_sec > b->tv_sec) {
        return 1;
    }
    else if (a->tv_usec < b->tv_usec) { // tv_sec equal now compare tv_usec
        return -1;
    }
    else if (a->tv_usec > b->tv_usec) {
        return 1;
    }

    return 0;
}

/*!
 * Searches a benchmark list for the first occurance of a benchmark and returns
 * a pointer to that benchmark or NULL if no benchmark is found
 *
 * @param   bl      bench list to search
 * @param   p       the parameter array to search for
 *
 * @return pointer to the first occurance of a benchmark with matching
 * parameters or NULL if no such benchmark is found
 *
 * @pre the number of parameters of the param array and the list model are equal
 *
 */
struct pmm_benchmark*
get_first_bench_from_bench_list(struct pmm_bench_list *bl,
		                        int *p)
{
	struct pmm_benchmark *b;

	b = bl->first;

	while(b != NULL) {
		if(params_cmp(b->p, p, bl->n_p == 0)) {
			return b;
		}
		b = b->next;
	}

	return b; //b should be NULL at this point

}

/*!
 * test if the data elements of benchmarks are equal (i.e. everything but the
 * pointers)
 *
 * @param   b1  pointer to first benchmark
 * @param   b2  pointer to second benchmark
 *
 * @return 0 if benchmarks are not equal, 1 if benchmarks are equal
 */
int
isequal_benchmarks(struct pmm_benchmark *b1, struct pmm_benchmark *b2)
{
    if(b1 == NULL || b2 == NULL) {
        return 0;
    }

    if(b1->n_p != b2->n_p) {
        return 0;
    }

    if(params_cmp(b1->p, b2->p, b1->n_p) != 0) { //if params are not equal
        return 0;
    }

    if(b1->complexity != b2->complexity ||
       b1->flops != b2->flops ||
       b1->seconds != b2->seconds ||
       timeval_cmp(&(b1->wall_t), &(b2->wall_t)) != 0 ||
       timeval_cmp(&(b1->used_t), &(b2->used_t)) != 0)
    {
        return 0;
    }

    return 1;
}

/*!
 * Returns pointer to the first instance of benchmark from the model that has
 * the same parameters as those requested, or null if no benchmark exists with
 * matching parameters
 *
 * @param   m       pointer to the model
 * @param   p       pointer to the target parameter array
 *
 * @return pointer to the first instance of a benchmark with parameters matching
 * the param array
 *
 * @pre the parameter array has the same size as the number of parameters the
 * model has been built in terms of
 */
struct pmm_benchmark*
get_first_bench(struct pmm_model *m, int *p)
{

	struct pmm_benchmark *b;

	b = (void *)NULL;

    b = get_first_bench_from_bench_list(m->bench_list, p);

	return b;
}


/*!
 * Returns pointer to the a newly allocated benchmark that represents the
 * average of any benchmarks found in the model with parameters matching
 * an aligned version of those described by param, or null if no benchmark
 * exists with matching parameters
 *
 * @param   m       pointer to the model
 * @param   param   pointer to the first element of the target parameter array
 *
 * @return pointer to a newly allocated benchmark that is an average of all
 * benchmarks with matching aligned parameters or NULL on error
 *
 * @pre the parameter array has the same size as the number of parameters the
 * model has been built in terms of
 */
struct pmm_benchmark*
get_avg_aligned_bench(struct pmm_model *m, int *param)
{
    int *p_aligned;
    struct pmm_benchmark *b;

    b = (void*)NULL;

    p_aligned = init_param_array_copy(param, m->n_p);
    if(p_aligned == NULL) {
        ERRPRINTF("Error copying param array.\n");
        return NULL;
    }

    align_params(p_aligned, m->parent_routine->pd_set);

    b = get_avg_bench(m, p_aligned);

    free(p_aligned);
    p_aligned = NULL;

    return b;
}

/*!
 * Returns pointer to the a newly allocated benchmark that represents the
 * average of any benchmarks found in the model that have the same parameters
 * as those described by param, or null if no benchmark exists with
 * matching parameters
 *
 * @param   m       pointer to the model
 * @param   p       pointer to the target parameter array
 *
 * @return pointer to a newly allocated benchmark that is an average of all
 * benchmarks with matching parameters
 *
 * @pre the parameter array has the same size as the number of parameters the
 * model has been built in terms of
 */
struct pmm_benchmark*
get_avg_bench(struct pmm_model *m, int *p)
{
	struct pmm_benchmark *b;

	b = (void *)NULL;

    b = get_avg_bench_from_sorted_bench_list(m->bench_list->first, p);

	return b;

}

/*!
 * Calculate some statistics about the benchmarking of a particular point in the
 * model, as described by the param parameter array. (Each point stored in a
 * model may have multiple benchmarks, essentially, the sum of the time used by
 * these benchmarks and the number of benchmarks are calculated.
 *
 * @param   m           pointer to the model
 * @param   param       pointer to an array of parameter values that
 * @param   time_spent  pointer to double where the time spent is stored
 * @param   num_execs   pointer to int where number of executions is stored
 *
 * @return double that is the number of secounds spent benchmarking the point
 * described by param
 */
void
calc_bench_exec_stats(struct pmm_model *m, int *param,
                      double *time_spent, int *num_execs)
{
	struct pmm_benchmark *b;
    struct pmm_benchmark *first, *last;

    *num_execs = 0;
    first = NULL;
    last = NULL;

    //allocate bench and zero ... don't care about copying param array
    b = new_benchmark();
    zero_benchmark(b);


    // find a sublist of matching benchmarks
    get_sublist_from_bench_list(m->bench_list, param, &first, &last);

    if(first == NULL) {
        // nothing found, leave b empty
    }
    else if(last == NULL) {
        // only one bench found in boundary (only need the wall_t)
        copy_timeval(&(b->wall_t), &(first->wall_t));
        *num_execs = 1;
    }
    else {
        // sum the benchmarks found between first and last
        *num_execs = sum_bench_list(first, last, b);
    }

    *time_spent = timeval_to_double(&(b->wall_t));

    free_benchmark(&b);

    return;
}

/*!
 * Calculate some basic statistics about a model
 *
 * @param   m   pointer to the model
 *
 * @return time spend executing benchmarks in model
 */
double
calc_model_stats(struct pmm_model *m)
{
    struct pmm_benchmark *b;
    double total_time;

    b = m->bench_list->first;

    total_time = 0.0;
    while(b != NULL) {
        total_time += timeval_to_double(&(b->wall_t));

        b = b->next;
    }

    return total_time;
}

/*!
 * Function removes benchmarks from a model that have parameters that match
 * a target set. Removed benchmarks are all not deallocated, their addresses
 * are stored in an array passed to the function as removed_array
 *
 * @param   m               pointer to the model
 * @param   p               pointer to an array of parameters
 * @param   removed_array   pointer to unallocated array of benchmarks
 *
 * @return the number of benchmarks found and removed from the model or -1 on
 * error
 *
 * @pre removed_array is unallocated
 * @post removed_array is allocated with a size matching the number of
 * benchmarks found and removed
 */
int
remove_benchmarks_at_param(struct pmm_model *m, int *p,
                           struct pmm_benchmark ***removed_array)
{
    int n, c;

    struct pmm_benchmark *first, *last, *this, *temp;


    n = get_sublist_from_bench_list(m->bench_list, p, &first, &last);

    if(n == 0) {
        return 0; //nothing found
    }

    DBGPRINTF("allocating remove_array of %d elemenets of size %lu.\n",
              n, sizeof(*removed_array));

    (*removed_array) = malloc(n * sizeof *(*removed_array));
    if((*removed_array) == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1;
    }

    this = first;
    c = 0;
    while(c < n) {
        DBGPRINTF("removing %p temporarily\n", this);
        print_benchmark(PMM_DBG, this);

        // store the next as this will be unplugged by remove_bench_...
        temp = this->next;

        //remove benchmarks between first/last from model
        remove_bench_from_bench_list(m->bench_list, this);


        DBGPRINTF("saving %p at position %d in removed_array %p\n",
                this, c, (*removed_array));

        //add removed to array
        (*removed_array)[c] = this;
        c++;

        this = temp; //next
    }


    return n;
}

/*!
 * find_oldapprox will find the approximation of performance at a target point
 * in a model, ignoring any data points that happen to be at that target point.
 * The main use of this function is to find what the approximation of a model
 * was before a data point was added to the model.
 *
 * @param   m   pointer to the model
 * @param   p   pointer to a parameter array describing the target point
 *
 * @return a pointer to a newly allocated benchmark that gives an approximation
 * of the model at the target point, ignoring any data at the exact point or
 * NULL on error
 */
struct pmm_benchmark *
find_oldapprox(struct pmm_model *m, int *p)
{
    struct pmm_benchmark *b;
    struct pmm_benchmark **removed_benchmarks_array; //array of pointers
    int i;
    int n_removed;

    // remove benchmarks from model with matching parameter to p
    n_removed = remove_benchmarks_at_param(m, p, &removed_benchmarks_array);


    //lookup version of model with benchmarks removed
    b = lookup_model(m, p);
    if(b == NULL) {
        ERRPRINTF("Error looking up model.\n");
    }

    //re-insert removed benchmarks back into model
    for(i=0; i<n_removed; i++) {
        insert_bench(m, removed_benchmarks_array[i]);
    }

    //return approximation b
    return b;
}

/*!
 * Call the appropriate lookup function to find value of model
 * given a set of parameters
 *
 * @param   m       pointer to model
 * @param   p       array of parameters
 *
 * @return  newly allocated benchmark describing performance at
 * p or NULL on failure
 */
struct pmm_benchmark* lookup_model(struct pmm_model *m, int *p)
{
	struct pmm_benchmark *b;

	b = NULL;

    if(m->n_p == 1) {
        b = interpolate_1d_model(m->bench_list, p);
    }
    else {
#ifdef ENABLE_OCTAVE
        // n-dimensional interpolation within boundaries via octave
		b = interpolate_griddatan(m, p);
#else
        ERRPRINTF("Cannot interpolate 2D+ models without octave support "
                  "compiled.\n");
        return NULL;
#endif /* ENABLE_OCTAVE */
    }
		
	return b;
}

/*!
 * Find the speed approximation given by a 1-D or single parameter model at
 * a point described by the parameter array p (size of 1!).
 *
 * @param   bl      pointer to the bench list
 * @param   p       pointer to parameter array
 *
 * @return pointer to a newly allocated benchmark structure which describes
 * the flops performance at the point p.
 * 
 *
 * TODO this does not use the average of benchmarks with the same parameters
 */
struct pmm_benchmark*
interpolate_1d_model(struct pmm_bench_list *bl,
                     int *p)
{
    struct pmm_benchmark *b, *cur;

    if(bl->n_p != 1) {
        ERRPRINTF("1d interpolation cannot use %dd data.\n", bl->n_p);
        return NULL;
    }

    b = new_benchmark();
    b->n_p = bl->n_p;

    b->p = init_param_array_copy(p, b->n_p);
    if(b->p == NULL) {
        ERRPRINTF("Error copying parameter array\n");
        return NULL;
        //TODO this error is not caught as NULL is spec'd as OK return
    }


    cur = bl->first;

    while(cur) {
        if(cur->p[0] < p[0]) {
            // if model parameter is less than target skip to next bench in
            // model
        }
        else if(cur->p[0] == p[0]) {
            // we got lucky
            b->flops = cur->flops;

            DBGPRINTF("benchmark at interpolation point: %f\n", b->flops);
            return b;
        }
        else if(cur->p[0] > p[0]) {
            // our search has passed the target, we can now interpolate between
            // cur (greater than target) and cur->previous (less than target)

            if(cur->previous != NULL) {
                //let's interpolate!
				//
				// the formula to calcuate the flops is as follows:
				//
				// x1,y1 are p[0] and flops of previous
				// x2,y2 are p[0] and flops of cur
				// xb,yb are p[0] and flops of b;
				//
                //
                // m = (y2 - y1)/(x2 - x1) (formula for slope)
                //
                // y - y1 = m(x - x1) (formula for a line)
                //
                // yb = m(xb - x1) + y1
                //
				{
					int x1, x2, xb;
					double y1, y2, yb;

					double m;

					x1=cur->p[0];
					x2=cur->previous->p[0];
					xb=b->p[0];

					y1=cur->flops;
					y2=cur->previous->flops;

					m = ((double)(y2-y1))/((double)(x2-x1));

					yb = m*((double)(xb-x1)) + (double)y1;

					b->flops = yb;

                    DBGPRINTF("interploated between %d and %d to %f\n",
                             x1, x2, b->flops);
                    return b;


				}
			}
            else {
                //cur has no preivous, i.e. it is the first benchmark in the
                //list model. We will assume speed for a smaller size is
                //equal to the first benchmark.
				b->flops = cur->flops;
                DBGPRINTF("< than first bench, interpolated to:%f\n", b->flops);
                return b;
            }
        }
        cur = cur->next;
    }

    //we have searched through the model and not found a benchmark
    //greater than the target, at minimum, the model will have a zero
    //speed benchmark at paramdef.end so we can assume that the target is
    //greater than this and speed is zero
    //
    //TODO this may not be best if nonzero end is set and the end bench is
    //not actually 0 speed.
    //

    b->flops = .0;
    return b;
}


#define MAX_CMP(x,y) (x) > (y) ? (x) : (y)
#define MIN_CMP(x,y) (x) < (y) ? (x) : (y)
#define PMM_PERC 0.05 // percentage threshold for GBBP

/*!
 * 
 * This function determines if the 'cut' of the model at a specific benchmarked
 * point, b1, contains the range of execution speeds that are defined by the cut
 * of the model at a second benchmark point, b2.
 *
 * The cut is defined as range of speeds between the maximum and minimum speeds
 * as predicted by the model. If C_b1 contains C_b2, then b1_max > b2_max and
 * b1_min < b2_min. As illustrated by the diagram below.
 *
 * i.e.
 * \verbatim
 *  |           b1_max
 * s|           +               b2_max
 * p|        ...|...............+....
 * e|   cut@b1- |               | -cut@b2
 * e|           |               |
 * d|        ...|...............+....
 *  |           +               b2_min
 *  |           b1_min
 * -+------------------------------------------
 *  |           ^     size      ^
 *              b1              b2
 * \endverbatim
 *
 * @param   h   pointer to the load history
 * @param   b1  pointer to first benchmark point
 * @param   b2  pointer to second benchmark point
 *
 * @returns 1 if the values of the model at b1 contain the values of
 * the model at b2, 0 otherwise
 */
int bench_cut_contains(struct pmm_loadhistory *h, struct pmm_benchmark *b1,
		               struct pmm_benchmark *b2) {

    //for now we just test if b1->flops and b2->flops are within a percent
    //of eachother
    //

    double max, min;

    (void)h; //TODO unused

    DBGPRINTF("b1->flops:%f b2->flops:%f\n", b1->flops, b2->flops);

    max = MAX_CMP(b1->flops, b2->flops);
    min = MIN_CMP(b1->flops, b2->flops);

    DBGPRINTF("max:%f min:%f\n", max, min);

    DBGPRINTF("max/min %f min/max %f\n", max/min, min/max);


    if(max/min <= 1+PMM_PERC && min/max >= 1-PMM_PERC) {
        DBGPRINTF("b1 contains b2\n");
        return 1; //true
    }
    else {
        DBGPRINTF("b1 does not contain b2\n");
        return 0; //false
    }

    //TODO implement bench_cut_contains properly (i.e. using loadhistory)
    //take t0, the optimal execution time of the benchmark

	//using the load history band caculate the upper and lower execution times
	//of the benchmark

	//convert execution times into speed of computation (divide by size)

	//compare upper and lower execution speeds
}

/*!
 * This function determines if the 'cut' of the model at a specific benchmarked
 * point, b1, overlaps the range of execution speeds that are defined by the cut
 * of the model at a second benchmark point, b2.
 *
 * The cut is defined as range of speeds between the maximum and minimum speeds
 * as predicted by the model. If C_b1 intersects C_b2, then b1_max > b2_max and
 * b1_min < b2_max or, b1_max > b2_min and b1_min < b2_min. As illustrated by
 * the diagram below.
 *
 * \verbatim
 * i.e.
 *  |           b1_max
 * s|           +
 * p|           |
 * e|   cut@b1- |               b2_max
 * e|        ...|...............+...
 * d|           |               | -cut@b2
 *  |           +               |
 *  |           b1_min     .....+...
 *  |                           b2_min
 * -+------------------------------------------
 *  |           ^     size      ^
 *              b1              b2
 * \endverbatim
 *
 * @param   h   pointer to the load history
 * @param   b1  pointer to first benchmark point
 * @param   b2  pointer to second benchmark point
 *
 * @return 1 if values of model at b1 intersect values of model at b2, 0
 * otherwise
 */
int bench_cut_intersects(struct pmm_loadhistory *h, struct pmm_benchmark *b1,
		                 struct pmm_benchmark *b2) {
    //for now we just test if b1->flops and b2->flops are within a percent
    //of eachother ... same as 'bench_cut_contains'

    double max, min;

    (void)h; //TODO unused

    DBGPRINTF("b1->flops:%f b2->flops:%f\n", b1->flops, b2->flops);

    max = MAX_CMP(b1->flops, b2->flops);
    min = MIN_CMP(b1->flops, b2->flops);

    DBGPRINTF("max:%f min:%f\n", max, min);

    DBGPRINTF("max/min %f min/max %f\n", max/min, min/max);

    if(max/min <= 1+PMM_PERC && min/max >= 1-PMM_PERC) {
        DBGPRINTF("b1 intersects b2\n");
        return 1; //true
    }
    else {
        DBGPRINTF("b1 does not intersects b2\n");
        return 0; //false
    }

    //TODO implement bench_cut_intersects properly (i.e. using loadhistory)
}

/*!
 * This function determines if the 'cut' of the model at a specific benchmarked
 * point, b1, has no overlap on the range of execution speeds that are defined
 * o by the cut of the model at a second benchmark point, b2. And further,
 * that the cut of b1 lies higher
 *
 * The cut is defined as range of speeds between the maximum and minimum speeds
 * as predicted by the model. If C_b1 is greater than C_b2, then b1_min > b2_max
 * . As illustrated by the diagram below.
 *
 * \verbatim
 * i.e.
 *  |           b1_max
 * s|           +
 * p|           |
 * e|   cut@b1- |
 * e|        ...|
 * d|           |
 *  |           +
 *  |           b1_min          b2_max
 *  |                     ......+...
 *  |                           | -cut@b2
 *  |                     ......+...
 *  |                           b2_min
 * -+------------------------------------------
 *  |           ^     size      ^
 *              b1              b2
 * \endverbatim
 * @param   h   pointer to the load history
 * @param   b1  pointer to first benchmark point
 * @param   b2  pointer to second benchmark point
 *
 * @returns 1 if the values of the model at b1 are greater than the values of
 * the model at b2, 0 otherwise
 */
int bench_cut_greater(struct pmm_loadhistory *h, struct pmm_benchmark *b1,
		              struct pmm_benchmark *b2) {

    (void)h; //TODO unused

    if(b1->flops > b2->flops*(1.0 + PMM_PERC)) {
        return 1; //b1 greater than b2 (by a percentage)
    }
    else {
        return 0; //false
    }
    //TODO implement bench_cut_greater properly (i.e. using loadhistory)
}

/*!
 * This function performs the inverse of of bench_cut_greater
 *
 * @param   h   pointer to the load history
 * @param   b1  pointer to first benchmark point
 * @param   b2  pointer to second benchmark point
 *
 * @returns 1 if the values of the model at b1 are less than the values of
 * the model at b2, 0 otherwise
 */
int bench_cut_less(struct pmm_loadhistory *h, struct pmm_benchmark *b1,
		           struct pmm_benchmark *b2) {

    (void)h; //TODO unused

    if(b1->flops < b2->flops*(1.0 - PMM_PERC)) {
        return 1; //b1 less than b2 (by a percentage)
    }
    else {
        return 0; //false
    }
    //TODO implement bench_cut_less properly (i.e. using loadhistory)
}

/*!
 * convert a construction method enum to a char array description
 *
 * @param   method  the construction method
 * 
 * @returns pointer to a character array describing the method
 */
char*
construction_method_to_string(enum pmm_construction_method method)
{
    switch (method) {
        case CM_NAIVE:
            return "naive";
        case CM_NAIVE_BISECT:
            return "naive_bisect";
        case CM_RAND:
            return "random";
        case CM_GBBP:
            return "gbbp";
        case CM_GBBP_NAIVE:
            return "gbbp_naive";
        case CM_INVALID:
            return "invalid";
        default:
            return "unknown";
    }
}

/*!
 * convert a construction condition enum to a char array description
 *
 * @param   conditon    the construction method
 * 
 * @returns pointer to a character array describing the condition
 */
char*
construction_condition_to_string(enum pmm_construction_condition condition)
{
    switch (condition) {
        case CC_INVALID:
            return "invalid";
        case CC_NOW:
            return "now";
        case CC_UNTIL:
            return "until";
        case CC_PERIODIC:
            return "periodic";
        case CC_IDLE:
            return "idle";
        case CC_NOUSERS:
            return "no users";
        default:
            return "unknown";
    }
}

/*
 * Test if 3 points (parameters of a model) are collinear (in n dimensions)
 *
 * For example, in 3 dimensions, points: \f$P_1, P_2, P_3\f$, defined by
 * \f$(x_1,y_2), (x_2,y_2), (x_3,y_3)\f$ respectively, are collinear if 
 * and only if:
 * 
 * \f$(x_2-x_1)/(x_3-x_1)=(y_2-y_1)/(y_3-y_1)=(z_2-z_1)/(z_3-z_1)\f$
 *
 * That is, the ratio of the distance from \f$P_1\f$ to \f$P_2\f$  and from
 * \f$P_1\f$ to \f$P_3\f$ in each seperate dimension, must be equal. This
 * is trivially expanded to N dimensions.
 *
 * Expanded to \f$n\f$ dimensions, where \f$P_i\f$ is defined by
 * \f$(a_i0, a_i1, ... a_in)\f$, and the ratio of the distance between points
 * in the \f$n^{th}\f$-dimensial plane is given by:
 *
 * \f$q_n = (a_{2n}-a_{2n})/(a_{3n}-a_{1n})\f$
 *
 * Points \f$P_0, P_1, P_2\f$ are collinear if for all \f$n\f$:
 *
 * \f$q_n = q_0\f$
 *
 *
 * @param   a   pointer to a parameter array
 * @param   b   pointer to a parameter array
 * @param   c   pointer to a parameter array
 * @param   n   number of elements in the arrays
 *
 * @return 0 if points are not collinear, 1 if points are collinear, -1 on error
 *
 * @pre all parameter arrays have the same number of elements
 */
/* TODO
int
is_collinear_params(int *a, int *b, int *c, int n)
{
    int *d1, *d2, *c;

    d1 = malloc

    //find difference between b -> a, and c -> a
    for(i=0; i<n; i++) {
        d1[i] = b[i] - a[i];
        d2[i] = c[i] - a[i];
    }


}
*/

/*!
 * prints the linked list of benchmark points in a pmm_model
 *
 * @param   output      output stream to print to
 * @param   m           model to print
 */
void print_model(const char *output, struct pmm_model *m) {
    char mtime_str[80];
    struct tm *ts;

	SWITCHPRINTF(output, "path: %s\n", m->model_path);
	SWITCHPRINTF(output, "unwritten execs: %d\n", m->unwritten_num_execs);
    SWITCHPRINTF(output, "unwritten time: %f\n", m->unwritten_time_spend);

    ts = localtime(&(m->mtime));
    strftime(mtime_str, sizeof(mtime_str), "%a %Y-%m-%d %H:%M:%S %Z", ts);
    SWITCHPRINTF(output, "modified time: %s\n", mtime_str);

    free(ts);
    ts = NULL;

	SWITCHPRINTF(output, "completion:%d\n", m->completion);
    SWITCHPRINTF(output, "uniqe benches:%d\n", m->unique_benches);

	SWITCHPRINTF(output, "--- bench list ---\n");
	print_bench_list(output, m->bench_list);
	SWITCHPRINTF(output, "--- end bench list ---\n");

	SWITCHPRINTF(output, "--- interval list ---\n");
	print_interval_list(output, m->interval_list);
	SWITCHPRINTF(output, "--- end interval list ---\n");


}

/*!
 * Print a pmm_bench_list structure to the log
 *
 * @param   bl  pointer to the pmm_bench_list structure
 */
void
print_bench_list(const char *output, struct pmm_bench_list *bl)
{
	struct pmm_benchmark *b;

	SWITCHPRINTF(output, "size: %d\n", bl->size);
	SWITCHPRINTF(output, "n_p: %d\n", bl->n_p);

	SWITCHPRINTF(output, "first: %p\n", bl->first);
	SWITCHPRINTF(output, "last: %p\n", bl->last);

	SWITCHPRINTF(output, "--- begin benchmarks ---\n");

	b = bl->first;

	while(b != NULL) {
		print_benchmark(output, b);
		b = b->next;
	}

	SWITCHPRINTF(output, "--- end benchmarks ---\n");

	return;
}

/*!
 * print a benchmark
 *
 * @param   output      output stream to print to
 * @param   b           pointer to benchmark
 */
void
print_benchmark(const char *output, struct pmm_benchmark *b) {
	int i;

    SWITCHPRINTF(output, "----benchmark----\n");
	for(i=0; i<b->n_p; i++) {
		SWITCHPRINTF(output, "p[%d]: %d\n", i, b->p[i]);
	}
	SWITCHPRINTF(output, "flops: %f\n", b->flops);
	SWITCHPRINTF(output, "seconds: %f\n", b->seconds);
	SWITCHPRINTF(output, "wall sec:%ld wall usec:%ld\n", b->wall_t.tv_sec,
			     b->wall_t.tv_usec);
	SWITCHPRINTF(output, "used sec:%ld used usec:%ld\n", b->used_t.tv_sec,
			     b->used_t.tv_usec);

	SWITCHPRINTF(output, "previous:%p\n", b->previous);

	SWITCHPRINTF(output, "current:%p\n", b);

	SWITCHPRINTF(output, "next:%p\n", b->next);
    SWITCHPRINTF(output, "--end-benchmark--\n");
}

/*!
 * prints the elements of a pmm_routine structure 
 *
 * @param   output      output stream to print to
 * @param   r           pointer to routine
 */
void print_routine(const char *output, struct pmm_routine *r) {

	SWITCHPRINTF(output, "-- rountine --\n");
	SWITCHPRINTF(output, "name: %s\n", r->name);
	SWITCHPRINTF(output, "exe_path: %s\n", r->exe_path);

    if(r->exe_args != NULL)
        SWITCHPRINTF(output, "exe_args: %s\n", r->exe_args);

    print_paramdef_set(output, r->pd_set);

	SWITCHPRINTF(output, "condition: %s\n", construction_condition_to_string(r->condition));
	SWITCHPRINTF(output, "priority:%d\n", r->priority);
    SWITCHPRINTF(output, "executable:%d\n", r->executable);

    SWITCHPRINTF(output, "min_sample_num:%d\n", r->min_sample_num);
    SWITCHPRINTF(output, "min_sample_time:%d\n", r->min_sample_time);
    SWITCHPRINTF(output, "max_completion:%d\n", r->max_completion);
    SWITCHPRINTF(output, "construction method: %s\n",
                 construction_method_to_string(r->construction_method));

	//print_model(output, r->model);
	SWITCHPRINTF(output, "model completion:%d\n", r->model->completion);

	SWITCHPRINTF(output, "-- end routine --\n");
	return;
}

/*!
 * function is used  to set string variables of the pmm_routine structure
 * and other string variables. Takes a pointer to the destination char*
 * and the source char* and then allocates enough memory to copy
 * the source into the destination.
 *
 * @param   dst     pointer to location fo destination string
 * @param   src     pointer to string to set
 *
 * @return 0 on failure, 1 on success
 *
 */
int set_str(char **dst, char *src) {
	size_t len;

    /*
     * TODO consider setting a routine string that has already some memory
     * allocated (from a previous set, i.e. resetting str ...
     *
     * or in english, FREE/malloc or realloc dst before copying src, duh!
     */

	if(src == NULL) {
		ERRPRINTF("null source string\n");
		return 0; //failure
	}

	len = strlen(src) + 1;

	// LOGPRINTF("len:%d\n", len);

	*dst = malloc(sizeof(char)*len); //this is a bit ptr crazy,

	if(*dst == NULL) {
		ERRPRINTF("Allocation of memory to string failed.\n");
		return 0; //failure
	}

	strcpy(*dst, src);

	// LOGPRINTF("src:%s dst:%s\n", src, *dst);

	return 1; //success
}

/*!
 * converts a ISO 8601 time string to a time_t value
 *
 * @param   date    pointer to charater string representing a date
 * @return date converted to a time_t value
 */
time_t parseISO8601Date(char *date) {
	struct tm tm_t;
    struct tm* tm_p;
	time_t t, t2, offset = 0;
	int success = 0;
	char *pos;

	if(date == NULL) {
		ERRPRINTF("Error date string is null.\n");
		return (time_t)-1;
	}

    tm_p = &tm_t;

	memset(tm_p, 0, sizeof(struct tm));

	// we expect at least something like "2003-08-07T15:28:19" and
	// don't require the second fractions and the timezone info
	// the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD

	// full specified variant
	pos = (char *) strptime((const char *)date,
                            (const char *)"%t%Y-%m-%dT%H:%M%t",
                            tm_p);

    if(pos != NULL)
    {
		// Parse seconds
		if (*pos == ':') {
			pos++;
		}
		if (isdigit(pos[0]) && !isdigit(pos[1])) {
			tm_t.tm_sec = pos[0] - '0';
			pos++;
		}
		else if (isdigit(pos[0]) && isdigit(pos[1])) {
			tm_t.tm_sec = 10*(pos[0]-'0') + pos[1] - '0';
			pos +=2;
		}
		// Parse timezone
		if (*pos == 'Z') {
			offset = 0;
		}
		else if ((*pos == '+' || *pos == '-') &&
		         isdigit(pos[1]) && isdigit(pos[2]) &&
		         strlen(pos) >= 3) {

			offset = (10*(pos[1] - '0') + (pos[2] - '0')) * 60 * 60;

			if (pos[3] == ':' && isdigit(pos[4]) && isdigit(pos[5])) {
				offset +=  (10*(pos[4] - '0') + (pos[5] - '0')) * 60;
			}
			else if (isdigit(pos[3]) && isdigit(pos[4])) {
				offset +=  (10*(pos[3] - '0') + (pos[4] - '0')) * 60;
			}

			offset *= (pos[0] == '+') ? 1 : -1;

		}
		success = 1;

	} // only date ... not enough for us, set success to 0
	//else if(NULL != strptime((const char *)date, "%t%Y-%m-%d", &tm_t)) {
	//	success = 0;
	//}
	// there were others combinations too...

	if(1 == success) {

		// if((time_t)(-1) != (t = mktime(&tm_t))) {

        t = mktime(&tm_t);
        if(t != (time_t)(-1)) {
			// Correct for the local timezone
			t = t - offset;
			t2 = mktime(gmtime(&t));
			t = t - (t2 - t);

			return t;
		}
		else {
			ERRPRINTF("Time conversion error: mktime failed.\n");
		}
	} else {
		ERRPRINTF("Invalid ISO8601 date format.\n");
	}

	return (time_t)-1;
}


/*!
 * Function checks the routine structure for misconfigurations
 *
 * @param   r   pointer to routine
 *
 * @returns 1 if routine is OK, 0 if routine is BAD
 *
 */
int check_routine(struct pmm_routine *r) {
	int ret = 1;

    // TODO add checking of path names and strings
	if(r->pd_set->n_p <= 0) {
		ERRPRINTF("Number of paramaters for routine not set correctly.\n");
		print_routine(PMM_ERR, r);
		ret = 0;
	}

    /* TODO after changing to enum do we need to check this any more?
	if(r->condition < 0 || r->condition > 4) {
		ERRPRINTF("Execution Condition for routine not set correctly.\n");
		print_routine(PMM_ERR, r);
		ret = 0;
	}
    */

	if(r->priority < 0) {
		ERRPRINTF("Priority for routine not set correctly.\n");
		print_routine(PMM_ERR, r);
		ret = 0;
	}

	return ret;
}

/*!
 * prints the elements of the pmm_config structure for debugging
 *
 * @param   output  pointer to output stream
 * @param   cfg     pointer to config structure
 */
void print_config(const char *output, struct pmm_config *cfg) {

	int i;

	SWITCHPRINTF(output, "daemon: ");
	if(cfg->daemon) {
		SWITCHPRINTF(output, "yes\n");
	} else {
		SWITCHPRINTF(output, "no\n");
	}

    SWITCHPRINTF(output, "main sleep period: %ds %lds\n",
                             (int) cfg->ts_main_sleep_period.tv_sec,
                                   cfg->ts_main_sleep_period.tv_nsec);
	SWITCHPRINTF(output, "log file: %s\n", cfg->logfile);
	SWITCHPRINTF(output, "config file: %s\n", cfg->configfile);
	SWITCHPRINTF(output, "load path: %s\n", cfg->loadhistory->load_path);
	SWITCHPRINTF(output, "routine array size: %d\n", cfg->allocated);
	SWITCHPRINTF(output, "routine array used: %d\n", cfg->used);
    
    SWITCHPRINTF(output, "pause: %d\n", cfg->pause);

	for(i=0; i<cfg->used; i++) {
		print_routine(output, cfg->routines[i]);
	}

	return;
}

/*!
 * frees a configure structure and members it contains
 *
 * @param   cfg     pointer to address of config structure
 */
void free_config(struct pmm_config **cfg) {
	int i;

    if((*cfg)->loadhistory != NULL)
	    free_loadhistory(&((*cfg)->loadhistory));

	for(i=0; i<(*cfg)->used; i++) {
		free_routine(&((*cfg)->routines[i]));
	}

	free((*cfg)->routines);
    (*cfg)->routines = NULL;

	free(*cfg);
    *cfg = NULL;
}

/*!
 * frees a routine structure and members it contains
 *
 * @param   routine     pointer to address of routine structure
 */
void free_routine(struct pmm_routine **r) {

    if((*r)->model != NULL)
	    free_model(&((*r)->model));

	//free some malloced 'strings'
	free((*r)->name);
    (*r)->name = NULL;

    if((*r)->exe_args != NULL) {
        free((*r)->exe_args);
        (*r)->exe_args = NULL;
    }

	free((*r)->exe_path);
    (*r)->exe_path = NULL;

	//free paramdef set
    free_paramdef_set(&(*r)->pd_set);

	free(*r);
    *r = NULL;
}

/*!
 * frees a model structure and members it contains
 *
 * @param   m    pointer to address of the model structure
 */
void free_model(struct pmm_model **m) {

    if((*m)->bench_list != NULL)
	    free_bench_list(&((*m)->bench_list));

    if((*m)->interval_list != NULL)
        free_interval_list(&((*m)->interval_list));

    if((*m)->pd_set != NULL)
        free_paramdef_set(&((*m)->pd_set));

	free((*m)->model_path);
    (*m)->model_path = NULL;

	free(*m);
    *m = NULL;
}

/*!
 * frees a benchmark list structure and members it contains
 *
 * @param   bl    pointer to address of the benchmark list
 */
void free_bench_list(struct pmm_bench_list **bl)
{

    free_benchmark_list_backwards(&((*bl)->first));

	free(*bl);
    *bl = NULL;
}

/*!
 * frees a benchmark list structure and members it contains by
 * progressing forwards through the list
 *
 * @param   last_b    pointer to address of the benchmark 
 */
void free_benchmark_list_forwards(struct pmm_benchmark **last_b) {
    struct pmm_benchmark *this, *next;
    //TODO check naming last_b yet we remove forwards ?
    
    this = *last_b;

    while(this != NULL) {
        next = this->next;
        free_benchmark(&this);
        this = next;
    }

    return;
}

/*!
 * frees a benchmark list structure and members it contains by
 * progressing backwards through the list
 *
 * @param   first_b    pointer to address of the benchmark 
 */
void free_benchmark_list_backwards(struct pmm_benchmark **first_b) {
    struct pmm_benchmark *this, *prev;
    
    this = *first_b;

    while(this != NULL) {
        prev = this->previous;
        free_benchmark(&this);
        this = prev;
    }

    return;
}

/*!
 * frees a benchmark 
 *
 * @param   b    pointer to address of the benchmark 
 */
void free_benchmark(struct pmm_benchmark **b)
{
	free((*b)->p);
    (*b)->p = NULL;

	free(*b);
    *b = NULL;
}

