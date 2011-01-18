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
 * @file    pmm_selector.c
 * @brief   Code for chosing benchmark points
 *
 * This is the core of the application, implementing many methods to choose
 * benchmarking points to minimise the construction time of a model
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>     // for malloc, free, rand, exit

#include "pmm_model.h"
#include "pmm_selector.h"
#include "pmm_interval.h"
#include "pmm_param.h"
#include "pmm_load.h"
#include "pmm_log.h"

#ifdef HAVE_MUPARSER
#include "pmm_muparse.h"
#endif
/*******************************************************************************
 * gbbp_select_new_bench
 *
 * this function uses the current construction state of the model to select
 * a new point in the model to benchmark.
 *
 * State Description:
 *
 * EMPTY -  the model is empty, the selected benchmark will be the smallest
 *          problem size
 * CLIMB -  the model is in the initial stages of construction and the speed
 *          of execution is getting faster for larger problem sizes
 * BISECT - we are in the main phase of model construction, we bisect intervals
 *          that are retrieved from a stack. The stack contains intervals of the
 *          performance model in which we have not finalised construction
 *
 * The Stack:
 *
 * The construction stack contains intervals of the model where the model has
 * not yet been fully constructed. New intervals are added to the stack and old
 * ones removed after a benchmark has been completed and is being added to the
 * model. This process is taken care of in a seperate stage by the
 * gbbp_insert_benchmark function.
 *
 * gbbp_insert_benchmark also handles the changing of the construction state
 * and that is described there
 *
 * this function reads an interval off the top of the stack and selects a new
 * benchmark point based on that interval.
 */
/*
int gbbp_select_new_bench(struct pmm_routine *r) {

	struct pmm_interval *i;

	/// read construction interval from top of stack
	if(read_top_interval(r->model->interval_list, i) == 0) {
		printf("[gbbp_select_new_bench]: Error reading interval from stack.\n");
		return -1;
	}

	// return new benchmark based on that interval
	return gbbp_bench_from_interval(r, i);

}
*/

int
check_benchmarking_minimums(struct pmm_routine *r, double t, int n);
int
rand_between(int min, int max);

int
init_naive_1d_intervals(struct pmm_routine *r);
int
init_gbbp_naive_intervals(struct pmm_routine *r);
int
init_gbbp_diagonal_interval(struct pmm_routine *r);
int
init_gbbp_boundary_intervals(struct pmm_routine *r);

int
naive_process_interval_list(struct pmm_routine *r, struct pmm_benchmark *b);
int
process_interval_list(struct pmm_routine *r, struct pmm_benchmark *b,
                      struct pmm_loadhistory *h);
int
process_interval(struct pmm_routine *r, struct pmm_interval *i,
                 struct pmm_benchmark *b, struct pmm_loadhistory *h);
int
process_it_gbbp_empty(struct pmm_routine *r, struct pmm_interval *i);
int
process_it_gbbp_climb(struct pmm_routine *r, struct pmm_interval *i,
                      struct pmm_benchmark *b, struct pmm_loadhistory *h);
int
process_it_gbbp_bisect(struct pmm_routine *r, struct pmm_interval *i,
                    struct pmm_benchmark *b, struct pmm_loadhistory *h);
int
process_it_gbbp_inflect(struct pmm_routine *r, struct pmm_interval *i,
                     struct pmm_benchmark *b, struct pmm_loadhistory *h);
int
process_it_point(struct pmm_routine *r, struct pmm_interval *i);

int
naive_step_interval(struct pmm_routine *r, struct pmm_interval *interval);
int
adjust_interval_with_param_constraint_max(struct pmm_interval *i,
                                          struct pmm_paramdef_set *pd_set);
int
adjust_interval_with_param_constraint_min(struct pmm_interval *i,
                                          struct pmm_paramdef_set *pd_set);
int
set_params_step_along_climb_interval(int *params, int step,
                             struct pmm_interval *i,
                             struct pmm_paramdef_set *pd_set);
void
set_params_interval_midpoint(int *p, struct pmm_interval *i);
int
isnonzero_at_interval_end(struct pmm_interval *i,
                          struct pmm_paramdef_set *pd_set);
int
is_interval_divisible(struct pmm_interval *i, struct pmm_routine *r);
int
project_diagonal_intervals(struct pmm_model *m);

struct pmm_interval*
new_projection_interval(int *p, struct pmm_paramdef *pd, int d,
                         int n);
int
find_interval_matching_bench(struct pmm_routine *r, struct pmm_benchmark *b,
                             struct pmm_loadhistory *h,
                             struct pmm_interval **found_i);

int
multi_gbbp_bench_from_interval(struct pmm_routine *r,
		                       struct pmm_interval *interval,
                               int *params);

void mesh_boundary_models(struct pmm_model *m);
void recurse_mesh(struct pmm_model *m, int *p, int plane, int n_p);
/*
 * functions to write:
 *
 * bench_cut_greater(a,b) - takes benchmarks, uses history to calculate cuts and
 *							works out if a is greater than b
 *
 * bench_cut_contains(a,b) - takes benchmarks, uses history to calculate cuts
 *							 and works out if b is contained by a
 *
 * bench_cut_intersects(a,b) - takes benchmarks, uses history to calculate cuts
 *							   and works of it a intersects b
 *
 */


/*!
 * Select a new benchmark following a naive construction method where every
 * possible point in the model is benchmarked.
 *
 * This action is simply taking the top interval from the construction stack
 * and returning the parameters it describes.
 *
 * @param   r   pointer to the routine who's model is under construction
 *
 * @return pointer to newly allocated parameter array describing next bench
 * point or NULL on error
 *
 */
int*
multi_naive_select_new_bench(struct pmm_routine *r)
{
    int *params;

    struct pmm_interval_list *i_list;
    struct pmm_interval *new_i, *top_i;
    struct pmm_model *m;
#ifdef HAVE_MUPARSER
    int i;
    double pc;
#endif


    m = r->model;
    i_list = m->interval_list;

    // if interval list is empty initialise with a point interval at the start
    // parameter point
    if(isempty_interval_list(i_list) == 1) {
        DBGPRINTF("Interval list is empty, initialising new interval list.\n");

        new_i = new_interval();
        new_i->type = IT_POINT;
        new_i->n_p = r->pd_set->n_p;
        new_i->start = init_param_array_start(r->pd_set);

        if(new_i->start == NULL) {
            ERRPRINTF("Error initialising start parameter array.\n");

            free_interval(&new_i);

            return NULL;
        }

        // if pc min/max is defined
        if(r->pd_set->pc_max != -1 || r->pd_set->pc_min != -1) {
#ifdef HAVE_MUPARSER
            i = 0;
            while(i != new_i->n_p ) { // while not finished the naive space

                if(evaluate_constraint_with_params(r->pd_set->pc_parser,
                                                   new_i->start, &pc) < 0)
                {
                    ERRPRINTF("Error evalutating parameter constraint.\n");

                    free_interval(&new_i);

                    return NULL;
                }

                if(r->pd_set->pc_max != -1 &&
                   r->pd_set->pc_min == -1) 
                { //test only max
                    if(pc > r->pd_set->pc_max) {
                        i = naive_step_interval(r, new_i);
                    }
                    else {
                        break; //finished
                    }
                }
                else if(r->pd_set->pc_max == -1 &&
                        r->pd_set->pc_min != -1)
                { //test only min
                    if(pc < r->pd_set->pc_min) {
                        i = naive_step_interval(r, new_i);
                    }
                    else {
                        break; //finished
                    }
                }
                else
                { //test both
                    if(pc < r->pd_set->pc_min || pc > r->pd_set->pc_max) {
                        i = naive_step_interval(r, new_i);
                    }
                    else {
                        break;
                    }
                }
            }
            if(i == new_i->n_p) {
                ERRPRINTF("Could not initialise naive start point given pc "
                          "min/max restrictions.\n");

                free_interval(&new_i);

                return NULL;
            }
#else
            ERRPRINTF("muParser not enabled at configure.\n");

            free_interval(&new_i);

            return NULL;
#endif /* HAVE_MUPARSER */
        }

        add_top_interval(i_list, new_i);

    }

    
    if((top_i = read_top_interval(i_list)) == NULL) {
        ERRPRINTF("Error reading interval from head of list\n");
        return NULL;
    }

    DBGPRINTF("Choosing benchmark based on following interval:\n");
    print_interval(PMM_DBG, top_i);

    switch(top_i->type) {
        case IT_POINT :
        
            DBGPRINTF("assigning copy of interval 'point'.\n");

            params = init_param_array_copy(top_i->start, top_i->n_p);
            if(params == NULL) {
                ERRPRINTF("Error copying interval parameter point.\n");
                return NULL;
            }

            align_params(params, r->pd_set);

            return params;

        default :
            ERRPRINTF("Invalid interval type: %s\n",
                      interval_type_to_string(top_i->type));
            return NULL;
    }
}

/*!
 * Process the insertion of a new benchmark into a model being constructed
 * with a naive method.
 *
 * Benchmark is inserted into model and statistics are gathered on the
 * set of benchmarks in the model at the corresponding point. If certain
 * thresholds are exceeded the benchmark point will be removed from the
 * construction stack and the next point will be added to the stack
 *
 * The next point is calculated by 
 *
 * TODO rename pmm_interval to something more general ... ?
 */
int
multi_naive_insert_bench(struct pmm_routine *r, struct pmm_benchmark *b)
{

    double time_spend;
    int num_execs;
    int ret;

    ret = 0;

    //DBGPRINTF("benchmark params:\n");
    //print_params(PMM_DBG, b->p, b->n_p);

    if(insert_bench(r->model, b) < 0) {
        ERRPRINTF("Error inserting benchmark.\n");
        ret = -2;
    }

    //first check time spent benchmarking at this point, if it is less than
    //the threshold, we won't bother processing the interval list (so that
    //the benchmark may be executed again, until it exceeds the threshold
    calc_bench_exec_stats(r->model, b->p, &time_spend, &num_execs);

    DBGPRINTF("total time spent benchmarking point: %f\n", time_spend);
    DBGPRINTF("total number executions at benchmarking point: %d\n", num_execs);

    if(check_benchmarking_minimums(r, time_spend, num_execs))
    {
        DBGPRINTF("benchmarking threshold exceeeded (t:%d, n:%d), processing intervals.\n", r->min_sample_time, r->min_sample_num);


        
        if(naive_process_interval_list(r, b) < 0) {
            ERRPRINTF("Error proccessing intervals, inserting bench anyway\n");
            ret = -1;
        }



    } else {
        DBGPRINTF("benchmarking threshold not exceeded.\n");
    }


	// after intervals have been processed ...

    return ret;
}

/*!
 * check benchmark execution statistics against minimum requirements
 * in routine configuration
 *
 * @param   r   pointer to the routine
 * @param   t   time spent benchmarking as a double
 * @param   n   number of benchmarks taken
 *
 * @return 0 if minimums are not satisfied, 1 if they are
 */
int
check_benchmarking_minimums(struct pmm_routine *r, double t, int n)
{
    if((r->min_sample_time != -1 && t >= (double)r->min_sample_time) ||
       (r->min_sample_num != -1 && n >= r->min_sample_num) ||
       (r->min_sample_time == -1 && r->min_sample_num  == -1))
    {
        return 1;
    }
    else {
        return 0;
    }
}

/*!
 * Process the interval list of the naive construction method, after a new
 * benchmark point has been aquired.
 *
 * Check that the top interval corresponds to the new benchmark and if it
 * does, step the parameter point of the top interval along to the next
 * target point.
 *
 * In the naive construction method we benchmark every possible point in the
 * parameter space described by the parameter definitions. In general, this
 * set of points P is given by the n-ary Cartesian product of the sets of
 * each of the n parameters, p_0, p_1, p_n, where such sets are defined by
 * our parameter definitions in terms of start values, end values, stride
 * and offsets.
 *
 * We iterate through such a set of points in lexicographical order, i.e.
 * given points a and b in P, they are successive iff:
 *
 *  (a_1, a_2, \dots, a_n) <^d (b_1,b_2, \dots, b_n) \iff
 *  (\exists\ m > 0) \ (\forall\ i < m) (a_i = b_i) \land (a_m <_m b_m) 
 *
 * Given a point p, incrementing it to p' involves the following: Increment
 * the first term, if this incremented value is greater than the end defined
 * for the first term (i.e. it has overflowed), set the term to the start
 * value and equal start_1 and apply this overflow by incrementing the next
 * term, testing that the next term has not also overflowed, and letting any 
 * overflow cascade through terms of the point. You may recognise this kind
 * of process in the natural way one counts :-)
 *
 * @param   r   pointer to the routine
 * @param   b   pointer to the new benchmark
 *
 * @return 0 on success, -1 on failure
 */
