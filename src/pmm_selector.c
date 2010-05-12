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

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>


#include "pmm_data.h"
#include "pmm_selector.h"
#include "pmm_log.h"

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
long long int gbbp_select_new_bench(struct pmm_routine *r) {

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
long long int*
multi_naive_select_new_bench(struct pmm_routine *r)
{
    long long int *params;

    struct pmm_interval_list *i_list;
    struct pmm_interval *new_i, *top_i;
    struct pmm_model *m;


    m = r->model;
    i_list = m->interval_list;

    // if interval list is empty initialise with a point interval at the min
    // parameter point
    if(isempty_interval_list(i_list) == 1) {
        DBGPRINTF("Interval list is empty, initialising new interval list.\n");

        new_i = new_interval();
        new_i->type = IT_POINT;
        new_i->n_p = r->n_p;
        new_i->start = init_param_array_min(r->paramdef_array, r->n_p);

        if(new_i->start == NULL) {
            ERRPRINTF("Error initialising minimum parameter array.\n");
            return NULL;
        }

        add_top_interval(i_list, new_i);

    }

    
    if((top_i = read_top_interval(i_list)) == NULL) {
        ERRPRINTF("Error reading interval from head of list\n");
        return NULL;
    }

    DBGPRINTF("Choosing benchmark based on following interval:\n");
    print_interval(top_i);

    switch(top_i->type) {
        case IT_POINT :
        
            DBGPRINTF("assigning copy of interval 'point'.\n");

            params = init_param_array_copy(top_i->start, top_i->n_p);
            if(params == NULL) {
                ERRPRINTF("Error copying interval parameter point.\n");
                return NULL;
            }

            align_params(params, r->paramdef_array, r->n_p);

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
    //print_params(b->p, b->n_p);

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

    if(time_spend >= (double)r->min_sample_time ||
       num_execs >= r->min_sample_num)
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

/*
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
 * our parameter definitions in terms of minimum values, maximum values, stride
 * and offsets.
 *
 * We iterate through such a set of points in lexicographical order, i.e.
 * given points a and b in P, they are successive iff:
 *
 *  (a_1, a_2, \dots, a_n) <^d (b_1,b_2, \dots, b_n) \iff
 *  (\exists\ m > 0) \ (\forall\ i < m) (a_i = b_i) \land (a_m <_m b_m) 
 *
 * Given a point p, incrementing it to p' involves the following: Increment
 * the first term, if this incremented value is greater than the max defined
 * for the first term (i.e. it has overflowed), set the term to the minimum
 * value and equal min_1 and apply this overflow by incrementing the next
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
    long long int *aligned_params;
    int i;


    m = r->model;
    interval = m->interval_list->top;

    aligned_params = init_aligned_params(interval->start, r->paramdef_array,
                                         interval->n_p);
    if(aligned_params == NULL) {
        ERRPRINTF("Error initialising aligned param array.\n");
        return -1;
    }

    

    //if the benchmark parameters do not match ...
    if(params_cmp(b->p, aligned_params, b->n_p) != 0) {
        LOGPRINTF("unexpected benchmark ... expected:\n");
        print_params(interval->start, interval->n_p);
        LOGPRINTF("got:\n");
        print_params(b->p, b->n_p);

        return 0; //no error
    }

    // step the parameters the interval describes along by 'stride'
    //
    // iterate over all parameters
    for(i=0; i<interval->n_p; i++) {
        // if incrementing a parameter does not cause an overflow
        if(interval->start[i]+r->paramdef_array[i].stride <=
           r->paramdef_array[i].max)
        {
            // increment the parameter as tested and finish
            interval->start[i] += r->paramdef_array[i].stride;
            break;
        }
        else {
            //otherwise overflow would occur so set this parameter to
            //it's minimum value and allow the overflow to cascade into the
            //next parameter by continuing through the loop (and trying to
            //iterate the next parameter)
            interval->start[i] = r->paramdef_array[i].min;
        }
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
            print_interval_list(m->interval_list);
            return -1;
        }
    }

    return 0;
}

/*
 * multi_random_select - builds a multi-parameter piece-wise performance model
 * using a random selection of benchmark points
 *
 * Alogirthm description:
 *
 * if model is empty
 *   select minimum values for all parameters and return benchmark point
 * else
 *   seed random generator
 *
 *   for each parameter
 *     select a random parameter size based on the paramdef limits and return
 *
 */
long long int* multi_random_select_new_bench(struct pmm_routine *r)
{
    long long int *params;
    int i;


	//allocate parameter return array
    params = malloc(r->n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
		return NULL;
	}



	//if model is empty
	if(isempty_model(r->model)) {

		//set paremeter reutrn array to the origin point
		for(i=0; i<r->n_p; i++) {
			params[i] = r->paramdef_array[i].min;
		}

	}
	else {

		//seed
		srand(time(NULL));

		//choose a random set of params within the parmdef limits
		//make sure choosen random params are not already in the model
		//do {
		//	for(i=0; i<r->n_p; i++) {
		//		params[i] = rand_between(r->paramdef_array[i].min,
		//		                         r->paramdef_array[i].max);
		//	}
		//} while(get_bench(r->model, params) != NULL); //test in model already

		for(i=0; i<r->n_p; i++) {
			params[i] = rand_between(r->paramdef_array[i].min,
			                         r->paramdef_array[i].max);
		}

        align_params(params, r->paramdef_array, r->n_p);

	}

	//return benchmark parameters
	return params;
}

long long int rand_between(long long int min, long long int max)
{
	if(min==max) {
		return min;
	}

	if(min>max) {
		max = max+min;
		min = max-min; //min now equals original max
		max = max-min; //max now equals original min
	}

	return (long long int)(rand()%(max-min))+min;

}
/*!
 */
long long int*
multi_gbbp_diagonal_select_new_bench(struct pmm_routine *r)
{
    long long int *params;

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

    params = malloc(r->n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    if((top_i = read_top_interval(i_list)) == NULL) {
        ERRPRINTF("Error reading interval from head of list\n");
        free(params);
        return NULL;
    }

    DBGPRINTF("Choosing benchmark based on following interval:\n");
    print_interval(top_i);

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

            if(r->n_p > 1) {
                //mesh boundary models to create points
                if(project_diagonal_intervals(m) < 0) {
                    ERRPRINTF("Error projecting new intervals from diagonal\n");
                    free(params);
                    return NULL;
                }

                //read another top interval
                if((top_i = read_top_interval(i_list)) == NULL) {
                    ERRPRINTF("Error reading interval from head of list\n");
                    free(params);
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
                    DBGPRINTF("interval list now empty.\n");

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
            print_interval(top_i);
            free(params);
            params = NULL;
            break;
    }

    return params;
}

int
init_gbbp_diagonal_interval(struct pmm_routine *r)
{
    int j;
    struct pmm_model *m;
    struct pmm_interval_list *i_list;
    struct pmm_interval *new_i;

    m = r->model;
    i_list = m->interval_list;

    //models with more than 1 boundary need a tag to indicate mesh stage
    //of construction
    if(r->n_p > 1) {
        new_i = new_interval();
        new_i->type = IT_BOUNDARY_COMPLETE;
        add_top_interval(i_list, new_i);
    }



    // add an empty interval from min -> max along a diagonal through
    // the parameter space
    new_i = new_interval(); 
    new_i->type = IT_GBBP_EMPTY; 

    new_i->n_p = r->n_p;

    new_i->start = init_param_array_min(r->paramdef_array, r->n_p);
    if(new_i->start == NULL) {
        ERRPRINTF("Error initialising minimum parameter array.\n");
        free_interval(new_i);
        return -1;
    }

    new_i->end = init_param_array_max(r->paramdef_array, r->n_p);
    if(new_i->end == NULL) {
        ERRPRINTF("Error initialising maximum parameter array.\n");
        free_interval(new_i);
        return -1;
    }

    if(is_interval_divisible(new_i, r) == 1) {
        add_top_interval(m->interval_list, new_i);
    }
    else {
        LOGPRINTF("Interval not divisible, not adding.\n");
        print_interval(new_i);
        free_interval(new_i);
    }


    // when fuzzy_max is set there will be no zero-speed benchmarks that
    // bound the parameter space of the model. So we must benchmark at these
    // points so that or model has a fully bounded space before the GBBP
    // algorithm begins
    //
    // if fuzzy_max is set on any of the parameters the then we will benchmark
    // at max of all parameters. A POINT interval at max_0,max_1,...,max_n will
    // be added to the top of the interval stack so that it is measured before
    // GBBP diagonal construction proceeds
    for(j=0; j<r->n_p; j++) {
        if(r->paramdef_array[j].fuzzy_max == 1) {
            DBGPRINTF("fuzzy max on param:%d\n", j);
            // add benchmark at max of all parameters
            new_i = new_interval();
            new_i->type = IT_POINT;
            new_i->n_p = r->n_p;

            new_i->start = init_param_array_max(r->paramdef_array, r->n_p);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising maximum parameter array.\n");
                free_interval(new_i);
                return -1;
            }

            add_top_interval(i_list, new_i);

            break;
        }
    }

    // now for each parameter, if fuzzy max is set, create a POINT interval at
    // a max value for that parameter along its axis, i.e.
    // min,min,...,max_i,...,min_n
    for(j=0; j<r->n_p; j++) {
        if(r->paramdef_array[j].fuzzy_max == 1) {
            // add benchmark at max on axis boundary (fuzzy max)
            new_i = new_interval();
            new_i->type = IT_POINT;
            new_i->n_p = r->n_p;

            new_i->start = init_param_array_min(r->paramdef_array, r->n_p);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising minimum parameter array.\n");
                free_interval(new_i);
                return -1;
            }

            new_i->start[j] = r->paramdef_array[j].max;

            add_top_interval(i_list, new_i);
        }
    }

    DBGPRINTF("New Intervals initialized:\n");
    print_interval_list(i_list);

    return 0; //success
}

/*!
 * Assuming all points in the model are along a diagonal constructed by
 * GBBP from min_0, min_1, ..., min_n to max_0, max_1, ..., max_n, project
 * construction intervals through each diagonal point, along mutually
 * perpendicular lines.
 *
 * Or more formally, given a set n parameter definitions, describing maximum
 * and minimum values:
 *      MAX = max_0, max_1, ..., max_n
 *      MIN = min_0, min_1, ..., min_n
 *
 * For each point p along a diagonal from a point (min_0, min_1, ...,min_n) to
 * (max_0, max_1, ..., max_n)
 *    For each element p_i of a point p = (p_0, p_1, ..., p_n)
 *       find a point a by replacing p_i in the point p with min_i
 *       find a point b by replacing p_i in the point p with max_i
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

        for(j=0; j<m->n_p; j++) {

            new_i = new_projection_interval(b->p,
                                       &(m->parent_routine->paramdef_array[j]),
                                       j, b->n_p);
            if(new_i == NULL) {
                ERRPRINTF("Error creating new projection interval.\n");
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                DBGPRINTF("Adding diagonal projection interval.\n");
                print_interval(new_i);
                add_top_interval(m->interval_list, new_i);
            }
            else {
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
            }

            if(m->parent_routine->paramdef_array[j].fuzzy_max == 1) {
                //add an IT_POINT interval to benchmark at max
                //
                new_i = init_interval(0, b->n_p, IT_POINT, b->p, NULL);
                if(new_i == NULL) {
                    ERRPRINTF("Error creating new point interval.\n");
                    return -1;
                }

                new_i->start[j] = m->parent_routine->paramdef_array[j].max;


                DBGPRINTF("Adding fuzzy max interval in diagonal projection\n");
                print_interval(new_i);

                add_top_interval(m->interval_list, new_i);

            }
            else {
                // create a zero speed point at max ...
                //
                zero_b = new_benchmark();
                if(zero_b == NULL) {
                    ERRPRINTF("Error allocating new benchmark.\n");
                    return -1;
                }

                //copy parameters from current benchmark
                zero_b->n_p = b->n_p;
                zero_b->p = init_param_array_copy(b->p, b->n_p);
                if(zero_b->p == NULL) {
                    ERRPRINTF("Error copying parameter array.\n");
                    free_benchmark(zero_b);
                    return -1;
                }
                //set parameter of the projection direction to max
                zero_b->p[j] = m->parent_routine->paramdef_array[j].max;

                //make speed zero
                zero_b->flops = 0.;

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
        print_benchmark(zero_b);
        if(insert_bench(m, zero_b) < 0) {
            ERRPRINTF("Error inserting a zero benchmark.\n");

            free_benchmark(zero_b);
            free_benchmark_list_backwards(prev_b);

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
 * @return pointer to a new interval going from p_0,p_1, ... min_d, ... p_n
 * to p_0, p_1, .... max_d, ... p_n
 *
 */
struct pmm_interval*
new_projection_interval(long long int *p, struct pmm_paramdef *pd, int d,
                         int n)
                                 
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
        free_interval(new_i);
        return NULL;
    }
    new_i->start[d] = pd->min;

    new_i->end = init_param_array_copy(p, n);
    if(new_i->end == NULL) {
        ERRPRINTF("Error initialising interval end parameter.\n");
        free_interval(new_i);
        return NULL;
    }
    new_i->end[d] = pd->max;

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
long long int*
multi_gbbp_select_new_bench(struct pmm_routine *r)
{
    long long int *params;

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

    params = malloc(r->n_p * sizeof *params);
    if(params == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    if((top_i = read_top_interval(i_list)) == NULL) {
        ERRPRINTF("Error reading interval from head of list\n");
        free(params);
        return NULL;
    }

    DBGPRINTF("Choosing benchmark based on following interval:\n");
    print_interval(top_i);

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

            if(r->n_p > 1) {
                //mesh boundary models to create points
                mesh_boundary_models(m);

                //read another top interval
                if((top_i = read_top_interval(i_list)) == NULL) {
                    ERRPRINTF("Error reading interval from head of list\n");
                    free(params);
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
                    DBGPRINTF("interval list now empty.\n");

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
            print_interval(top_i);
            free(params);
            params = NULL;
            break;
    }

    return params;
}

int
init_gbbp_boundary_intervals(struct pmm_routine *r)
{
    int j;
    struct pmm_model *m;
    struct pmm_interval_list *i_list;
    struct pmm_interval *new_i;

    m = r->model;
    i_list = m->interval_list;

    //models with more than 1 boundary need a tag to indicate mesh stage
    //of construction
    if(r->n_p > 1) {
        new_i = new_interval();
        new_i->type = IT_BOUNDARY_COMPLETE;
        add_top_interval(i_list, new_i);
    }

    // for each parameter of the routine
    for(j=0; j<r->n_p; j++) {
        //push boundary model of that parameter onto the interval stack


        // if fuzzy_max is set then we will benchmark at max and build
        // the model between this point and min. EMPTY interval will range
        // from min to max and a POINT interval at max will be added to
        // the top of the interval stack so that it is measured before GBBP
        // construction proceeds
        if(r->paramdef_array[j].fuzzy_max == 1) {

            DBGPRINTF("fuzzy max on param:%d\n", j);

            // add an empty interval from min -> max along boundary
            new_i = new_interval(); 
            new_i->type = IT_GBBP_EMPTY; 

            new_i->plane = j;
            new_i->n_p = r->n_p;

            new_i->start = init_param_array_min(r->paramdef_array, r->n_p);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising minimum parameter array.\n");
                free_interval(new_i);
                return -1;
            }

            new_i->end = init_param_array_min(r->paramdef_array, r->n_p);
            if(new_i->end == NULL) {
                ERRPRINTF("Error initialising minimum parameter array.\n");
                free_interval(new_i);
                return -1;
            }
            new_i->end[j] = r->paramdef_array[j].max;

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
            }


            // add benchmark at max on boundary (fuzzy max)
            new_i = new_interval();
            new_i->type = IT_POINT;
            new_i->n_p = r->n_p;

            new_i->start = init_param_array_min(r->paramdef_array, r->n_p);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising minimum parameter array.\n");
                free_interval(new_i);
                return -1;
            }

            new_i->start[j] = r->paramdef_array[j].max;

            add_top_interval(i_list, new_i);


        }
        else {
            // add an empty interval from min -> max along boundary
            new_i = new_interval(); 
            new_i->type = IT_GBBP_EMPTY; 

            new_i->plane = j;
            new_i->n_p = r->n_p;

            new_i->start = init_param_array_min(r->paramdef_array, r->n_p);
            if(new_i->start == NULL) {
                ERRPRINTF("Error initialising minimum parameter array.\n");
                free_interval(new_i);
                return -1; //failure
            }

            new_i->end = init_param_array_min(r->paramdef_array, r->n_p);
            if(new_i->end == NULL) {
                ERRPRINTF("Error initialising minimum parameter array.\n");
                free_interval(new_i);
                return -1; //failure
            }
            new_i->end[j] = r->paramdef_array[j].max;

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
            }

        }

    }

    return 0; //success
}

