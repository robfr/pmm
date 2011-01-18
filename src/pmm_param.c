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
 * @file pmm_param.c
 *
 * @brief functions operating on benchmark and model parameters
 *
 * this file contains the code for working with parameters, parameter
 * definitions, parameter arrays and so forth.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h> // for malloc/free
#include <string.h> // for strcmp
#include <limits.h> // for INT_MAX

#include "pmm_log.h"
#include "pmm_data.h"


/*!
 * Create an empty parameter definition set structure. Note pd_array will
 * not yet be allocated
 *
 * @return pointer to newly allocated structure
 */
struct pmm_paramdef_set* new_paramdef_set()
{
    struct pmm_paramdef_set *pd_set;

    pd_set = malloc(sizeof *pd_set);

    pd_set->n_p = -1;

    pd_set->pc_formula = (void *)NULL;
    pd_set->pc_max = -1;
    pd_set->pc_min = -1;

    return pd_set;
}

/*!
 * Initialise a parameter array to all start values, as descripted by the
 * parameter definitions
 *
 * @param   pd_array    pointer to parameter definition set
 *
 * @return  pointer to an array of parameters with all start values of size n
 */
int*
init_param_array_start(struct pmm_paramdef_set *pd_set)
{
    int *p;

    p = malloc(pd_set->n_p * sizeof *p);
    if(p == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    set_param_array_start(p, pd_set);

    return p;
}

/*!
 * Initialise a parameter array to all end values, as descripted by the
 * parameter definitions
 *
 * @param   pd_set      pointer to parameter definition set structure
 *
 * @return  pointer to an array of parameters with all end values of size n
 */
int*
init_param_array_end(struct pmm_paramdef_set *pd_set)
{
    int *p;

    p = malloc(pd_set->n_p * sizeof *p);
    if(p == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    set_param_array_end(p, pd_set);

    return p;
}

/*!
 * Set a parameter array to all start values, as described by the parameter
 * definitions
 *
 * @param   p           pointer to an array of parameters
 * @param   pd_set      pointer to parameter definitions set structure
 *
 * @pre p must be a pointer to allocated memory, the number of elements in the
 * p array must be identical to the number of parameter definitions
 */
void
set_param_array_start(int *p, struct pmm_paramdef_set *pd_set)
{
    int i;

    for(i=0; i<pd_set->n_p; i++) {
        p[i] = pd_set->pd_array[i].start;
    }

    return;
}

/*!
 * Set a parameter array to all end values, as described by the parameter
 * definitions
 *
 * @param   p           pointer to an array of parameters
 * @param   pd_set      pointer to parameter definitions set structure
 *
 * @pre p must be a pointer to allocated memory, the number of elements in the
 * p array must be identical to the number of parameter definitions
 */
void
set_param_array_end(int *p, struct pmm_paramdef_set *pd_set)
{
    int i;

    for(i=0; i<pd_set->n_p; i++) {
        p[i] = pd_set->pd_array[i].end;
    }

    return;
}


/*!
 * Copy a parameter array into a newly allocated parameter array
 *
 * @param   src pointer to the first element of the source parameter array
 * @param   n   number of parameters in the source parameter array
 *
 * @return  pointer to copied parameter array or NULL on failure
 *
 */
int*
init_param_array_copy(int *src, int n)
{
    int *dst;

    //TODO rename to param_array_copy_to_new or something

    dst = malloc(n * sizeof *dst);
    if(dst == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    set_param_array_copy(dst, src, n);

    return dst; //success
}

/*!
 * Copy from one parameter array to another
 *
 * @param   dst     pointer to the destination array
 * @param   src     pointer tot the source array
 * @param   n       number of elements in each array
 *
 * @return 0 on success, -1 on failure
 *
 * @pre src and dst arrays are of the same size (n)
 */
void
set_param_array_copy(int *dst, int *src, int n)
{
    int i;

    for(i=0; i<n; i++) {
        dst[i] = src[i];
    }

    return;
}

/*!
 * Copy a parameter definition structure from src to dst
 *
 * @param   src     pointer to the paramdef to copy
 * @param   dst     pointer to the paramdef to copy into
 *
 * @return 0 on success, -1 on failure
 *
 * @pre src and dst point to allocated structures
 */
int
copy_paramdef(struct pmm_paramdef *dst, struct pmm_paramdef *src)
{
	dst->end = src->end;
	dst->start = src->start;
	dst->order = src->order;
    dst->nonzero_end = src->nonzero_end;
    dst->stride = src->stride;
    dst->offset = src->offset;

	if(!set_str(&(dst->name), src->name)) {
		ERRPRINTF("set_str failed setting name\n");
        return -1;
	}

	return 0;
}

/*!
 * Test if a set of parameters lies on one of the parameter axes of the model,
 * i.e. all but one parameter is at a start or all parameters are start
 * (i.e. at the origin).
 *
 * @param   p           pointer to the parameter array
 * @param   n           number of elements in the parameter array
 * @param   pd_array    pointer to the parameter defintions array
 *
 *
 * @return the plane index of the parameter axis the benchmark belongs to or -1
 * if on all axes (i.e. at origin), or -2 if on no boundaries
 */
int
param_on_axis(int *p,
              int n,
              struct pmm_paramdef *pd_array)
{
	int i;
	int plane = -1;

	for(i=0; i<n; i++) {
		// if a parameter is not a start
		if(p[i] != pd_array[i].start) {

			//
			// and if it is the first non-start parameter encountered then set
			// the possible axis index to the parameter's index
			//
			if(plane == -1) {
				plane = i; // set axis index
			}
			//
			// else if it is the second non-start encounted we can conclude
			// that the benchmark is not on parameter axis and return negative
			//
			else {
				return -2;
			}
		}
	}

	return plane;

}

/*!
 * Test if a set of parameters is within the limits defined by the parameter
 * definitions or outside them
 *
 * @param   p       pointer to parameter array
 * @param   n       number of parameters
 * @param   pd_array    pointer to parameter definition array
 *
 * @returns 1 if parameter p is within the parameter space defined by the
 * parameter definitions, 0 if it is not
 *
 */
int
params_within_paramdefs(int *p, int n, struct pmm_paramdef *pd_array)
{
    int i;

    for(i=0; i<n; i++) {
        if(param_within_paramdef(p[i], &pd_array[i]) == 0) {
            return 0;
        }
    }

    return 1;

}

/*!
 * Test if a single parameter is within the limits defined by a parameter
 * definition
 *
 * @param   p   the parameter
 * @param   pd  pointer to the parameter definition
 *
 * @return 1 if parameter is within the parameter space defined by the
 * parameter defintion. I.e. it is between the start and end points
 */
int
param_within_paramdef(int p, struct pmm_paramdef *pd) {
    int dist;

    dist = abs(pd->end - pd->start);

    if(abs(p - pd->end) > dist ||
       abs(p - pd->start) > dist)
    {
        return 0;
    }

    return 1;
}


/*!
 * Align a parameter according to stride and offset defined in corresponding
 * paramdef.
 *
 * @param   param       the value of the parameter we are aligning
 * @param   pd          the corresponding parameter definition
 *
 * @return the aligned parameter
 *
 * @post the return value is aligned as closely as possible to the passed param.
 * In cases where the aligned parameter is greater than the end, the parameter
 * is decremented by the stride until it is within the end bound. If nonzero en
 * is set, the end point is considered a valid point and the parameter is
 * aligned to this, regardless if it is within the stride & offset describing
 * the parameter sequence. If the aligned parameter is less than the start
 * it is set to the start value, regardless of whether this is within the stride
 * & offset describing the parameter sequence.
 * 
 */
int
align_param(int param, struct pmm_paramdef *pd)
{
    int aligned;
    //int end_exceeded = 0;

    // if stride is 1 we can return the parameter as it will always be aligned
    // while we limit input scalars to int types
    if(pd->stride == 1) {
        return param;
    }

    //
    // formula for alignment ...
    //
    // S is a sequence of the following form:
    //
    // S(x) = ax + b, x=0..
    //
    // where a = stride and b = offset
    //
    // to align a value p with the sequence, we must find the closet member
    // of S to p: p_s. This can be achieved with the following steps:
    //
    // 1. Solve S(n) = p, for n
    // 2. round n to get n_s, the position of the aligned value p_s in the
    //    sequence S
    // 3. calculate S(n_s) to get value of p_s
    //
    // i.e.
    //
    // S(n) = an + b
    // S(n) = p
    //
    // p = an + b
    // n = (p-b)/a
    //
    // n_s = round(n) = round((p-b)/a)
    //
    // p_s = S(n_s) = a * round((p-b)/a) + b
    //
    // thus, we have the below code the align the param in terms of the
    // offset and stride passed to this function
    //
    // a = stride, p = param, b = offset
    //
    // Note, round of integer division x/y = (x+y/2)/y ...
    aligned  =  pd->stride *
                (((param - pd->offset)+pd->stride/2) /
                                      pd->stride) +
                pd->offset;



    // if aligned parameter is greater than the end
    if(aligned >= pd->end) {
        //DBGPRINTF("greater than end:%d\n", aligned);
        aligned = pd->end;

        /*
        if(pd->nonzero_end == 1) {
            aligned = pd->end-1; //set to end as this is a valid point, even
                                 //if it is not in the sequence
        }
        else {
            aligned = pd->endx;
            while(aligned >= pd->end) { //decrement until it is less than end 
                aligned -= pd->stride;
            }
        }*/
    }

    if(aligned < pd->start) { // if aligned parameter is less than start
        //DBGPRINTF("less than start: %d\n", aligned);
        aligned = pd->start; //set to start 
    }


    //DBGPRINTF("param:%d aligned:%d\n", param, aligned);

    return aligned;
}

/*!
 * align an array of parameters
 *
 * @param   [in,out]    params  pointer to the array of parameters to align
 * @param   [in]        pd_set  pointer to the corresponding parameter
 *                              definition array
 */
void
align_params(int *params, struct pmm_paramdef_set *pd_set)
{
    int i;

    //DBGPRINTF("n_p:%d\n", n_p);

    for(i=0; i<pd_set->n_p; i++) {
        //DBGPRINTF("params[%d]:%d\n", i, params[i]);
        params[i] = align_param(params[i], &(pd_set->pd_array[i]));
    }

    return;
}

/*!
 * initialized a set of align a set of parameters based on
 * a set of unaligned parameters and parameter definitions
 *
 * @param   p       pointer to unaligned parameter array
 * @param   pd_set  pointer to corresponding parameter definitions
 *
 * @returns allocated array of aligned parameters or NULL on failure
 */
int*
init_aligned_params(int *p, struct pmm_paramdef_set *pd_set)
{
    int *aligned_p;

    aligned_p = init_param_array_copy(p, pd_set->n_p);
    if(aligned_p == NULL) {
        ERRPRINTF("Error copying param array.\n");
        return NULL;
    }

    align_params(aligned_p, pd_set);

    return aligned_p;
}

/*!
 * Check if two parameter definition sets are identical
 *
 * @param   pds_a   pointer to first parameter definition set
 * @param   pds_b   pointer to second parameter definition set
 *
 * @return 0 if not identical, 1 if identical
 */
int
isequal_paramdef_set(struct pmm_paramdef_set *pds_a,
                     struct pmm_paramdef_set *pds_b)
{
    if(pds_a->n_p != pds_b->n_p ||
       pds_a->pc_max != pds_b->pc_max ||
       pds_a->pc_min != pds_b->pc_min)
    {
        return 0;
    }

    if(pds_a->pc_formula != NULL && pds_b->pc_formula != NULL)
    {
        if(strcmp(pds_a->pc_formula, pds_b->pc_formula) != 0) {
            return 0;
        }
    }
    else if((pds_a->pc_formula == NULL && pds_b->pc_formula != NULL) ||
            (pds_a->pc_formula != NULL && pds_b->pc_formula == NULL))
    {
        return 0;
    }

    if(!isequal_paramdef_array(pds_a->pd_array, pds_b->pd_array, pds_a->n_p)) {
        return 0;
    }

    return 1;
}

/*!
 * Check if two parameter definition arrays are equal.
 *
 * @param   pd_array_a  pointer to first parameter array
 * @param   pd_array_b  pointer to second parameter array
 *
 * @return 0 if arrays contain different parameter definitions, 1 if they
 * are identical
 */
int
isequal_paramdef_array(struct pmm_paramdef *pd_array_a,
                       struct pmm_paramdef *pd_array_b, int n_p)
{
    int i;

    for(i=0; i<n_p; i++) {
        if(!isequal_paramdef(&(pd_array_a[i]), &(pd_array_b[i]))) {
            return 0;
        }
    }

    return 1;
}

/*!
 * Check if two parameter definitions are identical
 *
 * @param   a   pointer to first parameter definition
 * @param   b   pointer to second parameter definition
 * 
 * @return 0 if not identical, 1 if identical
 */
int
isequal_paramdef(struct pmm_paramdef *a, struct pmm_paramdef *b)
{

    if(strcmp(a->name, b->name) != 0 ||
       //a->type != b->type || TODO implement parameter types
       a->order != b->order ||
       a->nonzero_end != b->nonzero_end ||
       a->end != b->end ||
       a->start != b->start ||
       a->stride != b->stride ||
       a->offset != b->offset)
    {
        return 1;
    }

    return 0;
}

/*!
 * Compare two parameter arrays in terms of their elements. Arrays are equal if
 * all elements are equal, otherwise the most significant non-equal element
 * determines which is greater.
 *
 * @param   p1  pointer to first element of array 1
 * @param   p2  pointer to first element of array 2
 * @param   n   number of elements in arrays
 *
 * @return >0 if array p1 is 'greater' than p2, <0 if p1 is 'less' than 'p2' or
 * 0 of p1 is equal to p2.
 *
 * @pre arrays must be of the same length and not NULL
 *
 */
int
params_cmp(int *p1, int *p2, int n)
{
	int i;

    if(p1 == NULL || p2 == NULL) {
        ERRPRINTF("Parameter arrays should not be null.\n");
        exit(EXIT_FAILURE);
    }

	for(i=0; i<n; i++) {
		if(p1[i] < p2[i]) {
			return -1;
		}
        else if(p1[i] > p2[i]) {
            return 1;
        }
	}

	return 0; // at this point, all parameters are identical
}

/*!
 * Step between two points, according to a minimum step size defined in
 * the parameter definitions.
 *
 * @param   params      pointer to an array to store the parameters at the n-th
 *                      step between the start and end points
 * @param   start       pointer to array describing the start point
 * @param   end         pointer to array describing the start point
 * @param   step        number of steps to take along the interval (- to step
 *                      backwards, + to step forwards)
 * @param   pd_array    pointer to the parameter definition array
 * @param   n_p         number of parameters
 *
 * @return 0 on success, -1 if the step will exceed the end-point of the
 * interval or -2 on error
 */
int
set_params_step_between_params(int *params, int *start, int *end,
                               int step, struct pmm_paramdef_set *pd_set)
{
    int j;
    int min_stride;        //value of the minimum stride
    int min_stride_i;      //parameter index of the minimum stride
    int min_stride_i_dist; //distance from start and end minstride parameters
    int *direction;        //direction of stride for each parameter (+1/-1)

    //
    // The increment along the line from start->end will be achived by
    // adding a value to start. If start > end, for a component of that line
    // then that component should be decremented, not incremented. We control
    // this by way of a direction factor, +1 for increment, -1 for decriment
    //
    direction = malloc(pd_set->n_p * sizeof *direction);
    if(direction == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -2;
    }
    for(j=0; j<pd_set->n_p; j++) {
        if(start[j] > end[j]) {
            DBGPRINTF("parameter:%d will be decremented.\n", j);
            direction[j] = -1;
        }
        else {
            DBGPRINTF("parameter:%d will be incremented.\n", j);
            direction[j] = +1;
        }
    }

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
    for(j=0; j<pd_set->n_p; j++) {
        if(start[j] != end[j]) {
            if(pd_set->pd_array[j].stride < min_stride) {
                min_stride = pd_set->pd_array[j].stride;
                min_stride_i = j;
            }
        }
    }



    DBGPRINTF("min_stride:%d, min_stride_i:%d\n", min_stride, min_stride_i);
    if(min_stride_i == -1) {
        DBGPRINTF("start and end points identical, cannot stride.\n");

        free(direction);
        direction = NULL;

        return -1;
    }
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
    // let:
    //
    //   divisor = e_i-s_i;
    //
    // So finally:
    //   I_x = s_x + inc*((e_x-s_x)/divisor)
    //
    
    // to get the 1st/2nd/nth next/previous climb point
    min_stride = min_stride*step;
    min_stride_i_dist = end[min_stride_i] - start[min_stride_i];

    DBGPRINTF("step:%d min_stride:%d min_stride_i_dist:%d\n",
              step, min_stride, min_stride_i_dist);




    for(j=0; j<pd_set->n_p; j++) {
        params[j] = start[j] +  direction[j] *
                                    (min_stride *
                                        ((end[j] - start[j]) /
                                         min_stride_i_dist
                                        )
                                    );

        //             1 + (5/(300-1))*(1-1) = 
        //             1 + (5/(300-1))*(300-1) = 1+5 ...

        DBGPRINTF("start[%d]:%d end{%d]:%d direction[%d]:%d\n", j, start[j], j,
                  end[j], j, direction[j]);
        DBGPRINTF("start[min_stride_i]:%d end[min_stride_i]:%d\n",
                   start[min_stride_i], end[min_stride_i]);

        DBGPRINTF("params[%d]:%d\n", j, params[j]);
    }


    //check that params are within the range of the interval
    {
        int start_to_end;

        for(j=0; j<pd_set->n_p; j++) {
            start_to_end = abs(end[j] - start[j]);
            if(abs(end[j]-params[j]) > start_to_end ||
               abs(start[j]-params[j]) > start_to_end)
            {

                DBGPRINTF("stepped params are outside interval limits\n");

                free(direction);
                direction = NULL;

                return -1;
            }
        }
    }

    align_params(params, pd_set);

    DBGPRINTF("aligned params:\n");
    print_params(PMM_DBG, params, pd_set->n_p);

    free(direction);
    direction = NULL;

    return 0; // success

}

/*!
 * Print a parameter array
 *
 * @param   output  output stream to print to
 * @param   p       pointer to the parameter array
 * @param   n       size of the parameter array
 *
 */
void
print_params(const char *output, int *p, int n)
{
	int i;

	for(i=0; i<n; i++) {
		SWITCHPRINTF(output, "p[%d]: %d\n", i, p[i]);
	}
}

/*!
 * print a parameter definition array
 *
 * @param   output      output stream to print to
 * @param   pd_array    pointer to parameter definition array
 * @param   n           number of elements in array
 */
void
print_paramdef_array(const char *output, struct pmm_paramdef *pd_array, int n)
{
	int i;

    SWITCHPRINTF(output, "---paramdef-array---\n");
    SWITCHPRINTF(output, "n: %d\n", n);
	for(i=0; i<n; i++) {
		print_paramdef(output, &(pd_array[i]));
	}
}

/*!
 * print a parameter definition
 *
 * @param   output      output stream to print to
 * @param   pd          pointer to parameter definition
 */
void print_paramdef(const char *output, struct pmm_paramdef *pd)
{
	SWITCHPRINTF(output, "name :%s\n", pd->name);
	SWITCHPRINTF(output, "type :");

	if(pd->type == -1) {
        //TODO implement parameter types
	}
	SWITCHPRINTF(output,"order: %d\n", pd->order);
    SWITCHPRINTF(output, "nonzero_end: %d\n", pd->nonzero_end);
	SWITCHPRINTF(output, "end: %d\n", pd->end);
	SWITCHPRINTF(output, "start: %d\n", pd->start);
	SWITCHPRINTF(output, "stride: %d\n", pd->stride);
	SWITCHPRINTF(output, "offset: %d\n", pd->offset);
}

/*!
 * print a parameter definition set
 *
 * @param   output      output stream to print to
 * @param   pd_set      pointer to parameter definition set
 */
void print_paramdef_set(const char *output, struct pmm_paramdef_set *pd_set)
{
    SWITCHPRINTF(output, "n_p:%d\n", pd_set->n_p);
    
	print_paramdef_array(output, pd_set->pd_array, pd_set->n_p);

    if(pd_set->pc_formula != NULL) {
        SWITCHPRINTF(output, "pc_formula:%s\n", pd_set->pc_formula);
    } else {
        SWITCHPRINTF(output, "pc_formula:\n");
    }

    SWITCHPRINTF(output, "pc_max:%d\n", pd_set->pc_max);
    SWITCHPRINTF(output, "pc_min:%d\n", pd_set->pc_min);
}

/*!
 * frees a parameter definition set structure and members it contains
 *
 * @param   pd_set pointer to address of parameter definition structure
 */
void free_paramdef_set(struct pmm_paramdef_set **pd_set)
{

    if((*pd_set)->pd_array != NULL)
        free_paramdef_array(&(*pd_set)->pd_array, (*pd_set)->n_p);

    free((*pd_set)->pc_formula);
    (*pd_set)->pc_formula = NULL;

    free(*pd_set);
    *pd_set = NULL;

}

/*!
 * frees a parameter definition array
 *
 * @param   pd_array    pointer to address of parameter definition array
 * @param   n_p         number of parameter definitions in array
 */
void free_paramdef_array(struct pmm_paramdef **pd_array, int n_p) {
	int i;

	for(i=0; i<n_p; i++) {
		free((*pd_array)[i].name);
        (*pd_array)[i].name = NULL;
	}

	free(*pd_array);
    *pd_array = NULL;

}