int
naive_process_interval_list(struct pmm_routine *r, struct pmm_benchmark *b)
{
    struct pmm_model *m;
    struct pmm_interval *interval;
    struct pmm_interval *new_i;
    int *aligned_params;
    int i;
#ifdef HAVE_MUPARSER
    double pc;
#endif

    m = r->model;
    interval = m->interval_list->top;

    aligned_params = init_aligned_params(interval->start, r->pd_set);
    if(aligned_params == NULL) {
        ERRPRINTF("Error initialising aligned param array.\n");
        return -1;
    }

    

    //if the benchmark parameters do not match ...
    if(params_cmp(b->p, aligned_params, b->n_p) != 0) {
        LOGPRINTF("unexpected benchmark ... expected:\n");
        print_params(PMM_LOG, interval->start, interval->n_p);
        LOGPRINTF("got:\n");
        print_params(PMM_LOG, b->p, b->n_p);

        return 0; //no error
    }

    // step the parameters the interval describes along by 'stride'
    //
    // iterate over all parameters
    if(r->pd_set->pc_max != -1 || r->pd_set->pc_min != -1) {
#ifdef HAVE_MUPARSER
        i = naive_step_interval(r, interval);
        while(i != interval->n_p ) { // while not finished the naive space

            if(evaluate_constraint_with_params(r->pd_set->pc_parser,
                        interval->start, &pc) < 0)
            {
                ERRPRINTF("Error evalutating parameter constraint.\n");
                return -1;
            }

            if(r->pd_set->pc_max != -1 &&
               r->pd_set->pc_min == -1) 
            { //test only max
                if(pc > r->pd_set->pc_max) {
                    i = naive_step_interval(r, interval);
                }
                else {
                    break; //finished
                }
            }
            else if(r->pd_set->pc_max == -1 &&
                    r->pd_set->pc_min != -1)
            { //test only min
                if(pc < r->pd_set->pc_min) {
                    i = naive_step_interval(r, interval);
                }
                else {
                    break; //finished
                }
            }
            else
            { //test both
                if(pc < r->pd_set->pc_min || pc > r->pd_set->pc_max) {
                    i = naive_step_interval(r, interval);
                }
                else {
                    break;
                }
            }
        }
#else
        ERRPRINTF("muParser not enabled at configure.\n");
        return -1;
#endif /* HAVE_MUPARSER */
    }
    else {
        i = naive_step_interval(r, interval);
    }

    //if the most signicant parameter/digit also overflowed that means we
    //have gone through all parameters as defined by our constraints,
    //construction is now complete.
    if(i==interval->n_p) {

        LOGPRINTF("Naive construction complete.\n");
        remove_interval(m->interval_list, interval);

        if(isempty_interval_list(m->interval_list) == 1) {
            new_i = new_interval();
            new_i->type = IT_COMPLETE;
            add_top_interval(m->interval_list, new_i);

            m->complete = 1;
        }
        else {
            ERRPRINTF("Expected empty construction interval list but found:\n");
            print_interval_list(PMM_ERR, m->interval_list);
            return -1;
        }
    }

    return 0;
}

/*!
 * Step the start point of a naive interval to the next benchmark point.
 *
 * @param   r           pointer to the routine
 * @param   interval    pointer to the interval
 *
 * @return index of the last parameter that was incremented or N (the number of
 * parameters) when all paramters overflowed, i.e. the start point could not
 * be stepped and the last naive benchmark has been reached.
 */
int
naive_step_interval(struct pmm_routine *r, struct pmm_interval *interval)
{
    int i;
    int direction;

    for(i=0; i<interval->n_p; i++) {
        
        if(r->pd_set->pd_array[i].start > r->pd_set->pd_array[i].end) {
            direction = -1;
        }
        else {
            direction = 1;
        }

        // if stepping a parameter does not cause it to leave its boundaries
        if(param_within_paramdef(interval->start[i] +
                                 direction*r->pd_set->pd_array[i].stride,
                                 &(r->pd_set->pd_array[i])) == 1)
        {
            // increment the parameter as tested and finish
            interval->start[i] += direction*r->pd_set->pd_array[i].stride;

            print_params(PMM_DBG, interval->start, r->pd_set->n_p);

            break; // we are done
        }
        else {
            //otherwise overflow would occur so set this parameter to
            //it's start value and allow the overflow to cascade into the
            //next parameter by continuing through the loop (and trying to
            //iterate the next parameter)
            interval->start[i] = r->pd_set->pd_array[i].start;
        }
    }

    return i;
}

/*!
 * Returns a new point to benchmark using the naive bisection in 1 dimension
 * method. Briefly, this method initialises a bisection construction interval
 * across the problem space (1-d only). Then recursively bisects this interval
 * and benchmarks at bisection points, until it is no longer divisible.
 *
 * @param   r   pointer to the routine for which we will find a new benchmark
 *              point
 *
 * @return pointer to an array describing the new benchmark point or NULL on
 * failure
 */