void mesh_boundary_models(struct pmm_model *m)
{
    long long int *params;
    int n_p;
    int plane = 0;


    n_p = m->parent_routine->n_p;

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

}

//TODO prevent boundary benchmarks from being added as they are already
//measured
/*
 *
 */
void recurse_mesh(struct pmm_model *m, long long int *p, int plane, int n_p)
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
            if(is_benchmark_at_origin(n_p, m->parent_routine->paramdef_array,
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
                print_params(interval->start, n_p);
                print_params(b->p, n_p);
            }

            b = get_previous_different_bench(b);
        }
    }
    */

}

/*
 * multi_gbbp_insert_benchmark - the second half of the gbbp proceedure. After
 * a benchmark has been made it must be added to the model and the state of
 * the building proceedure must be adjusted according to the new shape of
 * the model.
 *
 * The rules that govern this adjustment and the specific adjustments are as
 * follows:
 *
 * if the model is empty, we add the benchmark to the model and set the state
 * to be climbing.
 *
 * if the model is climbing we test the new benchmark to see if the model is
 * still climbing, or has levelled out, or has begun to decrease. If the
 * model is not still climbing or levelled out, we change the state to
 *
 * @return 0 on success, -1 on failure to process intervals, -2 on failure
 * to insert benchmark
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
    print_params(b->p, b->n_p);

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

    if(time_spend >= (double)r->min_sample_time ||
       num_execs >= r->min_sample_num)
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
 * @return 0 on successful processing of interval, -1 on failure
 */
