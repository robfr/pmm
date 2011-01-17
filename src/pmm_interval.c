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
 * @file    pmm_interval.c
 * @brief   Code for working with construction intervals
 *
 * Construction intervals describe built/unbuilt regions of a model and
 * are core to the optimised construction algorithms
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h> // for malloc/free
#include <string.h> // for strcmp

#include "pmm_log.h"
#include "pmm_interval.h"
#include "pmm_data.h" // for param_array_copy, print_params

/*!
 * Allocate and return an interval list structure
 *
 * @returns pointer to new interval list
 */
struct pmm_interval_list*
new_interval_list()
{
	struct pmm_interval_list *l;

	l = malloc(sizeof *l);

	l->size = 0;
	l->top = NULL;
	l->bottom = NULL;

	return l;
}

/*!
 * frees an interval list structure and members it contains
 *
 * @param   il   pointer to address of the interval list
 */
void
free_interval_list(struct pmm_interval_list **il)
{

	struct pmm_interval *this, *next;

	this = (*il)->top;

	while(this != NULL) {
		next = this->next;

		free_interval(&this);

		this = next;
	}

	free(*il);
    *il = NULL;
}

/*!
 * determine if interval list is empty
 * 
 * @param   l   pointer to interval list
 * @returns 1 if list is empty, 0 if list is not empty
 */
int
isempty_interval_list(struct pmm_interval_list *l)
{
    //TODO static
	if(l->size <= 0) {
		return 1; // true
	}
	else {
		return 0; // false
	}
}

/*!
 * Allocate and return an interval structure with default, zero parameter values
 *
 * @return pointer to the allocated structure or NULL on failure
 */
struct pmm_interval* new_interval()
{
	struct pmm_interval *i;

	i = malloc(sizeof *i);
	if(i == NULL) {
		ERRPRINTF("Error allocating memory for interval struct\n");
        return i;
	}

    i->plane = -1;
    i->climb_step = 0;

    i->n_p = -1;
	i->start = NULL;
	i->end = NULL;

	i->type = IT_NULL;

	i->next = NULL;
	i->previous = NULL;

	return i;
}

/*!
 * Initialize an interval with parameters passed.
 *
 * Function maybe be used with _INFLECT and _BISECT intervals and with
 * _POINT intervals if the start parameter is set to the point. May also
 * be used to on initial IT_GBBP_CLIMB but not once climb_step is non-zero
 *
 * @param   plane   plane that the interval belongs to (defunct)
 * @param   n_p     number of parameters of the interval points
 * @param   type    interval tyle
 * @param   start   pointer to the start parameter array
 * @param   end     pointer to the end parameter array
 *
 * @return pointer to a newly allocated interval with values initialised as
 * per the arguments passed
 *
 */
struct pmm_interval* init_interval(int plane,
                                  int n_p,
                                  enum pmm_interval_type type,
                                  int *start,
                                  int *end)
{
    struct pmm_interval *i;

    i = new_interval();

    i->plane = plane;
    i->n_p = n_p;
    i->type = type;

    switch(i->type) {
        case IT_GBBP_CLIMB :
        case IT_GBBP_BISECT :
        case IT_GBBP_INFLECT :
            i->start = init_param_array_copy(start, n_p);
            i->end = init_param_array_copy(end, n_p);
            break;

        case IT_POINT :
            i->start = init_param_array_copy(start, n_p);
            break;

        default :
            ERRPRINTF("Interval type not supported; %s\n",
                                             interval_type_to_string(i->type));
            free_interval(&i);
            return NULL;
    }

    return i;
}

/*!
 * frees an interval structure and members it contains
 *
 * @param   i    pointer to address of the interval
 */
void free_interval(struct pmm_interval **i)
{
    if((*i)->start != NULL) {
        free((*i)->start); 
        (*i)->start = NULL;
    }
    if((*i)->end != NULL) {
        free((*i)->end);
        (*i)->end = NULL;
    }

    free(*i);
    *i = NULL;
}

