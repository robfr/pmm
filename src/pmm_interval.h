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
 * @file   pmm_interval.h
 * @brief  data structures for construction intervals
 *
 * Construction intervals describe constructed and unconstructed regions of
 * the model. They are essential in the model building algorithms.
 *
 */
#ifndef PMM_INTERVAL_H_
#define PMM_INTERVAL_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif
/*! enumeration of possible model construction interval types */
typedef enum pmm_interval_type {
    IT_NULL,                /*!< null empty interval */
    IT_BOUNDARY_COMPLETE,   /*!< for models in terms of more than one
                                 parameter, specifies that a boundary axis of the
                                 model is complete */
	IT_GBBP_EMPTY,          /*!< initial empty GPPB interval type */
    IT_GBBP_CLIMB,          /*!< construction interval where model is still
                                 climbing (for GBBP) */
    IT_GBBP_BISECT,         /*!< construction interval where model has taken
                                 descending form (for GBBP)*/
    IT_GBBP_INFLECT,        /*!< construction interval where model is being
                                 tested for accuracy and may almost be complete */
    IT_POINT,               /*!< construction interval that defines a single
                                 point where a benchmark is required */
    IT_COMPLETE             /*!< construction interval which is complete and
                                 does not need further testing */
} PMM_Interval_Type;

/*
static char *pmm_interval_type_str[] = { "NULL",
                             "BOUNDARY_COMPLETE",
                             "GBBP_EMPTY",
                             "GBBP_CLIMB",
                             "GBBP_BISECT",
                             "GBBP_INFLECT",
                             "POINT",
                             "COMPLETE" };
*/


/*!
 * structure describing the construction status of an interval of a model
 */
typedef struct pmm_interval {
	enum pmm_interval_type type;    /*!< type of interval */

    int plane;  /*!< index of the plane that this interval pertains to */
    int n_p;    /*!< number of parameters the interval has */
	int *start; /*!< end point of the interval */
	int *end;   /*!< start point of the interval */

    int climb_step; /*!< index of the step along the interval that we are
                      currently constructing for, when the interval type is
                      IT_GBBP_CLIMB */

	struct pmm_interval *next; /*!< pointer to the next interval in stack */
	struct pmm_interval *previous; /*!< pointer to previous interval in stack */
} PMM_Interval;

/*!
 * structure holding the interval list/stack
 */
typedef struct pmm_interval_list {
	struct pmm_interval *top;       /*!< pointer to top of stack */
	struct pmm_interval *bottom;    /*!< pointer to bottom of stack */
	int size;                       /*!< number of intervals in stack */

} PMM_Interval_List;

struct pmm_interval*
new_interval();
struct pmm_interval_list*
new_interval_list();
int
isempty_interval_list(struct pmm_interval_list *l);
void
print_interval_list(const char *output, struct pmm_interval_list *l);

struct pmm_interval*
read_top_interval(struct pmm_interval_list *l);

int
add_top_interval(struct pmm_interval_list *l, struct pmm_interval *i);

int
add_bottom_interval(struct pmm_interval_list *l, struct pmm_interval *i);

int
remove_top_interval(struct pmm_interval_list *l);

int
remove_interval(struct pmm_interval_list *l, struct pmm_interval *i);

void
print_interval(const char *output, struct pmm_interval *i);

/*
int interval_contains_bench(struct pmm_routine *r, struct pmm_interval *i,
		                    struct pmm_benchmark *b);
*/

struct pmm_interval*
init_interval(int plane,
              int n_p,
              enum pmm_interval_type type,
              int *start,
              int *end);

void
free_interval_list(struct pmm_interval_list **il);
void
free_interval(struct pmm_interval **i);

#endif /*PMM_INTERVAL_H_*/