int
process_interval_list(struct pmm_routine *r, struct pmm_benchmark *b,
                      struct pmm_loadhistory *h)
{

    struct pmm_interval *i, *i_prev;
    long long int *temp_params;
    int done = 0, ret;

    i = r->model->interval_list->top;

    temp_params = malloc(r->n_p * sizeof *temp_params);
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
            return -1;
        }

        // if a gbbp benchmark point was set by multi_gbbp_bench ....  then
        // check that the benchmark b is at the appropriate gbbp point
        if(ret == 0 && params_cmp(b->p, temp_params, b->n_p) == 0) {

            DBGPRINTF("Interval found:\n");
            print_interval(i);
            print_params(temp_params, r->n_p);

            done = process_interval(r, i, b, h);
            if(done < 0) {

                ERRPRINTF("Error processing interval.\n");
                free(temp_params);
                return -1;

            }
        }
        // benchmark is not at appropriate gbbp point
        else {
            // TODO try to reduce construction time by processing benchmarks
            // TODO that are not lying on appropriate GBBP points somehow.
        }

        // work backwards from the top/front of the list
        i = i_prev;
    }

    free(temp_params);
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

            done = process_gbbp_bisect(r, i, b, h) < 0 ? -1 : 1;

            break;

        case IT_GBBP_INFLECT :

            done = process_gbbp_inflect(r, i, b, h) < 0 ? -1 : 1;

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
 * top of the interval list. The min,min,...,min set of parameters match all
 * IT_GBBP_EMPTY intervals for all different boundary planes. We will process
 * all of these intervals, not just the first occurance in the interval list.
 * However, not all of the planes are ready for construction (e.g. their
 * fuzzy_max benchmark may not be executed yet). Inserting the next interval
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
    long long int *tmp_params;

    m = r->model;


    //if we have not had more than 2 climb steps there will not be enough
    //points in the model to test for end of climbing ...
    if(i->climb_step < 2) {
        //not enough points
        DBGPRINTF("not enough points to test climbing, keep climbing...\n");

        i->climb_step = i->climb_step+1;

        //check interval is divisible and remove if not
        if(is_interval_divisible(i, r) != 1) {
            LOGPRINTF("Interval not divisible, removing.\n");
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
            ERRPRINTF("Error initialising minimum parameter array.\n");
            return -1;
        }

        // get avg of benchmarks to the left of the new benchmark, set params
        // to the current climb step ...
        set_params_step_along_climb_interval(tmp_params, i->climb_step, i,
                r->paramdef_array, r->n_p);
        b_left_0 = get_avg_bench(m, tmp_params);

        //get parameters for previous climb step to current
        set_params_step_along_climb_interval(tmp_params, i->climb_step-1, i,
                r->paramdef_array, r->n_p);
        b_left_1 = get_avg_bench(m, tmp_params);

        //get parameters for 2nd previous climb step to current
        set_params_step_along_climb_interval(tmp_params, i->climb_step-2, i,
                r->paramdef_array, r->n_p);
        b_left_2 = get_avg_bench(m, tmp_params);

        if(b_left_0 == NULL || b_left_1 == NULL || b_left_2 == NULL) {
            ERRPRINTF("Error, no benchmark @ a previous climb point\n");

            free_benchmark(b_avg);

            if(b_left_0 != NULL)
                free_benchmark(b_left_0);
            if(b_left_1 != NULL)
                free_benchmark(b_left_1);
            if(b_left_2 != NULL)
                free_benchmark(b_left_2);

            free(tmp_params);

            return -1;
        }

        DBGPRINTF("b_left_0:\n");
        print_benchmark(b_left_0);
        DBGPRINTF("b_left_1\n");
        print_benchmark(b_left_1);
        DBGPRINTF("b_left_2:\n");
        print_benchmark(b_left_2);


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
            DBGPRINTF("%f@%lld %f@%lld %f@%lld\n",
                      b_left_0->flops, b_left_0->p[i->plane],
                      b_left_1->flops, b_left_1->p[i->plane],
                      b_left_2->flops, b_left_2->p[i->plane]);

            DBGPRINTF("this bench:\n");
            print_benchmark(b_avg);

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
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(i);
                free_interval(new_i);
            }

            remove_interval(m->interval_list, i);
        }
        // otherwise if the benchmark b is greater than any of the 3 previous
        // benchmarks, continue with the CLIMB phase of construction,
        // increment step
        else {
            DBGPRINTF("performance still climbing\n");
            DBGPRINTF("previous bench speeds:\n");
            DBGPRINTF("%f@%lld %f@%lld %f@%lld\n",
                    b_left_0->flops, b_left_0->p[i->plane],
                    b_left_1->flops, b_left_1->p[i->plane],
                    b_left_2->flops, b_left_2->p[i->plane]);
            DBGPRINTF("this bench:\n");
            print_benchmark(b_avg);

            i->climb_step = i->climb_step+1;

            //check interval is divisible and remove if not
            if(is_interval_divisible(i, r) != 1) {
                LOGPRINTF("Interval not divisible, removing.\n");
                remove_interval(m->interval_list, i);
            }
        }

        free_benchmark(b_avg);
        free_benchmark(b_left_0);
        free_benchmark(b_left_1);
        free_benchmark(b_left_2);

        free(tmp_params);
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
 * Step alogn the climb interval, from the start point forwards or
 * backwards, a number of times.
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
 */