int*
naive_1d_bisect_select_new_bench(struct pmm_routine *r)
{
    int *params;

	struct pmm_interval_list *i_list;
	struct pmm_interval *top_i;
    struct pmm_model *m;

    m = r->model;
	i_list = m->interval_list;


    if(r->pd_set->n_p != 1) {
        ERRPRINTF("Error, Naive 1D Bisection method cannot be used to construct"
                  " for multi-dimensional problem.\n");
        return NULL;
    }



	//if model construction has not started, i_list will be empty
	if(isempty_interval_list(i_list) == 1) {

        DBGPRINTF("Interval list is empty, initializing new interval list.\n");

        // create a new bisection interval type from start to end
        // create point intervals a the end points of the the bisection interval
        if(init_naive_1d_intervals(r) < 0) {
            ERRPRINTF("Error initialising new construction intervals.\n");
            return NULL;
        }

	}

    params = malloc(r->pd_set->n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    if((top_i = read_top_interval(i_list)) == NULL) {
        ERRPRINTF("Error reading interval from head of list\n");

        free(params);
        params = NULL;

        return NULL;
    }

    DBGPRINTF("Choosing benchmark based on following interval:\n");
    print_interval(PMM_DBG, top_i);

    switch(top_i->type) {
        case IT_GBBP_BISECT :

            DBGPRINTF("Applying step of GBBP to benchmark selection.\n");

            //apply GBBP to the bisection interval (we will get midpoint)
            multi_gbbp_bench_from_interval(r, top_i, params);
            break;


        case IT_POINT :

            DBGPRINTF("Benchmarking at point ...\n");
            //benchmark at this point
            multi_gbbp_bench_from_interval(r, top_i, params);
            break;

        default :
            ERRPRINTF("Invalid interval type: %s (%d)\n",
                       interval_type_to_string(top_i->type), top_i->type);
            print_interval(PMM_ERR, top_i);

            free(params);
            params = NULL;

            break;
    }

    return params;
}


/*!
 * Function initialises a construction intervals for 1d naive bisection. A
 * interval covering the problem space with bisection type is created. Two
 * further 'point' type intervals are created at the end points of the
 * bisection interval.
 *
 * @return 0 on success or -1 on failure
 */
int
init_naive_1d_intervals(struct pmm_routine *r)
{
    struct pmm_model *m;
    struct pmm_interval_list *i_list;
    struct pmm_interval *bisect_i, *point_i;

    m = r->model;
    i_list = m->interval_list;

    // first initialize a bisection interval
    bisect_i = new_interval();
    bisect_i->type = IT_GBBP_BISECT;

    bisect_i->n_p = r->pd_set->n_p;

    bisect_i->start = init_param_array_start(r->pd_set);
    bisect_i->end = init_param_array_end(r->pd_set);
    if(bisect_i->start == NULL || bisect_i->end == NULL) {
        ERRPRINTF("Error initializing start/end parameter array.\n");

        free_interval(&bisect_i);

        return -1;
    }

    // adjust the diagonal for pc_max (but not pc_min)
    if(r->pd_set->pc_max != -1) {
        adjust_interval_with_param_constraint_max(bisect_i, r->pd_set);
    }
    if(r->pd_set->pc_min != -1) {
        adjust_interval_with_param_constraint_min(bisect_i, r->pd_set);
    }

    if(is_interval_divisible(bisect_i, r) == 1) {
        add_top_interval(m->interval_list, bisect_i);
    }
    else {
        DBGPRINTF("Interval not divisible, not adding.\n");
        print_interval(PMM_DBG, bisect_i);
        free_interval(&bisect_i);

        return -1;
    }

    // add benchmark at end of bisection interval
    point_i = new_interval();
    point_i->type = IT_POINT;
    point_i->n_p = r->pd_set->n_p;

    point_i->start = init_param_array_copy(bisect_i->end, point_i->n_p);
    if(point_i->start == NULL) {
        ERRPRINTF("Error initialising parameter array copy.\n");
        free_interval(&point_i);
        return -1;
    }
    add_top_interval(i_list, point_i);


    // add benchmark at start of bisection interval
    point_i = new_interval();
    point_i->type = IT_POINT;
    point_i->n_p = r->pd_set->n_p;

    point_i->start = init_param_array_copy(bisect_i->start, point_i->n_p);
    if(point_i->start == NULL) {
        ERRPRINTF("Error initialising parameter array copy.\n");
        free_interval(&point_i);
        return -1;
    }
    add_top_interval(i_list, point_i);

    return 0; //success
}

/*!
 * builds a multi-parameter piece-wise performance model using a random
 * selection of benchmark points
 *
 * Alogirthm description:
 *
 * if model is empty
 *   select start values for all parameters and return benchmark point
 * else
 *   seed random generator
 *
 *   for each parameter
 *     select a random parameter size based on the paramdef limits and return
 *
 * @param   r   pointer to routine for which the model is being built
 * 
 * @return pointer to newly allocated array describing parameters for the
 * benchmark
 */
int* multi_random_select_new_bench(struct pmm_routine *r)
{
    int *params;
    int i;


	//allocate parameter return array
    params = malloc(r->pd_set->n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
		return NULL;
	}



	//if model is empty
	if(isempty_model(r->model)) {

		//set paremeter reutrn array to the origin point
        set_param_array_start(params, r->pd_set);

	}
	else {

		//seed
		srand(time(NULL));

		//choose a random set of params within the parmdef limits
		//make sure choosen random params are not already in the model
		//do {
		//	for(i=0; i<r->pd_set->n_p; i++) {
		//		params[i] = rand_between(r->pd_set->pd_array[i].start,
		//		                         r->pd_set->pd_array[i].end);
		//	}
		//} while(get_bench(r->model, params) != NULL); //test in model already

		for(i=0; i<r->pd_set->n_p; i++) {
			params[i] = rand_between(r->pd_set->pd_array[i].start,
			                         r->pd_set->pd_array[i].end);
		}

        align_params(params, r->pd_set);

	}

	//return benchmark parameters
	return params;
}


/*!
 * find a random integer between two values (inclusive)
 *
 * @param   min     first integer
 * @param   max     second integer
 *
 * @return random integer between the two values passed
 */
int
rand_between(int min, int max)
{
    //TODO static/inline
	if(min==max) {
		return min;
	}

    // if min is greater than max, swap them around
	if(min>max) {
		max = max+min;
		min = max-min; //min now equals original max
		max = max-min; //max now equals original min
	}

	return (int)(rand()%(max-min))+min;
}


/*!
 * Returns a new point to benchmark using the Multidimensional Naive GBBP
 * method. Briefly, this method initialises construction intervals in a grid
 * form, through all possible points as defined by the parameter definitions.
 * Then GBBP is applied to all construction intervals to select benchmark
 * points, until the model has been built along all lines in the grid.
 *
 * @param   r   pointer to the routine for which we will find a new benchmark
 *              point
 *
 * @return pointer to an array describing the new benchmark point or NULL on
 * failure
 */
int*
multi_gbbp_naive_select_new_bench(struct pmm_routine *r)
{
    int *params;

	struct pmm_interval_list *i_list;
	struct pmm_interval *top_i;
    struct pmm_model *m;

    m = r->model;
	i_list = m->interval_list;

	//if model construction has not started, i_list will be empty
	if(isempty_interval_list(i_list) == 1) {

        DBGPRINTF("Interval list is empty, initializing new interval list.\n");

        if(init_gbbp_naive_intervals(r) < 0) {
            ERRPRINTF("Error initialising new construction intervals.\n");
            return NULL;
        }
	}

    params = malloc(r->pd_set->n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    if((top_i = read_top_interval(i_list)) == NULL) {
        ERRPRINTF("Error reading interval from head of list\n");

        free(params);
        params = NULL;

        return NULL;
    }

    DBGPRINTF("Choosing benchmark based on following interval:\n");
    print_interval(PMM_DBG, top_i);

    switch(top_i->type) {
        case IT_GBBP_EMPTY :
        case IT_GBBP_CLIMB :
        case IT_GBBP_BISECT :
        case IT_GBBP_INFLECT :

            DBGPRINTF("Applying step of GBBP to benchmark selection.\n");

            //apply GBBP
            multi_gbbp_bench_from_interval(r, top_i, params);
            break;


        case IT_POINT :

            DBGPRINTF("Benchmarking at point ...\n");
            //benchmark at this point
            multi_gbbp_bench_from_interval(r, top_i, params);
            break;

        default :
            ERRPRINTF("Invalid interval type: %s (%d)\n",
                       interval_type_to_string(top_i->type), top_i->type);
            print_interval(PMM_ERR, top_i);

            free(params);
            params = NULL;

            break;
    }

    return params;
}

/*!
 * Function initialises a construction intervals in a grid form across the
 * whole parameter space. To form the grid, for each parameter, for all
 * possible parameter values on that axis, intervals are projected parallel to
 * all other axes.
 *
 * Other intervals are added to benchmark neccessary extremeties of the
 * parameter space and to tag the completion of the diagonal and the whole
 * model in general.
 *
 * @return 0 on success or -1 on failure
 */
int
init_gbbp_naive_intervals(struct pmm_routine *r)
{
    int j;
    struct pmm_model *m;
    struct pmm_interval_list *i_list;
    struct pmm_interval *diag_i, *proj_i, *point_i;
    struct pmm_benchmark *zero_b;

    int ret;
    int *step_params;
    int step;

    m = r->model;
    i_list = m->interval_list;

    step_params = malloc(r->pd_set->n_p * sizeof *step_params);
    if(step_params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1;
    }

    // first initialize a diagonal interval
    diag_i = new_interval();
    diag_i->type = IT_GBBP_EMPTY;

    diag_i->n_p = r->pd_set->n_p;

    diag_i->start = init_param_array_start(r->pd_set);
    diag_i->end = init_param_array_end(r->pd_set);
    if(diag_i->start == NULL || diag_i->end == NULL) {
        ERRPRINTF("Error initializing start/end parameter array.\n");

        free_interval(&diag_i);

        free(step_params);
        step_params = NULL;

        return -1;
    }

    // adjust the diagonal for pc_max (but not pc_min)
    if(r->pd_set->pc_max != -1) {

        adjust_interval_with_param_constraint_max(diag_i, r->pd_set);

    }


    // now step along this diagonal
    step = 0;
    while((ret = set_params_step_along_climb_interval(step_params, step, diag_i,
                                                      r->pd_set)) == 0)
                                                      
    {

        // at each point, iterate through all parameters and project intervals
        // perpendicular to all planes
        for(j=0; j<r->pd_set->n_p; j++)
        {


            proj_i = new_projection_interval(step_params,
                    &(r->pd_set->pd_array[j]),
                    j, r->pd_set->n_p);

            if(proj_i == NULL) {
                ERRPRINTF("Error creating projection interval.\n");

                free_interval(&diag_i);

                free(step_params);
                step_params = NULL;

                return -1;
            }

            //adjust interval for pc_max / pc_min
            if(r->pd_set->pc_max != -1) {
                adjust_interval_with_param_constraint_max(proj_i, r->pd_set);
                        
            }
            if(r->pd_set->pc_min != -1) {
                adjust_interval_with_param_constraint_min(proj_i, r->pd_set);
                        
            }

            // only add if the interval is not zero length and divisible
            if(proj_i->start[j] != proj_i->end[j] &&
                    is_interval_divisible(proj_i, r) == 1)
            {
                DBGPRINTF("adding interval:\n");
                print_interval(PMM_DBG, proj_i);
                add_bottom_interval(i_list, proj_i);

            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, proj_i);
                free_interval(&proj_i);
            }


            // if the projection to a non zero end point (and we did not
            // empty the proj_i because it was non-divisible ....
            if(r->pd_set->pd_array[j].nonzero_end == 1 && proj_i != NULL)
            {
                //add an IT_POINT interval at the endpoint of the projected
                //interval
                point_i = init_interval(0, r->pd_set->n_p, IT_POINT, proj_i->end,
                        NULL);
                if(point_i == NULL) {
                    ERRPRINTF("Error creating new point interval.\n");
                    return -1;
                }

                DBGPRINTF("Adding non zero end interval naive projection\n");
                print_interval(PMM_DBG, point_i);

                add_top_interval(i_list, point_i);

            }
            else if(proj_i != NULL)
            {
                // create a zero speed point at end ...
                zero_b = init_zero_benchmark(proj_i->end, r->pd_set->n_p);
                if(zero_b == NULL) {
                    ERRPRINTF("Error allocating new benchmark.\n");
                    return -1;
                }

                //add to model
                if(insert_bench(r->model, zero_b) < 0) {
                    ERRPRINTF("Error inserting a zero benchmark.\n");
                    free_benchmark(&zero_b);

                    return -1;
                }
            }
        }

            step++;
    }

    if(ret != -1) {
        ERRPRINTF("Error stepping along diagonal interval.\n");

        free_interval(&diag_i);

        free(step_params);
        step_params = NULL;

        return -1;
    }


    free_interval(&diag_i);

    free(step_params);
    step_params = NULL;

    return 0; //success
}


/*!
 * Returns a new point to benchmark using the Multidimensional Diagonal GBBP
 * method. Briefly, this method first constructs, using the GBBP algorithm,
 * on an interval from the start point of all parameter definitions to the end
 * point of those definitions. After construction along this interval is
 * complete, all points that were measured along it are used to project new
 * construction intervals. From each point N construction intervals are
 * projected parallel to each of the N parameter axes. GBBP is then applied to
 * these new construction intervals, completing the model.
 *
 * @param   r   pointer to the routine for which we will find a new benchmark
 *              point
 *
 * @return pointer to an array describing the new benchmark point or NULL on
 * failure
 */
int*
multi_gbbp_diagonal_select_new_bench(struct pmm_routine *r)
{
    int *params;

	struct pmm_interval_list *i_list;
	struct pmm_interval *top_i, *new_i;
    struct pmm_model *m;

    m = r->model;
	i_list = m->interval_list;

	//if model construction has not started, i_list will be empty
	if(isempty_interval_list(i_list) == 1) {

        DBGPRINTF("Interval list is empty, initializing new interval list.\n");

        if(init_gbbp_diagonal_interval(r) < 0) {
            ERRPRINTF("Error initialising new construction intervals.\n");
            return NULL;
        }
	}

    params = malloc(r->pd_set->n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    if((top_i = read_top_interval(i_list)) == NULL) {
        ERRPRINTF("Error reading interval from head of list\n");

        free(params);
        params = NULL;

        return NULL;
    }

    DBGPRINTF("Choosing benchmark based on following interval:\n");
    print_interval(PMM_DBG, top_i);

    switch(top_i->type) {
        case IT_GBBP_EMPTY :
        case IT_GBBP_CLIMB :
        case IT_GBBP_BISECT :
        case IT_GBBP_INFLECT :

            DBGPRINTF("Applying step of GBBP to benchmark selection.\n");

            //apply GBBP
            multi_gbbp_bench_from_interval(r, top_i, params);
            break;

        case IT_BOUNDARY_COMPLETE :

            DBGPRINTF("GBBP stage of construction complete. Meshing ...\n");

            //remove top interval
            remove_interval(i_list, top_i);

            if(r->pd_set->n_p > 1) {
                //mesh boundary models to create points
                if(project_diagonal_intervals(m) < 0) {
                    ERRPRINTF("Error projecting new intervals from diagonal\n");

                    free(params);
                    params = NULL;

                    return NULL;
                }

                //read another top interval
                if((top_i = read_top_interval(i_list)) == NULL) {
                    ERRPRINTF("Error reading interval from head of list\n");

                    free(params);
                    params = NULL;

                    return NULL;
                }

                DBGPRINTF("Meshing complete, fetching benchmark ...\n");

                //get the benchmark from the interval
                multi_gbbp_bench_from_interval(r, top_i, params);
            }
            else {
                ERRPRINTF("Invalid IT_BOUNDARY_COMPLETE in 1 param model.\n");

                // must check to see if interval list is empty now, if
                // completion is not tagged here then construction will restart
                if(isempty_interval_list(m->interval_list)) {
                    LOGPRINTF("Multidensional Diagonal GBBP Construction "
                               "complete.\n");

                    new_i = new_interval();
                    new_i->type = IT_COMPLETE;
                    add_top_interval(m->interval_list, new_i);

                    m->complete = 1;
                }

                free(params);
                params = NULL;
            }

            break;

        case IT_POINT :

            DBGPRINTF("Benchmarking at point ...\n");
            //benchmark at this point
            multi_gbbp_bench_from_interval(r, top_i, params);
            break;

        default :
            ERRPRINTF("Invalid interval type: %s (%d)\n",
                       interval_type_to_string(top_i->type), top_i->type);
            print_interval(PMM_ERR, top_i);

            free(params);
            params = NULL;

            break;
    }

    return params;
}

/*!
 * Function initialises a construction interval between two points which form
 * a diagonal through the parameter space defined by the parameter definitions.
 * For $n$ parameters with defintions of start points $s_i$ and end points
 * $e_i$, the diagonal starts at:
 *
 *     $(s_0, s_1, ..., s_n)$
 *
 * and ends at:
 *
 *     $(e_0, e_1, ..., e_n)$
 *
 * @param   r   pointer to the routine for which the interval will be
 *              initialised
 *
 * Other intervals are added to benchmark neccessary extremeties of the
 * parameter space and to tag the completion of the diagonal and the whole
 * model in general.
 *
 * @return 0 on success or -1 on failure
 */
int
init_gbbp_diagonal_interval(struct pmm_routine *r)
{
    int j;
    int all_nonzero_end;
    struct pmm_model *m;
    struct pmm_interval_list *i_list;
    struct pmm_interval *new_i, *diag_i;
    struct pmm_benchmark *zero_b;

    m = r->model;
    i_list = m->interval_list;

    //models with more than 1 boundary need a tag to indicate mesh stage
    //of construction
    if(r->pd_set->n_p > 1) {
        new_i = new_interval();
        new_i->type = IT_BOUNDARY_COMPLETE;
        add_top_interval(i_list, new_i);
    }



    // add an empty interval from start -> end along a diagonal through
    // the parameter space
    diag_i = new_interval(); 
    diag_i->type = IT_GBBP_EMPTY; 

    diag_i->n_p = r->pd_set->n_p;

    diag_i->start = init_param_array_start(r->pd_set);
    if(diag_i->start == NULL) {
        ERRPRINTF("Error initialising start parameter array.\n");
        free_interval(&diag_i);
        return -1;
    }

    diag_i->end = init_param_array_end(r->pd_set);
    if(diag_i->end == NULL) {
        ERRPRINTF("Error initialising end parameter array.\n");
        free_interval(&diag_i);
        return -1;
    }

    // if parameter product max is set, we must adjust the new interval so
    // it satisfies the constraint
    if(r->pd_set->pc_max != -1) {

        adjust_interval_with_param_constraint_max(diag_i, r->pd_set);
                                               

    }
    if(r->pd_set->pc_min != -1) {
        adjust_interval_with_param_constraint_min(diag_i, r->pd_set);
    }

    if(is_interval_divisible(diag_i, r) == 1) {
        add_top_interval(m->interval_list, diag_i);
    }
    else {
        DBGPRINTF("Interval not divisible, not adding.\n");
        print_interval(PMM_DBG, diag_i);
        free_interval(&diag_i);
    }



    // nonzero_end is set there will be no zero-speed benchmarks that
    // bound the parameter space of the model. So we must benchmark at these
    // points so that or model has a fully bounded space before the GBBP
    // algorithm begins
    //
    // for each parameter, if nonzer end is set, create a POINT interval at
    // a end value for that parameter along its axis, i.e.
    // start_0,start_1,...,end_i,...start_n,
    // 
    // Also, if nonzero_end is NOT set, set all_nonzer_end to 0/false
    all_nonzero_end = 1;
    for(j=0; j<r->pd_set->n_p; j++) {
        if(r->pd_set->pd_array[j].nonzero_end == 1) {
            // add benchmark at end on axis boundary (nonzero end)
            new_i = new_interval();
            new_i->type = IT_POINT;
            new_i->n_p = r->pd_set->n_p;

            new_i->start = init_param_array_start(r->pd_set);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising start parameter array.\n");
                free_interval(&new_i);
                return -1;
            }

            new_i->start[j] = r->pd_set->pd_array[j].end;

            add_top_interval(i_list, new_i);
        }
        else {
            all_nonzero_end = 0;
        }
    }

    
    // if nonzero_end is set on all parameters the then we will benchmark
    // at the end point of the diagonal interval. This should be added
    // to the top of the interval stack so that it is measured before
    // GBBP diagonal construction proceeds
    if(all_nonzero_end == 1 && diag_i != NULL) {
        DBGPRINTF("nonzero end on all params\n");

        // add benchmark at end of diagonal
        new_i = new_interval();
        new_i->type = IT_POINT;
        new_i->n_p = r->pd_set->n_p;

        new_i->start = init_param_array_copy(diag_i->end, r->pd_set->n_p);
        if(new_i->start == NULL) {
            ERRPRINTF("Error initialising end point parameter array.\n");
            free_interval(&new_i);
            return -1;
        }

        add_top_interval(i_list, new_i);

    }
    else if(diag_i != NULL) {
        // if we made an adjustment to max,max,...max via the param
        // constraints
        if(r->pd_set->pc_max != -1) {

            // create a zero speed point at interval end ...
            zero_b = init_zero_benchmark(diag_i->end, r->pd_set->n_p);
            if(zero_b == NULL) {
                ERRPRINTF("Error allocating new benchmark.\n");
                return -1;
            }

            //add to model
            if(insert_bench(r->model, zero_b) < 0) {
                ERRPRINTF("Error inserting a zero benchmark.\n");
                free_benchmark(&zero_b);

                return -1;
            }
        }
    }

    DBGPRINTF("New Intervals initialized:\n");
    print_interval_list(PMM_DBG, i_list);

    return 0; //success
}

/*!
 * Step along an interval until the product of the parameters at the end/start
 * point of the interval matches the minium parameter product 
 *
 * @param   i           pointer to the interval that is to be adjusted
 * @param   pd_set      pointer to the parameter definition set
 *
 * @return 0 on success or -1 on failure
 */
int
adjust_interval_with_param_constraint_min(struct pmm_interval *i,
                                          struct pmm_paramdef_set *pd_set)
{
#ifdef HAVE_MUPARSER
    int *pc_min_params;
    double pp, start_pc, end_pc;
    int step;
    int ret;

    // test to see if interval end point or start point exceed the pc_min
    // if start exceeds, we adjust the start point, if end exceeds, we adjust
    // the endpoint, if neither exceed, no adjustments need to be made, and
    // if both exceed, there is an error

    if(evaluate_constraint_with_params(pd_set->pc_parser, i->start, &start_pc)
       < 0)
    {
        ERRPRINTF("Error evaluating parameter constraint.\n");
        return -1;
    }
    if(evaluate_constraint_with_params(pd_set->pc_parser, i->end, &end_pc)
       < 0)
    {
        ERRPRINTF("Error evaluating parameter constraint.\n");
        return -1;
    }

    if(start_pc < pd_set->pc_min && end_pc < pd_set->pc_min) {
        ERRPRINTF("Interval cannot be adjusted to fit parameter product "
                  "constraint.\n");
        return -1;
    }
    else if(start_pc < pd_set->pc_min) {
    }
    else if(end_pc < pd_set->pc_min) {
    }
    else {
        DBGPRINTF("Interval already satisfies parameter product constraint.\n");
    }

    pc_min_params = malloc(pd_set->n_p * sizeof *pc_min_params);
    if(pc_min_params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1;
    }



    step = 0;
    while(1) {

        // if it is the start point we are adjusting step along start->end
        if(start_pc < pd_set->pc_min) {
            ret = set_params_step_between_params(pc_min_params,
                                      i->start, i->end, step, pd_set);
        }
        // otherwise set along end->start
        else {
            ret = set_params_step_between_params(pc_min_params,
                                      i->end, i->start, step, pd_set);
        }
                                                   
        if(ret < 0) {
            break;
        }

        // now calculate product at the stepped point
        if(evaluate_constraint_with_params(pd_set->pc_parser, pc_min_params,
                                           &pp) < 0)
        {
            ERRPRINTF("Error evaluating parameter constraint.\n");

            free(pc_min_params);
            pc_min_params = NULL;

            return -1;
        }

        DBGPRINTF("param product:%f, param_product_min:%d\n", pp,
                  pd_set->pc_min);

        // stop when pp is greater or equal to pc_min, later we will set
        // the start/end point to pc_min_params
        if(pp >= pd_set->pc_min) {
            break;
        }

        step++;
    }

    // if we never found a pp that satisfied pc_min, but ran out of steps, then
    // just keep the interval as it is ...
    if(ret < 0) {
        DBGPRINTF("Parameter product was not exceeded by interval.\n");
    }
    // otherwise lets set the end/start point to the value at the step
    // TODO couldn't we just set start/end to the current pc_min_params array?
    else {
        if(start_pc < pd_set->pc_min) {
            ret = set_params_step_between_params(i->start,
                                      i->start, i->end, step, pd_set);
        }
        else {
            ret = set_params_step_between_params(i->end,
                                      i->end, i->start, step, pd_set);
        }
    }


    free(pc_min_params);
    pc_min_params = NULL;

    return 0;
#else
    (void)i;        //TODO unused
    (void)pd_set;
    ERRPRINTF("muParser not enabled at configure.\n");
    return -1;
#endif /* HAVE_MUPARSER */
}

/*!
 * Step along an interval until the product of the parameters at the end
 * point of the interval matches the maximum parameter product (equal or less
 * than if nonzero_max is set, or equal to or 1 step great than if nonzero_max
 * is not set).
 *
 * @param   i           pointer to the interval that is to be adjusted
 * @param   pd_set      pointer to the parameter definition set
 *
 * @return 0 on success or -1 on failure
 */
int
adjust_interval_with_param_constraint_max(struct pmm_interval *i,
                                          struct pmm_paramdef_set *pd_set)
{
#ifdef HAVE_MUPARSER
    int *pc_max_params;
    double pp, start_pc, end_pc;
    int step;
    int nonzero_end;
    int ret;

    // check if end point is fuzzy
    nonzero_end = isnonzero_at_interval_end(i, pd_set);

    // test to see if interval end point or start point exceed the pc_max
    // if start exceeds, we adjust the start point, if end exceeds, we adjust
    // the endpoint, if neither exceed, no adjustments need to be made, and
    // if both exceed, there is an error
    if(evaluate_constraint_with_params(pd_set->pc_parser, i->start, &start_pc)
       < 0)
    {
        ERRPRINTF("Error evaluating parameter constraint.\n");
        return -1;
    }
    if(evaluate_constraint_with_params(pd_set->pc_parser, i->end, &end_pc)
       < 0)
    {
        ERRPRINTF("Error evaluating parameter constraint.\n");
        return -1;
    }

    if(start_pc > pd_set->pc_max && end_pc > pd_set->pc_max) {
        ERRPRINTF("Interval cannot be adjusted to fit parameter product "
                  "constraint.\n");
        return -1;
    }
    else if(start_pc > pd_set->pc_max) {
    }
    else if(end_pc > pd_set->pc_max) {
    }
    else {
        DBGPRINTF("Interval already satisfies parameter product constraint.\n");
    }

    pc_max_params = malloc(pd_set->n_p * sizeof *pc_max_params);
    if(pc_max_params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1;
    }



    step = 0;
    while(1) {

        if(start_pc > pd_set->pc_max) {
            ret = set_params_step_between_params(pc_max_params,
                                      i->end, i->start, step, pd_set);
        }
        else {
            ret = set_params_step_between_params(pc_max_params,
                                      i->start, i->end, step, pd_set);
        }
                                                   
        if(ret < 0) {
            break;
        }

        // now calculate product at the stepped point
        if(evaluate_constraint_with_params(pd_set->pc_parser, pc_max_params,
                                           &pp) < 0)
        {
            ERRPRINTF("Error evaluating parameter constraint.\n");

            free(pc_max_params);
            pc_max_params = NULL;

            return -1;
        }

        DBGPRINTF("param product:%f, param_product_max%d\n", pp,
                  pd_set->pc_max);

        // if fuzzy end (i.e. we bench at adjusted end point)
        if(nonzero_end == 1) {

            // stop when pp exceeds pc_max and take previous step. I.e. end
            // point will be <= pc_max
            if(pp > pd_set->pc_max) {
                step--;
                break;
            }
        }
        // if fuzzy_end is not set and end point will have zero speed
        else {
            
            // stop when pp exceeds or is equal to pc_max, this will now be
            // the end point and have zero speed (step-1 will be benchmark-able
            // and with a pp less than pc_max)
            if(pp >= pd_set->pc_max) {
                break;
            }
        }

        step++;
    }

    // if we never found a pp that exceeded pc_max, but ran out of steps, then
    // just keep the interval as it is ...
    if(ret < 0) {
        DBGPRINTF("Parameter product was not exceeded by interval.\n");
    }
    // otherwise lets set the end/start point to the value at the step
    else {
        if(start_pc > pd_set->pc_max) {
            ret = set_params_step_between_params(i->start,
                                      i->end, i->start, step, pd_set);
        }
        else {
            ret = set_params_step_between_params(i->end,
                                      i->start, i->end, step, pd_set);
        }
    }


    free(pc_max_params);
    pc_max_params = NULL;

    return 0;
#else
    (void)i;        //TODO unused
    (void)pd_set;

    ERRPRINTF("muParser not enabled at configure.\n");
    return -1;
#endif /* HAVE_MUPARSER */
}

/*!
 * Test if the end point of an interval is nonzero or not. A nonzero end point
 * means we will benchmark at this point rather than set its speed to be zero.
 * 
 * The line between start and end points may be perpendicular to some axes
 * of the parameter space, or to none at all. Any parameter-axis that the
 * interval-line is perpendicular to, is not considered in the determination of
 * the end point 'fuzziness'.
 *
 * The end point is declared nonzero then, if all parameters that may be
 * considered have nonzero property set to true. Other wise, the end point
 * is not fuzzy and should have a speed set to be zero in the model.
 *
 *
 * @param   i           pointer to the interval
 * @param   pd_set      pointer to the parameter defintion set
 * @param   n           number of parameters
 *
 * @return 0 if not zero end point, 1 if nonzero end point
 *
 */
int
isnonzero_at_interval_end(struct pmm_interval *i,
                          struct pmm_paramdef_set *pd_set)
{
    int j;
    int all_nonzero_end;

    all_nonzero_end = 1;

    for(j=0; j<pd_set->n_p; j++) {
        if(i->start[j] != i->end[j]) { // if interval is not perpendicular to
                                       // this axis

            if(pd_set->pd_array[j].nonzero_end != 1) {
                all_nonzero_end = 0;
                break;
            }

        }
    }

    return all_nonzero_end;
}

/*!
 * Assuming all points in the model are along a diagonal constructed by
 * GBBP from start_0, start_1, ..., start_n to end_0, end_1, ..., end_n, project
 * construction intervals through each diagonal point, along mutually
 * perpendicular lines.
 *
 * Or more formally, given a set n parameter definitions, describing start
 * and end values values:
 *      START = start_0, start_1, ..., start_n
 *      END = end_0, end_1, ..., end_n
 *
 * For each point p along a diagonal from a point (start_0,start_1,...,start_n)
 * to (end_0, end_1, ..., end_n)
 *    For each element p_i of a point p = (p_0, p_1, ..., p_n)
 *       find a point a by replacing p_i in the point p with start_i
 *       find a point b by replacing p_i in the point p with end_i
 *       create a construction interval between a and b
 *
 * @param   m   pointer to the model
 *
 * @return 0 on success, -1 on failure
 */
int
project_diagonal_intervals(struct pmm_model *m)
{

    int j;
    struct pmm_routine *r;
    struct pmm_benchmark *b;
    struct pmm_benchmark *zero_b;
    struct pmm_benchmark *zero_list_head, *prev_b;
    struct pmm_interval *new_i;

    r = m->parent_routine;
    zero_list_head = NULL;


    //all other relevant benchmarks will be contained in the bench list
    b = m->bench_list->first;
    while(b != NULL) {

        if(benchmark_on_axis(m, b) < 0) {
            // don't skip, bench is at origin or not on an axis

        }
        else {
            // bench is on axis, skip it
            do {
                b = get_next_different_bench(b);
            } while(b != NULL && b->flops == 0.0);

            continue;
        }

        for(j=0; j<m->n_p; j++) {

            new_i = new_projection_interval(b->p,
                                       &(m->parent_routine->pd_set->pd_array[j]),
                                       j, b->n_p);
            if(new_i == NULL) {
                ERRPRINTF("Error creating new projection interval.\n");
                return -1;
            }

            // if parameter product max is set, we adjust the new interval so
            // it satisfies the constraint
            if(r->pd_set->pc_max != -1) {

                adjust_interval_with_param_constraint_max(new_i, r->pd_set);
                                                      

            }
            if(r->pd_set->pc_min != -1) {
                adjust_interval_with_param_constraint_min(new_i, r->pd_set);
                                                       
            }


            if(is_interval_divisible(new_i, r) == 1) {
                DBGPRINTF("Adding diagonal projection interval.\n");
                print_interval(PMM_DBG, new_i);
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            if(r->pd_set->pd_array[j].nonzero_end == 1 && new_i != NULL)
            {
                //add an IT_POINT interval at the endpoint of the projected
                //interval ... careful re-use of new_i pointer!
                new_i = init_interval(0, b->n_p, IT_POINT, new_i->end, NULL);
                if(new_i == NULL) {
                    ERRPRINTF("Error creating new point interval.\n");
                    return -1;
                }

                DBGPRINTF("Adding nonzero end interval in diagonal "
                          "projection\n");

                print_interval(PMM_DBG, new_i);

                add_top_interval(m->interval_list, new_i);

            }
            else {
                // create a zero speed point at end ...
                //
                zero_b = init_zero_benchmark(b->p, b->n_p);
                if(zero_b == NULL) {
                    ERRPRINTF("Error allocating new benchmark.\n");
                    return -1;
                }

                //set parameter of the projection direction to end
                zero_b->p[j] = r->pd_set->pd_array[j].end;

                //in this phase of this projection of intervals, the model
                //must only contain benchmarks on the diagonal. So we will add
                //all zero benchmarks to a list which will be added to the
                //model later.
                if(zero_list_head == NULL) {
                    zero_list_head = zero_b;
                }
                else {
                    zero_list_head->next = zero_b;
                    zero_b->previous = zero_list_head;
                    zero_list_head = zero_b;
                }

            }
        }

        //for this to work, the model must contain only benchmarks that are
        //along the diagonal (it assumes all benchmarks are ones along the
        //diagonal that should be projected).

        //fetch the next benchmark with non-zero execution speed. Zero speed
        //benchmarks represent the bounding region of the parameter space
        //and we do not need to pay attention to these benchmarks
        //
        // TODO this will also work around our issue that inspired the zero_list
        // above ... remove zero_list at some stage
        do {
            b = get_next_different_bench(b);
        } while(b != NULL && b->flops == 0.0);
    }


    //now add zero benchmarks to the model
    zero_b = zero_list_head;
    while(zero_b != NULL) {
        prev_b = zero_b->previous;  //when zero_b is added to the list its
                                    //its next/prev pointers are modified

        DBGPRINTF("Inserting zero benchmark from saved list:\n");
        print_benchmark(PMM_DBG, zero_b);
        if(insert_bench(m, zero_b) < 0) {
            ERRPRINTF("Error inserting a zero benchmark.\n");

            free_benchmark(&zero_b);
            free_benchmark_list_backwards(&prev_b);

            return -1;
        }


        zero_b = prev_b;
    }


    return 0;
}

/*!
 * Create a new interval that is a projection through a point p, perpendicular
 * to the plane of the d-th element of the point, which is described by
 * the parameter defintion pd
 *
 * @param   p   pointer to the parameter array describing the point
 * @param   pd  pointer to the parameter definition relating to the 
 *              perpendicular plane
 * @param   d   index of the perpendicular plane
 * @param   n   number of elements in the point
 *
 * @return pointer to a new interval going from p_0,p_1, ... start_d, ... p_n
 * to p_0, p_1, .... end_d, ... p_n
 *
 */
struct pmm_interval*
new_projection_interval(int *p, struct pmm_paramdef *pd, int d, int n)
                                 
{
    struct pmm_interval *new_i;


    new_i = new_interval();
    if(new_i == NULL) {
        ERRPRINTF("Error creating new interval structure.\n");
        return NULL; //failure
    }

    new_i->type = IT_GBBP_EMPTY;
    new_i->n_p = n;

    new_i->start = init_param_array_copy(p, n);
    if(new_i->start == NULL) {
        ERRPRINTF("Error initialising interval start parameter.\n");
        free_interval(&new_i);
        return NULL;
    }
    new_i->start[d] = pd->start;

    new_i->end = init_param_array_copy(p, n);
    if(new_i->end == NULL) {
        ERRPRINTF("Error initialising interval end parameter.\n");
        free_interval(&new_i);
        return NULL;
    }
    new_i->end[d] = pd->end;

    return new_i;
}

/*!
 * multi_gbbp - builds multi-parameter piece-wise performance models using the
 * GBBP optimisation
 *
 * Alorithm description:
 *
 * init
 *   - push boundary_complete onto empty interval stack
 *   - for each parameter of the model
 *     - push boundary model of that parameter onto interval stack
 *
 * main
 *   - read interval from top of stack
 *   - if interval is of of a boundary type
 *     -  apply GBBP to the construction of the boundary model
 *   - if interval is tagged boundary_complete
 *     - mesh points on each boundary model to create interior benchmark points.
 *       Push each of these points onto the interval stack with a point_inteval
 *       tag.
 *     - pop the top point_interval and execute a benchmark on it
 *   - if interval is of the point_interval type
 *     - execute a benchmark on the point and add to model
 *   - if stack is empty
 *     - mark model construction as complete
 *
 *
 */
int*
multi_gbbp_select_new_bench(struct pmm_routine *r)
{
    int *params;

	struct pmm_interval_list *i_list;
	struct pmm_interval *top_i, *new_i;
    struct pmm_model *m;

    m = r->model;
	i_list = m->interval_list;

	//if model construction has not started, i_list will be empty
	if(isempty_interval_list(i_list) == 1) {

        DBGPRINTF("Interval list is empty, initializing new interval list.\n");

        if(init_gbbp_boundary_intervals(r) < 0) {
            ERRPRINTF("Error initialising new construction intervals.\n");
            return NULL;
        }
	}

    params = malloc(r->pd_set->n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    if((top_i = read_top_interval(i_list)) == NULL) {
        ERRPRINTF("Error reading interval from head of list\n");

        free(params);
        params = NULL;

        return NULL;
    }

    DBGPRINTF("Choosing benchmark based on following interval:\n");
    print_interval(PMM_DBG, top_i);

    switch(top_i->type) {
        case IT_GBBP_EMPTY :
        case IT_GBBP_CLIMB :
        case IT_GBBP_BISECT :
        case IT_GBBP_INFLECT :

            DBGPRINTF("Applying step of GBBP to benchmark selection.\n");

            //apply GBBP
            multi_gbbp_bench_from_interval(r, top_i, params);
            break;

        case IT_BOUNDARY_COMPLETE :

            DBGPRINTF("GBBP stage of construction complete. Meshing ...\n");

            //remove top interval
            remove_interval(i_list, top_i);

            if(r->pd_set->n_p > 1) {
                //mesh boundary models to create points
                mesh_boundary_models(m);

                //read another top interval
                if((top_i = read_top_interval(i_list)) == NULL) {
                    ERRPRINTF("Error reading interval from head of list\n");

                    free(params);
                    params = NULL;

                    return NULL;
                }

                DBGPRINTF("Meshing complete, fetching benchmark ...\n");

                //get the benchmark from the interval
                multi_gbbp_bench_from_interval(r, top_i, params);
            }
            else {
                ERRPRINTF("Invalid IT_BOUNDARY_COMPLETE in 1 param model.\n");

                // must check to see if interval list is empty now, if
                // completion is not tagged here then construction will restart
                if(isempty_interval_list(m->interval_list)) {
                    LOGPRINTF("GBBP Construction complete.\n");

                    new_i = new_interval();
                    new_i->type = IT_COMPLETE;
                    add_top_interval(m->interval_list, new_i);

                    m->complete = 1;
                }

                free(params);
                params = NULL;
            }

            break;

        case IT_POINT :

            DBGPRINTF("Benchmarking at point ...\n");
            //benchmark at this point
            multi_gbbp_bench_from_interval(r, top_i, params);
            break;

        default :
            ERRPRINTF("Invalid interval type: %s (%d)\n",
                       interval_type_to_string(top_i->type), top_i->type);
            print_interval(PMM_ERR, top_i);

            free(params);
            params = NULL;

            break;
    }

    return params;
}

/*!
 * intialize the interval stack for an empty model that will be built using a GBBP
 * algorithm
 *
 * @param   r   pointer to the routine which is having its intervals intiialized
 *
 * @return o on success, -1 on failure
 */
int
init_gbbp_boundary_intervals(struct pmm_routine *r)
{
    //TODO rename function
    int j;
    struct pmm_model *m;
    struct pmm_interval_list *i_list;
    struct pmm_interval *new_i;

    m = r->model;
    i_list = m->interval_list;

    //models with more than 1 boundary need a tag to indicate mesh stage
    //of construction
    if(r->pd_set->n_p > 1) {
        new_i = new_interval();
        new_i->type = IT_BOUNDARY_COMPLETE;
        add_top_interval(i_list, new_i);
    }

    // for each parameter of the routine
    for(j=0; j<r->pd_set->n_p; j++) {
        //push boundary model of that parameter onto the interval stack


        // if nonzero_end is set then we will benchmark at end and build
        // the model between this point and start. EMPTY interval will range
        // from start to end and a POINT interval at end will be added to
        // the top of the interval stack so that it is measured before GBBP
        // construction proceeds
        if(r->pd_set->pd_array[j].nonzero_end == 1) {

            DBGPRINTF("nonzero_end on param:%d\n", j);

            // add an empty interval from start -> end along boundary
            new_i = new_interval(); 
            new_i->type = IT_GBBP_EMPTY; 

            new_i->plane = j;
            new_i->n_p = r->pd_set->n_p;

            new_i->start = init_param_array_start(r->pd_set);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising start parameter array.\n");
                free_interval(&new_i);
                return -1;
            }

            new_i->end = init_param_array_start(r->pd_set);
            if(new_i->end == NULL) {
                ERRPRINTF("Error initialising start parameter array.\n");
                free_interval(&new_i);
                return -1;
            }
            new_i->end[j] = r->pd_set->pd_array[j].end;

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }


            // add benchmark at end on boundary (nonzero end)
            new_i = new_interval();
            new_i->type = IT_POINT;
            new_i->n_p = r->pd_set->n_p;

            new_i->start = init_param_array_start(r->pd_set);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising start parameter array.\n");
                free_interval(&new_i);
                return -1;
            }

            new_i->start[j] = r->pd_set->pd_array[j].end;

            add_top_interval(i_list, new_i);


        }
        else {
            // add an empty interval from start -> end along boundary
            new_i = new_interval(); 
            new_i->type = IT_GBBP_EMPTY; 

            new_i->plane = j;
            new_i->n_p = r->pd_set->n_p;

            new_i->start = init_param_array_start(r->pd_set);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising start parameter array.\n");
                free_interval(&new_i);
                return -1; //failure
            }

            new_i->end = init_param_array_start(r->pd_set);
            if(new_i->end == NULL) {
                ERRPRINTF("Error initialising start parameter array.\n");
                free_interval(&new_i);
                return -1; //failure
            }
            new_i->end[j] = r->pd_set->pd_array[j].end;

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

        }

    }

    return 0; //success
}