//TODO remove this is not neccessary
struct pmm_interval*
read_top_interval(struct pmm_interval_list *l)
{
	if(!isempty_interval_list(l)) {
		return l->top; // success
	}
	else {
		return NULL; // failure
	}
}

/*!
 * Removes and deletes the top interval on an interval list
 *
 * @param   l   pointer to the interval list
 *
 * @return 0 on failure, 1 on success
 */
//TODO fix naming of functions so 'remove'/'delete' indicates deallocation also
//TODO fix return to -1 on failure, 0 on success
int
remove_top_interval(struct pmm_interval_list *l)
{
	// remove top destructively removes the top element of the list, remember
	// to read it first or it will be gone forever

	struct pmm_interval *zombie;

	if(isempty_interval_list(l)) {
		return 0; // failure
	}
	else {

		if(l->top == l->bottom) { /* only one element in list */
			zombie = l->top;
			l->top = NULL;
			l->bottom = NULL;

		}
		else {
			/* swap old top with new top */
			zombie = l->top;
			l->top = zombie->previous;

			l->top->next=NULL;


		}

        free_interval(&zombie);

		l->size -= 1;

		return 1; // success
	}
}

/*!
 * Removes and deletes an interval from an interval list
 *
 * @param   l   pointer to interval list
 * @param   i   pointer to the interval to remove
 *
 * @return 1 on success
 */
//TODO failure? return 0 on success
int
remove_interval(struct pmm_interval_list *l, struct pmm_interval *i)
{

    LOGPRINTF("removing interval with type: %d\n", i->type);

    //if interval is at top or bottom, rewire top/bottom pointers
	if(l->top == i) {
		l->top = i->previous;
	}
	if(l->bottom == i) {
		l->bottom = i->next;
	}

    //rewire intervals that were previous/next intervals of the remove-ee
	if(i->next != NULL) {
		i->next->previous = i->previous;
	}
	if(i->previous != NULL) {
		i->previous->next = i->next;
	}


    //free interval
	free_interval(&i);
	l->size -= 1;

    LOGPRINTF("size: %d\n", l->size);

	return 1;
}


/*!
 * link the interval into interval list at the top/front
 *
 * @param   l   pointer to interval list
 * @param   i   pointer to interval to add to list
 *
 * @return success always
 */
int add_top_interval(struct pmm_interval_list *l, struct pmm_interval *i)
{

	 /* if the list was empty, new i will be only member, top & bottom */
	if(isempty_interval_list(l)) {
		i->previous = NULL;
		i->next = NULL;

		l->bottom = i;
		l->top = i;
	} else {
		i->previous = l->top;
        l->top->next = i;

		l->top = i;
		i->next = NULL;
	}

	l->size += 1;

	return 0; // success TODO void
}

/*!
 * Add an interval to the bottom of the interval list
 *
 * @param   l   pointer to the interval list
 * @param   i   pointer to the interval
 *
 * @return 0 on success
 */
int
add_bottom_interval(struct pmm_interval_list *l, struct pmm_interval *i)
{

	// if the list was empty, new i will be only member, top & bottom
	if(isempty_interval_list(l)) {
		i->previous = NULL;
		i->next = NULL;

		l->bottom = i;
		l->top = i;
	} else {
        i->next = l->bottom;
        l->bottom->previous = i;

        l->bottom = i;
        i->previous = NULL;
	}

	l->size += 1;

	return 0; // success
}


/*!
 * print interval list
 *
 * @param   output      output stream to print to
 * @param   l           interval list to print
 */
void
print_interval_list(const char *output, struct pmm_interval_list *l)
{
	struct pmm_interval *i;
    int c;

	i = l->top;

    c=0;
	while(i != NULL) {
        SWITCHPRINTF(output, "%d of %d intervals ...\n", c, l->size);

		print_interval(output, i);

		i = i->previous;
        c++;
	}
}

/*!
 * map interval type to string for printing
 *
 * @param       type    interval type
 *
 * @return  char string describing interval
 */