void
set_params_step_along_climb_interval(long long int *params, int step, 
                             struct pmm_interval *i,
                             struct pmm_paramdef *pd_array, int n_p)
{
    int j;
    int min_stride;    //value of the minimum stride
    int min_stride_i;  //parameter index of the minimum stride


    //
    // The increment along a line drawn from start->end is determined
    // by the smallest stride of all the parameters whose axes in the
    // model are not perpendicular to the line from start->end.
    //
    // E.g. if start = (1,1) and end = (1,5000) then the line joining
    // start to end is perpendicular to the axis of the 1st parameter,
    // and its stride should be ignored.
    //
    // It obviously follows that if the values of a parameter
    // are equal in both the start and end points, then that parameter
    // axis is perpendicular to the interval line.
    //
    min_stride = INT_MAX;
    min_stride_i = -1;
    for(j=0; j<n_p; j++) {
        if(i->start[j] != i->end[j]) {
            if(pd_array[j].stride < min_stride) {
                min_stride = pd_array[j].stride;
                min_stride_i = j;
            }
        }
    }

    DBGPRINTF("min_stride:%d, min_stride_i:%d\n", min_stride, min_stride_i);

    //
    // Now that we have identified the parameter and stride we will
    // increment along we must calculate the position of the incremented
    // point with the particular stride we apply.
    //
    // The parametric representation of a line L between two points, S 
    // = (s_1, s_2, ..., s_n) and E = (e_1, e_2, ..., e_n) is:
    //
    //   L = L(a) = S^T + a*B, where B = E^T - S^T
    //
    // If we take one of the components (the min_stride_i-th) of the
    // start point S: s_i and increment it by the stride to get i_i
    // (the value of the component at the incremented point I), then
    // we can solve for a:
    //
    //   i_i = s_i+inc = s_i+a(e_i-s_i)
    //               a = inc/(e_i-s_i)
    //
    // Then we may calculate all other values x= 0 ... n, for the
    // incremented point I by calculating L with the solved a.
    //
    //   I_x = s_x + (inc/(e_i-s_i))*(e_x-s_x)
    //
    // In order to maintain some precision when doing integer math we
    // rearrange this to be
    //
    //   I_x = s_x + inc*((e_x-s_x)/(e_i-s_i))
    //
    