/*!
 * given a set of complete models along the parameter boundaries, creat a
 * mech of new benchmarking points for the interior of the model
 *
 * @param   m   pointer to the model
 */
void mesh_boundary_models(struct pmm_model *m)
{
    int *params;
    int n_p;
    int plane = 0;


    n_p = m->parent_routine->pd_set->n_p;

    //no need to build a mesh of boundary models when there is only 1 boundary!
    if(n_p == 1) {
        LOGPRINTF("1 parameter models have no mesh.\n");
        return;
    }

    params = malloc(n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        exit(EXIT_FAILURE);
    }

    recurse_mesh(m, params, plane, n_p);

    free(params);

    return;
}

//TODO prevent boundary benchmarks from being added as they are already
//measured
/*!
 * recurse through each plane of a model creating a mesh of
 * benchmark points using the completed parameter boundaries
 *
 * @param   m       pointer the model
 * @param   p       pointer to a parameter array
 * @param   plane   current plane
 * @param   n_p     number of parameters/planes
 */
void recurse_mesh(struct pmm_model *m, int *p, int plane, int n_p)
{

    /* TODO fix this to work without boundary models
    struct pmm_benchmark *b;
    struct pmm_interval *interval;

    if(plane<n_p) {

        b = m->boundary_model_array[plane].last;

        while(b != NULL) {

            if(b->flops <= 0) {
                b = get_previous_different_bench(b); //skip 0/-1 speed benches
                continue;
            }

            //skip benchmarks at the 'origin' of the model as meshing these only
            //create benchmarks along the boundaries which have already been
            //measured
            if(is_benchmark_at_origin(n_p, m->parent_routine->pd_set->pd_array,
                                      b) == 1)
            {
                b = get_previous_different_bench(b); //this should return a NULL
                continue;
            }

            p[plane] = b->p[plane];
            if(plane<n_p-1) {
                recurse_mesh(m, p, plane+1, n_p);
            }
            else {
                interval = init_interval(-1, n_p, IT_POINT, p, NULL);
                if(interval == NULL) {
                    ERRPRINTF("Error initialising interval.\n");
                    //TODO this error is not caught
                }

                add_top_interval(m->interval_list, interval);

                printf("plane: %d n_p:%d\n", plane, n_p);
                print_params(PMM_DBG, interval->start, n_p);
                print_params(PMM_DBG, b->p, n_p);
            }

            b = get_previous_different_bench(b);
        }
    }
    */

}