char*
interval_type_to_string(enum pmm_interval_type type)
{
    switch (type) {
        case IT_GBBP_EMPTY:
            return "IT_GBBP_EMPTY";
        case IT_GBBP_CLIMB:
            return "IT_GBBP_CLIMB";
        case IT_GBBP_BISECT:
            return "IT_GBBP_BISECT";
        case IT_GBBP_INFLECT:
            return "IT_GBBP_INFLECT";
        case IT_BOUNDARY_COMPLETE:
            return "IT_BOUNDARY_COMPLETE";
        case IT_COMPLETE:
            return "IT_COMPLETE";
        case IT_POINT:
            return "IT_POINT";
        case IT_NULL:
            return "IT_NULL";
        default:
            return "UNKNOWN";
    }

}

/*!
 * print interval
 *
 * @param   output      output stream to print to
 * @param   i           interval to print
 */
void print_interval(const char *output, struct pmm_interval *i) {

    SWITCHPRINTF(output, "type: %s\n", interval_type_to_string(i->type));

    switch (i->type) {
        case IT_GBBP_EMPTY :
        case IT_GBBP_CLIMB :
            SWITCHPRINTF(output, "climb_step: %d\n", i->climb_step);
        case IT_GBBP_BISECT :
        case IT_GBBP_INFLECT :
            SWITCHPRINTF(output, "plane: %d\n", i->plane);
            SWITCHPRINTF(output, "n_p: %d\n", i->n_p);
            SWITCHPRINTF(output, "start:\n");
            print_params(output, i->start, i->n_p);
            SWITCHPRINTF(output, "end:\n");
            print_params(output, i->end, i->n_p);
            break;
        case IT_BOUNDARY_COMPLETE:
            break;
        case IT_COMPLETE:
            break;
        case IT_POINT:
            SWITCHPRINTF(output, "n_p: %d\n", i->n_p);
            print_params(output, i->start, i->n_p);
            break;
        default:
            break;
    }
}
/*
 * Test if a benchmark is contained within an interval.
 *
 * For intervals of IT_POINT type the test is if the benchmark is at the
 * interval point, for other interval types the test is if the benchmark
 * lies inside a line segment going from the start point of the interval to
 * the end point of the interval.
 *
 * @param   r   pointer to the routine the bench and intervals belong to
 * @param   i   pointer to the interval being tested
 * @param   b   poitner to the benchmark being tested
 *
 * @return 0 if the benchmark is not contained by the interval, 1 if the
 * benchmark is contained by the interval, -1 on error
 */
/* TODO implement interval_contains_bench with non-boundary intervals
int interval_contains_bench(struct pmm_routine *r, struct pmm_interval *i,
		                    struct pmm_benchmark *b)
{
    int b_plane;

    switch(i->type) {
        case IT_BOUNDARY_COMPLETE :
        case IT_COMPLETE :
		    return 0; //false
            break;

        case IT_POINT :
		    if(params_cmp(b->p, i->point->p, b->n_p) == 0) {
		    	return 1; //true
		    }
		    else {
		    	return 0; //false
		    }
            break;
	    case IT_GBBP_EMPTY :
        case IT_GBBP_CLIMB :
        case IT_GBBP_BISECT :
        case IT_GBBP_INFLECT :
            // 
            // to test if a point c is between two other points a & b,
            // first, test they are collinear
            //
            b_plane = param_on_boundary(r->n_p, r->paramdef_array, b->p);
            if(b_plane == i->plane || //if b is on this interval's plane
               b_plane == -1) { //or if b is on all interval planes

                if(b->p[i->plane] >= i->start && b->p[i->plane] <= i->end)

                {
                    return 1; //true
                }
                else {
                    return 0; //false
                }
            }
            else {
                ERRPRINTF("benchmark not on expected boundary plane.\n");
            }
            break;

        default:
            ERRPRINTF("Invalid interval type: %d\n", i->type);
            break;
	}

    return 0;
}
*/