    // to get the 1st/2nd/nth next/previous climb point
    min_stride = min_stride*step;

    DBGPRINTF("step:%d min_stride:%d\n", step, min_stride);

    for(j=0; j<n_p; j++) {
        params[j] = i->start[j] + (min_stride *
                                     ((i->end[j] - i->start[j]) /
                                            (i->end[min_stride_i] - 
                                             i->start[min_stride_i])));

        //             1 + (5/(300-1))*(1-1) = 
        //             1 + (5/(300-1))*(300-1) = 1+5 ...

        DBGPRINTF("i->start[%d]:%lld i->end{%d]:%lld\n", j, i->start[j], j,
                  i->end[j]);
        DBGPRINTF("i->start[min_stride_i]:%lld i->end[min_stride_i]:%lld\n",
                   i->start[min_stride_i], i->end[min_stride_i]);

        DBGPRINTF("params[%d]:%lld\n", j, params[j]);
    }

    align_params(params, pd_array, n_p);

    DBGPRINTF("aligned params:\n");
    print_params(params, n_p);

    return;

}

/*!
 * @return 0 on success, -1 on failure
 */
int
process_gbbp_bisect(struct pmm_routine *r, struct pmm_interval *i,
                    struct pmm_benchmark *b, struct pmm_loadhistory *h)
{