/*!
 * Insert a benchmark into a 1 parameter model being constructed with the
 * naive bisection method.
 *
 * @param   r   pointer to the routine forwhich the model is being constructed
 * @param   b   benchmark to insert
 *
 * @return 0 on success, -1 on failure to process intervals, -2 on failure
 * to insert benchmark
 */
int
naive_1d_bisect_insert_bench(struct pmm_routine *r, struct pmm_benchmark *b)
{

    double time_spend;
    int num_execs;
    int ret;
    struct pmm_interval *interval, *new_i;;
    int *midpoint;

    ret = 0;

    DBGPRINTF("benchmark params:\n");
    print_params(PMM_DBG, b->p, b->n_p);

    if(insert_bench(r->model, b) < 0) {
        ERRPRINTF("Error inserting benchmark.\n");
        ret = -2;
    }

    //first check time spent benchmarking at this point, if it is less than
    //the threshold, we won't bother processing the interval list (so that
    //the benchmark may be executed again, until it exceeds the threshold
    calc_bench_exec_stats(r->model, b->p, &time_spend, &num_execs);

    DBGPRINTF("total time spent benchmarking point: %f\n", time_spend);
    DBGPRINTF("total number executions at benchmarking point: %d\n", num_execs);

    if(check_benchmarking_minimums(r, time_spend, num_execs))
    {

        DBGPRINTF("benchmarking threshold exceeeded (t:%d, n:%d), processing intervals.\n", r->min_sample_time, r->min_sample_num);

        //find interval
        if(find_interval_matching_bench(r, b, NULL, &interval) < 0) {
            ERRPRINTF("Error searching intervals.\n");
            return -1;
        }

        if(interval != NULL) {
            if(interval->type == IT_GBBP_BISECT) {

                // find midpoint
                midpoint = malloc(interval->n_p * sizeof *midpoint);
                if(midpoint == NULL) {
                    ERRPRINTF("Error allocating memory.\n");
                    return -1;
                }
                set_params_interval_midpoint(midpoint, interval);


                // create new interval from start to midpoint
                new_i = init_interval(0, interval->n_p, IT_GBBP_BISECT,
                                      interval->start, midpoint);
                if(new_i == NULL) {
                    ERRPRINTF("Error initialising new interval.\n");
                    return -1;
                }

                // don't add it unless it is divisible
                if(is_interval_divisible(new_i, r) == 1) {
                    add_bottom_interval(r->model->interval_list, new_i);
                }
                else {
                    DBGPRINTF("Interval not divisible, not adding.\n");
                    print_interval(PMM_DBG, new_i);
                    free_interval(&new_i);
                }

                // create second new interval from midpoint to end
                new_i = init_interval(0, interval->n_p, IT_GBBP_BISECT,
                                      midpoint, interval->end);
                if(new_i == NULL) {
                    ERRPRINTF("Error initialising new interval.\n");
                    return -1;
                }

                // don't add it unless it is divisible
                if(is_interval_divisible(new_i, r) == 1) {
                    add_bottom_interval(r->model->interval_list, new_i);
                }
                else {
                    DBGPRINTF("Interval not divisible, not adding.\n");
                    print_interval(PMM_DBG, new_i);
                    free_interval(&new_i);
                }

                // remove the old interval
                remove_interval(r->model->interval_list, interval);

            }
            else if(interval->type == IT_POINT) {

                // remove old interval
                remove_interval(r->model->interval_list, interval);

            }
            else {
                ERRPRINTF("Unexpected interval type.\n");
                return -1;
            }

            if(isempty_interval_list(r->model->interval_list)) {
                DBGPRINTF("interval list now empty.\n");

                new_i = new_interval();
                new_i->type = IT_COMPLETE;
                add_top_interval(r->model->interval_list, new_i);

                r->model->complete = 1;
            }
        }

    } else {
        DBGPRINTF("benchmarking threshold not exceeded.\n");
    }


	// after intervals have been processed ...

    return ret;
}