    struct pmm_interval *new_i;
    struct pmm_model *m;
    struct pmm_benchmark *b_avg, *b_oldapprox, *b_left, *b_right;
    long long int *midpoint;
    int intersects_left, intersects_right;

    m = r->model;


    //get the average benchmark at the model where b is positioned
    b_avg = get_avg_bench(m, b->p);


    //find the unaligned midpoint of the interval
    midpoint = malloc(i->n_p * sizeof *midpoint);
    if(midpoint == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        free_benchmark(b_avg);
        return -1;
    }
    set_params_interval_midpoint(midpoint, i);

    // look up benchmarks at aligned interval->start
    b_left = get_avg_aligned_bench(m, i->start);
    if(b_left == NULL) {
        ERRPRINTF("Error, no benchmark @ interval start\n");
        free(midpoint);
        free_benchmark(b_avg);
        return -1;
    }

    // look up benchmark at aligned interval->end
    b_right = get_avg_aligned_bench(m, i->end);
    if(b_right == NULL) {
        ERRPRINTF("Error, no benchmark @ interval end\n");
        free_benchmark(b_left);
        free(midpoint);
        free_benchmark(b_avg);
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
        print_params(i->start, i->n_p);
        DBGPRINTF("to:\n");
        print_params(i->end, i->n_p);

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
            LOGPRINTF("Interval not divisible, not adding.\n");
            print_interval(new_i);
            free_interval(new_i);
        }

        DBGPRINTF("finished building from:\n");
        print_params(i->start, i->n_p);
        DBGPRINTF("to:\n");
        print_params(midpoint, i->n_p);

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
            LOGPRINTF("Interval not divisible, not adding.\n");
            print_interval(new_i);
            free_interval(new_i);
        }

        DBGPRINTF("finished building from:\n");
        print_params(midpoint, i->n_p);
        DBGPRINTF("to:\n");
        print_params(i->end, i->n_p);

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
                LOGPRINTF("Interval not divisible, not adding.\n");
                free_interval(new_i);
                print_interval(new_i);
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
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
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

            if(is_interval_divisible(new_i, r) == 0) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
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
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
            }

            remove_interval(m->interval_list, i);
        }

        if(b_oldapprox != NULL) {
            free_benchmark(b_oldapprox);
        }
    }

    free(b_avg);
    free(midpoint);

    if(b_left != NULL) {
        free_benchmark(b_left);
    }
    if(b_right != NULL) {
        free_benchmark(b_right);
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
 * @return 0 on success, -1 on failure
 */
int
process_gbbp_inflect(struct pmm_routine *r, struct pmm_interval *i,
                     struct pmm_benchmark *b, struct pmm_loadhistory *h)
{

    struct pmm_interval *new_i;
    struct pmm_model *m;
    struct pmm_benchmark *b_avg, *b_left, *b_right, *b_oldapprox;
    long long int *midpoint;
    int intersects_left, intersects_right;

    m = r->model;


    //get the average benchmark at the model where b is positioned
    b_avg = get_avg_bench(m, b->p);

    // lookup the model's approximation of b, with no benchmarks at b
    // contributing
    b_oldapprox = find_oldapprox(r->model, b->p);
    if(b_oldapprox == NULL) {
        ERRPRINTF("Error finding old approximation.\n");
        free_benchmark(b_avg);
        return -1;
    }

    //find the unaligned midpoint of the interval, we only use this for
    //creating the next intervals
    midpoint = malloc(i->n_p * sizeof *midpoint);
    if(midpoint == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        free_benchmark(b_oldapprox);
        free_benchmark(b_avg);
        return -1;
    }
    set_params_interval_midpoint(midpoint, i);

    // look up benchmarks at aligned interval->start
    b_left = get_avg_aligned_bench(m, i->start);
    if(b_left == NULL) {
        ERRPRINTF("Error, no benchmark @ interval start\n");
        free(midpoint);
        free_benchmark(b_oldapprox);
        free_benchmark(b_avg);
        return -1;
    }

    // look up benchmark at aligned interval->end
    b_right = get_avg_aligned_bench(r->model, i->end);
    if(b_right == NULL) {
        ERRPRINTF("Error, no benchmark @ interval end\n");
        free_benchmark(b_left);
        free(midpoint);
        free_benchmark(b_oldapprox);
        free_benchmark(b_avg);
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
                free_benchmark(b_right);
                free_benchmark(b_left);
                free(midpoint);
                free_benchmark(b_oldapprox);
                free_benchmark(b_avg);
                return -1;
            }
            
            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
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
                free_benchmark(b_right);
                free_benchmark(b_left);
                free(midpoint);
                free_benchmark(b_oldapprox);
                free_benchmark(b_avg);
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
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
                free_benchmark(b_right);
                free_benchmark(b_left);
                free(midpoint);
                free_benchmark(b_oldapprox);
                free_benchmark(b_avg);
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
            }

            new_i = init_interval(i->plane, i->n_p, IT_GBBP_BISECT,
                                  midpoint, i->end);

            if(new_i == NULL) {
                ERRPRINTF("Error initialising new interval.\n");
                free_benchmark(b_right);
                free_benchmark(b_left);
                free(midpoint);
                free_benchmark(b_oldapprox);
                free_benchmark(b_avg);
                return -1;
            }

            if(is_interval_divisible(new_i, r) == 1) {
                add_top_interval(m->interval_list, new_i);
            }
            else {
                LOGPRINTF("Interval not divisible, not adding.\n");
                print_interval(new_i);
                free_interval(new_i);
            }

            remove_interval(m->interval_list, i);
        }
    }

    free(midpoint);
    free_benchmark(b_avg);

    if(b_oldapprox != NULL) {
        free_benchmark(b_oldapprox);
    }
    if(b_left != NULL) {
        free_benchmark(b_left);
    }
    if(b_right != NULL) {
        free_benchmark(b_right);
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
    long long int *params;
    long long int *start_aligned, *end_aligned;


    switch (i->type) {
        case IT_GBBP_EMPTY:
            return 1;

        case IT_GBBP_CLIMB:
        case IT_GBBP_BISECT:
        case IT_GBBP_INFLECT:

            // allocate/init some memory
            params = malloc(r->n_p * sizeof *params);
            start_aligned = init_param_array_copy(i->start, r->n_p);
            end_aligned = init_param_array_copy(i->end, r->n_p);
            if(params == NULL || start_aligned == NULL || end_aligned == NULL) {
                ERRPRINTF("Error allocating memory.\n");
                return -1;
            }

            //align start/end
            align_params(start_aligned, r->paramdef_array, r->n_p);
            align_params(end_aligned, r->paramdef_array, r->n_p);
            
            // find the GBBP point of the interval
            if(multi_gbbp_bench_from_interval(r, i, params) < 0) {
                ERRPRINTF("Error getting GBBP bench point from interval.\n");
                return -1;
            }

            // TODO maybe test if the point is > start and < end

            // test that point is not equal to aligned start or end of interval
            if(params_cmp(start_aligned, params, r->n_p) == 0 ||
               params_cmp(end_aligned, params, r->n_p) == 0)
            {
                return 0; //not divisible
            }
            else {
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
                               long long int *params)
{

    //depending on interval type, set the param of the boundary plane
    switch (interval->type) {
        case IT_GBBP_EMPTY : // empty, return the interval's start

            DBGPRINTF("assigning minimum problem size.\n");

            set_param_array_copy(params, interval->start, r->n_p);

            return 0;

        case IT_GBBP_CLIMB : // climbing, return increment on interval's start

            DBGPRINTF("assigning increment on interval start point.\n");


            set_params_step_along_climb_interval(params, interval->climb_step+1,
                                           interval, r->paramdef_array, r->n_p);

            align_params(params, r->paramdef_array, r->n_p);

            return 0;

        case IT_GBBP_BISECT : // interval bisect, return midpoint
        case IT_GBBP_INFLECT : // interval inflect, return midpoint

            //set params to be midpoint between start and end of interval
            set_params_interval_midpoint(params, interval);

            //align midpoint
            align_params(params, r->paramdef_array, r->n_p);

            DBGPRINTF("assigned midpoint of interval from start:\n");
            print_params(interval->start, interval->n_p);
            DBGPRINTF("to end:\n");
            print_params(interval->end, interval->n_p);
            DBGPRINTF("at:\n");
            print_params(params, interval->n_p);

            return 0;

       case IT_POINT : // 'point' type interval from mesh, return copied point

            DBGPRINTF("assigning copy of interval 'point'.\n");

            set_param_array_copy(params, interval->start, r->n_p);

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
set_params_interval_midpoint(long long int *p, struct pmm_interval *i)
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