/*!
 * Insert a benchmark into a multi-parameter model being constructed with the
 * GBBP method.
 *
 * The second half of the gbbp proceedure. After a benchmark has been made it
 * must be added to the model and the state of the building proceedure must be
 * adjusted according to the new shape of the model.
 *
 * The rules that govern this adjustment and the specific adjustments are as
 * follows:
 *
 * if the model is empty, we add the benchmark to the model and set the state
 * to be climbing.
 *
 * if the model is climbing we test the new benchmark to see if the model is
 * still climbing, or has levelled out, or has begun to decrease. If the
 * model is not still climbing or levelled out, we change the state to bisection.
 *
 * The bisection state permits the optimal selection of new benchmarking points.
 * In this state, any new benchmark being inserted is comparted to the existing
 * model. If the model already accurately approximates the benchmark the state
 * is set to inflection.
 *
 * The inflection state is a second level of bisection, in this state if a new
 * benchmark is again accurately approximated by the existing model we deem the
 * model to be complete in this region
 *
 * @return 0 on success, -1 on failure to process intervals, -2 on failure
 * to insert benchmark
 *
 * @param   h   pointer to the load history
 * @param   r   pointer to the routine to which the benchmark is added
 * @param   b   pointer to the benchmark to be added
 *
 * @return 0 onsucces, -1 on failure to process intervals, -2 on complete failure
 * to add benchmark to model
 */
int
multi_gbbp_insert_bench(struct pmm_loadhistory *h, struct pmm_routine *r,
                        struct pmm_benchmark *b)
{

    double time_spend;
    int num_execs;
    int ret;

    ret = 0;

    DBGPRINTF("benchmark params:\n");
    print_params(PMM_DBG, b->p, b->n_p);

    if(insert_bench(r->model, b) < 0) {
        ERRPRINTF("Error inserting benchmark.\n");
        ret = -2;
    }

    //first check time spent benchmarking at this point, if it is less than
    //the threshold, we won't bother processing the interval list (so that
    //the benchmark may be executed again, until it exceeds the threshold
    calc_bench_exec_stats(r->model, b->p, &time_spend, &num_execs);

    DBGPRINTF("total time spent benchmarking point: %f\n", time_spend);
    DBGPRINTF("total number executions at benchmarking point: %d\n", num_execs);

    if(check_benchmarking_minimums(r, time_spend, num_execs))
    {
        DBGPRINTF("benchmarking threshold exceeeded (t:%d, n:%d), processing intervals.\n", r->min_sample_time, r->min_sample_num);

        if(process_interval_list(r, b, h) < 0) {
            ERRPRINTF("Error proccessing intervals, inserting bench anyway\n");
            ret = -1;
        }

    } else {
        DBGPRINTF("benchmarking threshold not exceeded.\n");
    }


	// after intervals have been processed ...

    return ret;
}

/*!
 * Find the interval in an interval stack that corresponds to a given benchmark.
 *
 * This permits adding benchmarks to the model out of the order deemed by the
 * construction intervals.
 *
 * @param   r           pointer to routine containing model and parameter definitions
 * @param   b           pointer to benchmark whos interval we are searching for
 * @param   h           pointer to load history
 * @param   found_i     pointer to address that will hold the found interval or point
 *                      to NULL
 * 
 * @return 0 on successful search (found or not found), -1 on failure
 */
int
find_interval_matching_bench(struct pmm_routine *r, struct pmm_benchmark *b,
                             struct pmm_loadhistory *h,
                             struct pmm_interval **found_i)
{

    struct pmm_interval *i, *i_prev;
    int *temp_params;
    int done = 0, ret;

    (void)h; //TODO unused

    // set found to NULL incase we don't find anything and in case of error
    *found_i = NULL;

    i = r->model->interval_list->top;

    temp_params = malloc(r->pd_set->n_p * sizeof *temp_params);
    if(temp_params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1;
    }

    // interate over intervals
    while(i != NULL && !done) {

        i_prev = i->previous; //store this as the interval will be deallocated
                              //if it is processed by 'process_interval'


        // get the appropriate gbbp point from the interval
        ret = multi_gbbp_bench_from_interval(r, i, temp_params);
        if(ret < -1) {
            ERRPRINTF("Error getting benchmark from interval.\n");

            free(temp_params);
            temp_params = NULL;

            return -1;
        }

        // if a gbbp benchmark point was set by multi_gbbp_bench ....  then
        // check that the benchmark b is at the appropriate gbbp point
        if(ret == 0 && params_cmp(b->p, temp_params, b->n_p) == 0) {

            DBGPRINTF("Interval found:\n");
            print_interval(PMM_DBG, i);
            print_params(PMM_DBG, temp_params, r->pd_set->n_p);

            free(temp_params);
            temp_params = NULL;

            *found_i = i;

            return 0;

        }
        // benchmark is not at appropriate gbbp point
        else {
            // TODO try to reduce construction time by processing benchmarks
            // TODO that are not lying on appropriate GBBP points somehow.
        }

        // work backwards from the top/front of the list
        i = i_prev;
    }

    // if we got here, no interval was found

    free(temp_params);
    temp_params = NULL;

    return 0;
}

/*!
 * process the interval list, with a newly aquired benchmark
 * 
 * @param   r   pointer to routine
 * @param   b   pointer to benchmark
 * @param   h   load history
 *
 * @return 0 on successful processing of interval, -1 on failure
 */
int
process_interval_list(struct pmm_routine *r, struct pmm_benchmark *b,
                      struct pmm_loadhistory *h)
{

    struct pmm_interval *i, *i_prev;
    int *temp_params;
    int done = 0, ret;

    i = r->model->interval_list->top;

    // allocate a parameter array so we can compare interval benchmark points
    // to the benchmark that is being processed
    temp_params = malloc(r->pd_set->n_p * sizeof *temp_params);
    if(temp_params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1;
    }

    // interate over intervals
    while(i != NULL && !done) {

        i_prev = i->previous; //store this as the interval will be deallocated
                              //if it is processed by 'process_interval'


        // get the appropriate gbbp benchmarking point from the interval
        ret = multi_gbbp_bench_from_interval(r, i, temp_params);
        if(ret < -1) {
            ERRPRINTF("Error getting benchmark from interval.\n");

            free(temp_params);
            temp_params = NULL;

            return -1;
        }

        // if a gbbp benchmark point was set by multi_gbbp_bench ....  then
        // check that the benchmark b is at the appropriate gbbp point
        if(ret == 0 && params_cmp(b->p, temp_params, b->n_p) == 0) {

            DBGPRINTF("Interval found:\n");
            print_interval(PMM_DBG, i);
            print_params(PMM_DBG, temp_params, r->pd_set->n_p);

            done = process_interval(r, i, b, h);
            if(done < 0) {

                ERRPRINTF("Error processing interval.\n");

                free(temp_params);
                temp_params = NULL;

                return -1;

            }
        }
        // benchmark is not at appropriate gbbp point, no need to modify the
        // construction intervals
        else {
            // TODO try to reduce construction time by processing benchmarks
            // TODO that are not lying on appropriate GBBP points somehow.
        }

        // work backwards from the top/front of the list
        i = i_prev;
    }

    free(temp_params);
    temp_params = NULL;

    return 0;
}

/*!
 * Process an interval based on its type.
 *
 * @param   r   pointer to the routine who's model is being constructed
 * @param   i   pointer to the interval to process
 * @param   b   pointer to the benchmark that is being inserted to the model
 * @param   h   pointer to the load history defining performance fluctuation
 *
 * @return 1 if processing is successful, and no further intervals should be
 * processed, 0 if processing is successful but further intervals should be
 * processed, -1 if an error occurs.
 */
int
process_interval(struct pmm_routine *r, struct pmm_interval *i,
                 struct pmm_benchmark *b, struct pmm_loadhistory *h)
{
    int done;

    switch(i->type) {
        case IT_GBBP_EMPTY :

            // set return to 0 so calling loop will continue to search for
            // IT_GBBP_EMPTY intervals
            done = process_it_gbbp_empty(r, i) < 0 ? -1 : 0;

            break;

        case IT_GBBP_CLIMB :

            //if process_ returns failure (<0) set done to -1, otherwise 1
            done = process_it_gbbp_climb(r, i, b, h) < 0 ? -1 : 1;

            break;

        case IT_GBBP_BISECT :

            done = process_it_gbbp_bisect(r, i, b, h) < 0 ? -1 : 1;

            break;

        case IT_GBBP_INFLECT :

            done = process_it_gbbp_inflect(r, i, b, h) < 0 ? -1 : 1;

            break;

        case IT_POINT :
            done = process_it_point(r, i) < 0 ? -1 : 1;

            break;

        default :
            ERRPRINTF("Invalid interval type:%d\n", i->type);
            done = -1;
            break;

    }

    return done;
}

/*!
 * Process a gbbp_empty type interval. Replace interval with an IT_GBBP_CLIMB
 * interval that starts and ends at the same points as the GBBP_EMPTY interval
 * it replaces.
 *
 * Note, the new interval must be inserted inplace of the old one, not at the
 * top of the interval list. The start,start,...,start set of parameters match
 * all IT_GBBP_EMPTY intervals for all different boundary planes. We will
 * process all of these intervals, not just the first occurance in the interval
 * list.  However, not all of the planes are ready for construction (e.g. their
 * nonzero_end benchmark may not be executed yet). Inserting the next interval
 * (IT_GBBP_CLIMP) inplace of the IT_GBBP_EMPTY interval resolves this issue.
 *
 * @param   r   pointer to the parent routine
 * @param   i   pointer to the interval we are processing
 *
 * @pre i as the type IT_GBBP_EMPTY
 *
 * @return 0 on success
 */
int
process_it_gbbp_empty(struct pmm_routine *r, struct pmm_interval *i)
{

    i->type = IT_GBBP_CLIMB;

    //check interval is divisible and remove if not
    if(is_interval_divisible(i, r) != 1) {
        DBGPRINTF("Interval not divisible, removing.\n");
        remove_interval(r->model->interval_list, i);
    }

    return 0;
}

/*!
 * Process a IT_GBBP_CLIMB interval type and associated benchmark. If the
 * new benchmark has a higher speed than the benchmark at the start/left-most
 * point of the interval, then the performance is still climbing. Replace the
 * interval with one that ranges from the new benchmark point to the old
 * old interval end point. The new interval will have the same GBBP_CLIMB type.
 *    Otherwise, if the performance is no longer climbing, replace the old
 * interval with a new interval ranging from the new benchmark point to the
 * end of the old interval, but with a GBBP_BISECT type.
 *
 * @param   r           pointer to the routine that cotains the model
 * @param   i           pointer to the interval that the benchmark belongs to
 * @param   b           pointer to the benchmark
 * @param   h           pointer to the load history (describes performance
 *                      variance)
 *
 * @pre interval i and benchmark b match, m is the correct model, etc.
 *
 * @return 0 on success, -1 on failure
 */
int
process_it_gbbp_climb(struct pmm_routine *r, struct pmm_interval *i,
                      struct pmm_benchmark *b, struct pmm_loadhistory *h)

{
    struct pmm_model *m;
    struct pmm_interval *new_i;
    struct pmm_benchmark *b_avg, *b_left_0, *b_left_1, *b_left_2;
    int *tmp_params;

    m = r->model;


    //if we have not had more than 2 climb steps there will not be enough
    //points in the model to test for end of climbing ...
    if(i->climb_step < 2) {
        //not enough points
        DBGPRINTF("not enough points to test climbing, keep climbing...\n");

        i->climb_step = i->climb_step+1;

        //check interval is divisible and remove if not
        if(is_interval_divisible(i, r) != 1) {
            DBGPRINTF("Interval not divisible, removing.\n");
            remove_interval(m->interval_list, i);
        }

    }
    else {
        //get the average benchmark at the model where b is positioned
        b_avg = get_avg_bench(m, b->p);
        if(b_avg == NULL) {
            ERRPRINTF("No benchmark found for current point.\n");
            return -1;
        }

        
        //need to create a param array to look up model for
        //starting points of intervals (on boundaries)
        tmp_params = malloc(b->n_p * sizeof *tmp_params);
        if(tmp_params == NULL) {
            ERRPRINTF("Error allocating parameter array.\n");
            return -1;
        }

        // get avg of benchmarks to the left of the new benchmark, set params
        // to the current climb step ...
        set_params_step_along_climb_interval(tmp_params, i->climb_step, i,
                                             r->pd_set);
        b_left_0 = get_avg_bench(m, tmp_params);

        //get parameters for previous climb step to current
        set_params_step_along_climb_interval(tmp_params, i->climb_step-1, i,
                                             r->pd_set);
        b_left_1 = get_avg_bench(m, tmp_params);

        //get parameters for 2nd previous climb step to current
        set_params_step_along_climb_interval(tmp_params, i->climb_step-2, i,
                                             r->pd_set);
        b_left_2 = get_avg_bench(m, tmp_params);

        if(b_left_0 == NULL || b_left_1 == NULL || b_left_2 == NULL) {
            ERRPRINTF("Error, no benchmark @ a previous climb point\n");

            free_benchmark(&b_avg);

            if(b_left_0 != NULL)
                free_benchmark(&b_left_0);
            if(b_left_1 != NULL)
                free_benchmark(&b_left_1);
            if(b_left_2 != NULL)
                free_benchmark(&b_left_2);

            free(tmp_params);
            tmp_params = NULL;

            return -1;
        }

        DBGPRINTF("b_left_0:\n");
        print_benchmark(PMM_DBG, b_left_0);
        DBGPRINTF("b_left_1\n");
        print_benchmark(PMM_DBG, b_left_1);
        DBGPRINTF("b_left_2:\n");
        print_benchmark(PMM_DBG, b_left_2);


        // if the 3 previous benchmarks along the climb interval
        // (b_left_0 -> b_left_2) are each successively less-than OR
        // equal to their previous neighbour, then end the CLIMB stage of
        // construction
        //
        // i.e. if the performance has been decreasing consistantly for the last
        // three benchmarks ...
        if((bench_cut_greater(h, b_left_0, b_avg) ||
                                  bench_cut_contains(h, b_left_0, b_avg)) &&
           (bench_cut_greater(h, b_left_1, b_left_0) ||
                                  bench_cut_contains(h, b_left_1, b_left_0)) &&
           (bench_cut_greater(h, b_left_2, b_left_1) ||
                                  bench_cut_contains(h, b_left_2, b_left_1)))
        {
            DBGPRINTF("performance no longer climbing\n");
            DBGPRINTF("previous bench speeds:\n");
            DBGPRINTF("%f@%d %f@%d %f@%d\n",
                      b_left_0->flops, b_left_0->p[i->plane],
                      b_left_1->flops, b_left_1->p[i->plane],
                      b_left_2->flops, b_left_2->p[i->plane]);

            DBGPRINTF("this bench:\n");
            print_benchmark(PMM_DBG, b_avg);

            // setup new interval, add and remove old
            new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                                  b->p, i->end);
            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, i);
                free_interval(&new_i);
            }

            remove_interval(m->interval_list, i);
        }
        // otherwise if the benchmark b is greater than any of the 3 previous
        // benchmarks, continue with the CLIMB phase of construction,
        // increment step
        else {
            DBGPRINTF("performance still climbing\n");
            DBGPRINTF("previous bench speeds:\n");
            DBGPRINTF("%f@%d %f@%d %f@%d\n",
                    b_left_0->flops, b_left_0->p[i->plane],
                    b_left_1->flops, b_left_1->p[i->plane],
                    b_left_2->flops, b_left_2->p[i->plane]);
            DBGPRINTF("this bench:\n");
            print_benchmark(PMM_DBG, b_avg);

            i->climb_step = i->climb_step+1;

            //check interval is divisible and remove if not
            if(is_interval_divisible(i, r) != 1) {
                DBGPRINTF("Interval not divisible, removing.\n");
                remove_interval(m->interval_list, i);
            }
        }

        free_benchmark(&b_avg);
        free_benchmark(&b_left_0);
        free_benchmark(&b_left_1);
        free_benchmark(&b_left_2);

        free(tmp_params);
        tmp_params = NULL;
    }


    if(isempty_interval_list(m->interval_list)) {
        DBGPRINTF("interval list now empty.\n");

        new_i = new_interval();
        new_i->type = IT_COMPLETE;
        add_top_interval(m->interval_list, new_i);

        m->complete = 1;
    }

    return 0;
}

/*!
 * Step along the climb interval, from the start point towards the end point, 
 * forwards or backwards, a number of times.
 *
 * @param   params      pointer to an array to store the parameters at the n-th
 *                      step along the interval
 * @param   step        number of steps to take along the interval (- to step
 *                      backwards, + to step forwards)
 * @param   i           pointer to the interval to step along
 * @param   pd_array    pointer to the parameter definition array
 * @param   n_p         number of parameters
 *
 * @pre interval should have start and end points set (i.e. be of the type
 * IT_EMTPY, IT_GBBP_CLIMB, IT_GBBP_BISECT, IT_GBBP_INFLECT) though the
 * function is only intended to operate on IT_GBBP_CLIMB interval types
 *
 * @return 0 on success, -1 if the step will exceed the end-point of the
 * interval or -2 on error
 */
int
set_params_step_along_climb_interval(int *params, int step, 
                             struct pmm_interval *i,
                             struct pmm_paramdef_set *pd_set)
{


    return set_params_step_between_params(params,i->start, i->end, step,
                                          pd_set);

}

/*!
 * process a GBBP_BISECT interval
 *
 * test if the model approximates the new benchmark and in what way,
 *
 * If the benchmark at the same level as the its left and right neighbours:
 * assume the model is flat in the region of the interaval and finalized this
 * area of the model by removing it.
 *
 * If the benchmark is at the same level as the left neighbour only, assume that
 * the model is flat to the left of the new benchmark, finalize this area by
 * remoing the interval and replacing it with a new interval representing the
 * area to the right of the benchmark.
 *
 * In the benchmark is at thte same level as the right neighbour only, preform
 * the same action, but in the inverse, replace the interval with a new interval
 * representing the area to the left of the benchmark
 *
 * In other cases, look up the current model at the position of the new
 * benchmark.
 *
 * If the current model accurately represents the new benchmark, replace the
 * construction interval with a new GBBP_INFLECT interval covering the same
 * area.
 *
 * If the current model does not accurately represent the new benchmark,
 * replace the interval with two new GGBP_BISECT intervals covering the left
 * and right areas between the new benchmark and the previous interval,
 * allowing further refinement of the model
 *
 *
 * @param   r   pointer to the routine
 * @param   i   pointer to the interval being processed
 * @param   b   pointer to the new benchmark
 * @param   h   pointer to the load history
 *
 * @return 0 on success, -1 on failure
 */
int
process_it_gbbp_bisect(struct pmm_routine *r, struct pmm_interval *i,
                       struct pmm_benchmark *b, struct pmm_loadhistory *h)
{

    struct pmm_interval *new_i;
    struct pmm_model *m;
    struct pmm_benchmark *b_avg, *b_oldapprox, *b_left, *b_right;
    int *midpoint;
    int intersects_left, intersects_right;

    m = r->model;


    //get the average benchmark at the model where b is positioned
    b_avg = get_avg_bench(m, b->p);


    //find the unaligned midpoint of the interval
    midpoint = malloc(i->n_p * sizeof *midpoint);
    if(midpoint == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        free_benchmark(&b_avg);
        return -1;
    }
    set_params_interval_midpoint(midpoint, i);

    // look up benchmarks at aligned interval->start
    b_left = get_avg_aligned_bench(m, i->start);
    if(b_left == NULL) {
        ERRPRINTF("Error, no benchmark @ interval start\n");

        free(midpoint);
        midpoint = NULL;

        free_benchmark(&b_avg);
        return -1;
    }

    // look up benchmark at aligned interval->end
    b_right = get_avg_aligned_bench(m, i->end);
    if(b_right == NULL) {
        ERRPRINTF("Error, no benchmark @ interval end\n");
        free_benchmark(&b_left);

        free(midpoint);
        midpoint = NULL;

        free_benchmark(&b_avg);
        return -1;
    }


    //
    intersects_left = bench_cut_intersects(h, b_left, b_avg);
    intersects_right = bench_cut_intersects(h, b_right, b_avg);

    if(intersects_left == 1 && intersects_right == 1) {
        // new benchmark cut intersets l.h.s. and r.h.s.
        // of previous model approximation, we are finished
        // building from range b_left to b_right so
        // remove the old construction interval and do
        // not replace it with anything

        DBGPRINTF("finished building from:\n");
        print_params(PMM_DBG, i->start, i->n_p);
        DBGPRINTF("to:\n");
        print_params(PMM_DBG, i->end, i->n_p);

        remove_interval(m->interval_list, i);
    }
    else if(intersects_left == 1 && intersects_right != 1) {
        //
        // new bench cut intersects only l.h.s. of previous
        // model approximation, we are finished building in
        // range b_left to b, so we remove old construction
        // interval and replace it with one ranging from b
        // to b_right OR midpoint to interval end
        //
        new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                              midpoint, i->end);
        if(new_i == NULL) {
            ERRPRINTF("Error initialising new interval.\n");
            return -1;
        }

        if(is_interval_divisible(new_i, r) == 1) {
            add_top_interval(m->interval_list, new_i);
        }
        else {
            DBGPRINTF("Interval not divisible, not adding.\n");
            print_interval(PMM_DBG, new_i);
            free_interval(&new_i);
        }

        DBGPRINTF("finished building from:\n");
        print_params(PMM_DBG, i->start, i->n_p);
        DBGPRINTF("to:\n");
        print_params(PMM_DBG, midpoint, i->n_p);

        remove_interval(m->interval_list, i);

    }
    else if(intersects_right == 1 && intersects_left != 1) {
        //
        // intersects only r.h.s. of old approximation we
        // are finished building in the range b to b_right, so
        // remove old construction interval and replace it with
        // one ranging from b_left to b OR interval start to
        // midpoint
        //
        new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                              i->start, midpoint);
        if(new_i == NULL) {
            ERRPRINTF("Error initialising new interval.\n");
            return -1;
        }

        if(is_interval_divisible(new_i, r) == 1) {
            add_top_interval(m->interval_list, new_i);
        }
        else {
            DBGPRINTF("Interval not divisible, not adding.\n");
            print_interval(PMM_DBG, new_i);
            free_interval(&new_i);
        }

        DBGPRINTF("finished building from:\n");
        print_params(PMM_DBG, midpoint, i->n_p);
        DBGPRINTF("to:\n");
        print_params(PMM_DBG, i->end, i->n_p);

        remove_interval(m->interval_list, i);
    }
    else {
        //intersets neither r.h.s. or l.h.s. of old approx
        DBGPRINTF("intersects neither left or right of"
                " interval, finding old approximation.\n");

        // lookup the existing model at new benchmark size
        b_oldapprox = find_oldapprox(r->model, b->p);
        if(b_oldapprox == NULL) {
            ERRPRINTF("Error finding old approximation.\n");
            return -1;
        }

        if(bench_cut_intersects(h, b_oldapprox, b_avg)) {
            //
            // insersects old approximation, change to
            // recurisve bisect with INFLECT tag, create
            // new intervals from b_left to b and b to b_right
            // OR start to midpoint and midpoint to end
            //

            DBGPRINTF("new bench intersects old approx.\n");

            new_i = init_interval(i->plane, i->n_p, IT_GBBP_INFLECT,
                                  i->start, midpoint);
            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            new_i = init_interval(i->plane, i->n_p, IT_GBBP_INFLECT,
                                  midpoint, i->end);
            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            remove_interval(m->interval_list, i);
        }
        else {
            //
            // doesn't intersect old approx, l.h.s or r.h.s. replace
            // interval with two further BISECT intervals from
            // b_left to b and b to b_right OR start to midpoint and
            // midpoint to end
            //

            DBGPRINTF("does not intersect left/right or "
                    "old approximation, splitting...\n");

            new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                                  i->start, midpoint);
            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                                  midpoint, i->end);
            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            remove_interval(m->interval_list, i);
        }

        if(b_oldapprox != NULL) {
            free_benchmark(&b_oldapprox);
        }
    }

    free_benchmark(&b_avg);

    free(midpoint);
    midpoint = NULL;

    if(b_left != NULL) {
        free_benchmark(&b_left);
    }
    if(b_right != NULL) {
        free_benchmark(&b_right);
    }

    if(isempty_interval_list(m->interval_list)) {
        DBGPRINTF("interval list now empty.\n");

        new_i = new_interval();
        new_i->type = IT_COMPLETE;
        add_top_interval(m->interval_list, new_i);

        m->complete = 1;
    }

    return 0;
}

/*!
 * process a GBBP_INFLECT interval
 *
 * test if the model approximates the new benchmark and in what way,
 *
 * Look up the current model at the position of the new benchmark, if the model
 * accurately represents the new benchmark then the model is complete in this area,
 * remove the construction interval.
 *
 * Otherwise, construction must contine in this area, but:
 *
 * If the benchmark is at the same level as the left neighbour only, assume that
 * the model is flat to the left of the new benchmark, finalize this area by
 * remoing the interval and replacing it with a new BISECT interval representing the
 * area to the right of the benchmark.
 *
 * In the benchmark is at thte same level as the right neighbour only, preform
 * the same action, but in the inverse, replace the interval with a new interval
 * representing the area to the left of the benchmark
 *
 * Otherwise replace the interval with two new GGBP_BISECT intervals covering
 * the left and right areas between the new benchmark and the previous
 * interval, allowing further refinement of the model.
 *
 *
 * @param   r   pointer to the routine
 * @param   i   pointer to the interval being processed
 * @param   b   pointer to the new benchmark
 * @param   h   pointer to the load history
 *
 * @return 0 on success, -1 on failure
 */
int
process_it_gbbp_inflect(struct pmm_routine *r, struct pmm_interval *i,
                        struct pmm_benchmark *b, struct pmm_loadhistory *h)
{

    struct pmm_interval *new_i;
    struct pmm_model *m;
    struct pmm_benchmark *b_avg, *b_left, *b_right, *b_oldapprox;
    int *midpoint;
    int intersects_left, intersects_right;

    m = r->model;


    //get the average benchmark at the model where b is positioned
    b_avg = get_avg_bench(m, b->p);

    // lookup the model's approximation of b, with no benchmarks at b
    // contributing
    b_oldapprox = find_oldapprox(r->model, b->p);
    if(b_oldapprox == NULL) {
        ERRPRINTF("Error finding old approximation.\n");
        free_benchmark(&b_avg);
        return -1;
    }

    //find the unaligned midpoint of the interval, we only use this for
    //creating the next intervals
    midpoint = malloc(i->n_p * sizeof *midpoint);
    if(midpoint == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        free_benchmark(&b_oldapprox);
        free_benchmark(&b_avg);
        return -1;
    }
    set_params_interval_midpoint(midpoint, i);

    // look up benchmarks at aligned interval->start
    b_left = get_avg_aligned_bench(m, i->start);
    if(b_left == NULL) {
        ERRPRINTF("Error, no benchmark @ interval start\n");

        free(midpoint);
        midpoint = NULL;

        free_benchmark(&b_oldapprox);
        free_benchmark(&b_avg);
        return -1;
    }

    // look up benchmark at aligned interval->end
    b_right = get_avg_aligned_bench(r->model, i->end);
    if(b_right == NULL) {
        ERRPRINTF("Error, no benchmark @ interval end\n");
        free_benchmark(&b_left);

        free(midpoint);
        midpoint = NULL;

        free_benchmark(&b_oldapprox);
        free_benchmark(&b_avg);
        return -1;
    }


    if(bench_cut_intersects(h, b_oldapprox, b_avg)) {
        //
        // we are on a straight line section of the model
        // no need to add new construction intervals, can
        // finalise in the region of the current interval
        //
        remove_interval(m->interval_list, i);
    }
    else {
        intersects_left = bench_cut_intersects(h, b_left, b_avg);
        intersects_right = bench_cut_intersects(h, b_right, b_avg);

        // this should never happen, as it would mean model already
        // accurately approximated the benchmark above
        if(intersects_left == 1 && intersects_right == 1) {
            remove_interval(m->interval_list, i);
        }
        else if(intersects_left == 1) {
            //
            // new bench cut intersects l.h.s. of previous
            // model approx, we are finished building in
            // the range b_left to b, so we remove the old
            // construction interval and replace it with one
            // ranging from b to b_right OR interval midpoint
            // to interval end.
            //
            new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                                  midpoint, i->end);
            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                free_benchmark(&b_right);
                free_benchmark(&b_left);

                free(midpoint);
                midpoint = NULL;

                free_benchmark(&b_oldapprox);
                free_benchmark(&b_avg);
                return -1;
            }
            
            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            remove_interval(m->interval_list, i);
        }
        else if(intersects_right == 1) {
            // intersects r.h.s. of old approximation, replace
            // with interval start to interval midpoint OR
            // b_left to b
            new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                                  i->start, midpoint);

            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                free_benchmark(&b_right);
                free_benchmark(&b_left);

                free(midpoint);
                midpoint = NULL;

                free_benchmark(&b_oldapprox);
                free_benchmark(&b_avg);
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            remove_interval(m->interval_list, i);
        }
        else {
            // intersects neither r.h.s. or l.h.s., replace with
            // intervals of BISECT type from interval start to interval
            // midpoint, and interval midpoint to interval end. OR
            // b_left to b, and b to b_right.
            new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                                  i->start, midpoint);

            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                free_benchmark(&b_right);
                free_benchmark(&b_left);

                free(midpoint);
                midpoint = NULL;

                free_benchmark(&b_oldapprox);
                free_benchmark(&b_avg);
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                                  midpoint, i->end);

            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                free_benchmark(&b_right);
                free_benchmark(&b_left);

                free(midpoint);
                midpoint = NULL;

                free_benchmark(&b_oldapprox);
                free_benchmark(&b_avg);
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                DBGPRINTF("Interval not divisible, not adding.\n");
                print_interval(PMM_DBG, new_i);
                free_interval(&new_i);
            }

            remove_interval(m->interval_list, i);
        }
    }

    free(midpoint);
    midpoint = NULL;

    free_benchmark(&b_avg);

    if(b_oldapprox != NULL) {
        free_benchmark(&b_oldapprox);
    }
    if(b_left != NULL) {
        free_benchmark(&b_left);
    }
    if(b_right != NULL) {
        free_benchmark(&b_right);
    }

    if(isempty_interval_list(m->interval_list)) {
        DBGPRINTF("interval list now empty.\n");

        new_i = new_interval();
        new_i->type = IT_COMPLETE;
        add_top_interval(m->interval_list, new_i);

        m->complete = 1;
    }

    return 0;
}

/*!
 * Process a point type interval. Remove interval from the interval list
 * and if the list is now empty, add a IT_COMPLETE interval to the list to
 * tag that construction has been completed, also set the complete parameter
 * of the model to 1
 *
 * @param   r   pointer to the routine the benchmark belongs to
 * @param   i   pointer to the interval
 *
 * @pre interval i has a type IT_POINT
 *
 * @return 0 on success, -1 on failure
 */
int
process_it_point(struct pmm_routine *r, struct pmm_interval *i)
{
    struct pmm_interval *new_i;
    struct pmm_model *m;

    m = r->model;

    remove_interval(m->interval_list, i);

    if(isempty_interval_list(m->interval_list)) {
        DBGPRINTF("interval list now empty.\n");

        new_i = new_interval();
        new_i->type = IT_COMPLETE;
        add_top_interval(m->interval_list, new_i);

        m->complete = 1;
    }

    return 0;
}

/*!
 * Tests if an interval can be processed to return a benchmark from it. I.e.
 * if the 'length' of the interval is enough for a new benchmark point to
 * be returned when GBBP is applied to that interval.
 *
 * This is achieved by applying GBBP to the interval, then aligning the start
 * and end points of the interval and comparing them to the GBBP point.
 *
 * If the GBBP point is equal to either the aligned start/end points, we
 * determine that the interval is not divisible.
 *
 * @param   i   pointer to the interval to test
 * @param   r   pointer to the routine that the interval belongs to
 *
 * @return 1 if interval is divisible, 0 if it is not or -1 on error
 *
 * @pre interval is of type IT_GBBP_CLIMB, IT_GBBP_BISECT or IT_GBBP_INFLECT
 *
 * TODO 'divisible' is the wrong adjective to describe this function
 */
int
is_interval_divisible(struct pmm_interval *i, struct pmm_routine *r)
{
    int *params;
    int *start_aligned, *end_aligned;


    switch (i->type) {
        case IT_GBBP_EMPTY:
            return 1;
        case IT_GBBP_CLIMB:
        case IT_GBBP_BISECT:
        case IT_GBBP_INFLECT:

            // allocate/init some memory
            params = malloc(r->pd_set->n_p * sizeof *params);
            start_aligned = init_param_array_copy(i->start, r->pd_set->n_p);
            end_aligned = init_param_array_copy(i->end, r->pd_set->n_p);
            if(params == NULL || start_aligned == NULL || end_aligned == NULL) {
                ERRPRINTF("Error allocating memory.\n");
                return -1;
            }

            //align start/end
            align_params(start_aligned, r->pd_set);
            align_params(end_aligned, r->pd_set);
            
            // find the GBBP point of the interval
            if(multi_gbbp_bench_from_interval(r, i, params) < 0) {
                ERRPRINTF("Error getting GBBP bench point from interval.\n");

                free(start_aligned);
                start_aligned = NULL;
                free(end_aligned);
                end_aligned = NULL;
                free(params);
                params = NULL;

                return -1;
            }

            // TODO maybe test if the point is > start and < end

            // test that point is not equal to aligned start or end of interval
            if(params_cmp(start_aligned, params, r->pd_set->n_p) == 0 ||
               params_cmp(end_aligned, params, r->pd_set->n_p) == 0)
            {
                free(start_aligned);
                start_aligned = NULL;
                free(end_aligned);
                end_aligned = NULL;
                free(params);
                params = NULL;

                return 0; //not divisible
            }
            else {
                free(start_aligned);
                start_aligned = NULL;
                free(end_aligned);
                end_aligned = NULL;
                free(params);
                params = NULL;

                return 1; //divisible
            }

            break;

        case IT_BOUNDARY_COMPLETE:
        case IT_COMPLETE:
        case IT_POINT:
        default:
            return 0;
            break;
    }
}

/*!
 * Finds the parameters of the next benchmarking point based on a
 * parameter interval and the GBBP algorithm.
 *
 * The benchmarking point is aligned to the stride/offset of parameter
 * definitions.
 *
 * @param   r           pointer to the relevant routine
 * @param   interval    pointer to the interval
 * @param   params      pointer to an array where the next point should be
 *                      stored
 *
 * @return 0 on success, -1 if no benchmark is associated with the interval
 * type (i.e. IT_COMPLETE) or -2 on an error
 *
 * @pre params is a pointer to allocated memory of the correct size
 */
int
multi_gbbp_bench_from_interval(struct pmm_routine *r,
		                       struct pmm_interval *interval,
                               int *params)
{

    //depending on interval type, set the param of the boundary plane
    switch (interval->type) {
        case IT_GBBP_EMPTY : // empty, return the interval's start

            DBGPRINTF("assigning start problem size.\n");

            set_param_array_copy(params, interval->start, r->pd_set->n_p);

            return 0;

        case IT_GBBP_CLIMB : // climbing, return increment on interval's start

            DBGPRINTF("assigning increment on interval start point.\n");


            set_params_step_along_climb_interval(params, interval->climb_step+1,
                                           interval, r->pd_set);

            align_params(params, r->pd_set);

            return 0;

        case IT_GBBP_BISECT : // interval bisect, return midpoint
        case IT_GBBP_INFLECT : // interval inflect, return midpoint

            //set params to be midpoint between start and end of interval
            set_params_interval_midpoint(params, interval);

            //align midpoint
            align_params(params, r->pd_set);

            DBGPRINTF("assigned midpoint of interval from start:\n");
            print_params(PMM_DBG, interval->start, interval->n_p);
            DBGPRINTF("to end:\n");
            print_params(PMM_DBG, interval->end, interval->n_p);
            DBGPRINTF("at:\n");
            print_params(PMM_DBG, params, interval->n_p);

            return 0;

       case IT_POINT : // 'point' type interval from mesh, return copied point

            DBGPRINTF("assigning copy of interval 'point'.\n");

            set_param_array_copy(params, interval->start, r->pd_set->n_p);

            return 0;

       case IT_COMPLETE :
       case IT_NULL :
       case IT_BOUNDARY_COMPLETE :
            return -1;

       default :
            ERRPRINTF("Invalid interval type: %s\n",
                      interval_type_to_string(interval->type));
            return -2;
    }
}

/*!
 * Sets a param array to the midpoint between start and end points of an
 * interval.
 *
 * Precision is limited to the integer type of the point definition, round
 * up is carried out on the division.
 *
 * @param   p   pointer to the parameter array
 * @param   i   pointer to the interval
 *
 * @pre p is allocated and i is an interval of the correct type, with start
 * and end points set (IT_BISECT, IT_INFLECT)
 */
void
set_params_interval_midpoint(int *p, struct pmm_interval *i)
{
    int j;

    DBGPRINTF("i->n_p:%d\n", i->n_p);

    for(j=0; j<i->n_p; j++) {
        //integer division by two with round up
        //round of x/y = (x+y/2)/y ...
        p[j] = ((i->start[j]+i->end[j])+1)/2;
    }

    return;
}

