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
 * @file   pmm_param.h
 * @brief  data structures for model/benchmark parameters
 *
 */
#ifndef PMM_PARAM_H_
#define PMM_PARAM_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MUPARSER
//#include "pmm_muparse.h"
#endif

#ifdef HAVE_MUPARSER
struct pmm_param_constraint_muparser;   //< forward declaration
#endif
/*!
 * Structure defining a parameter of a routine
 */
typedef struct pmm_paramdef {
    /*
     * TODO allow definition of a mathematical sequence to govern permissable
     * parameters. At present stride and offset allow a sequence equvilent
     * to: a*x + b where a = stride and b = offset
     */
	char *name;         /*!< name of parameter */
	int type;           /*!< */
	int order;          /*!< order in the sequence of parameters passed to
                             benchmarking routine */
    int nonzero_end;    /*!< toggle assumtion of zero speed at parameter end
                             value */
	int end;            /*!< ending value of parameter range */
	int start;          /*!< starting value of parameter range */
    int stride;         /*!< stride to use traversing parameter range */
    int offset;         /*!< offset from starting value to use traversing
                             parameter range */
} PMM_Paramdef;

/*!
 * structure describing a set of parameters and a formula in terms of
 * those parameters which may be constrained (i.e. parameters a, b, formula:a*b,
 * constraint on formula < 1000)
 */
typedef struct pmm_paramdef_set {
    int n_p;                            /*!< number of parameters in set */
    struct pmm_paramdef *pd_array;      /*!< array of parameter definitions */
    char *pc_formula;                   /*!< formula for parameter constraint */
    int pc_max;                         /*!< max of parameter constraint */
    int pc_min;                         /*< min of parameter constraint */

#ifdef HAVE_MUPARSER
    struct pmm_param_constraint_muparser *pc_parser; /*!< muparser structure
                                                          used for parameter
                                                          constraint */
#endif

} PMM_Paramdef_Set;

struct pmm_paramdef_set*
new_paramdef_set();

int*
init_param_array_start(struct pmm_paramdef_set *pds);
int*
init_param_array_end(struct pmm_paramdef_set *pds);

void
set_param_array_start(int *p, struct pmm_paramdef_set *pds);
void
set_param_array_end(int *p, struct pmm_paramdef_set *pds);

int
copy_paramdef(struct pmm_paramdef *dst, struct pmm_paramdef *src);

int*
init_param_array_copy(int *src, int n);
void
set_param_array_copy(int *dst, int *src, int n);

int params_cmp(int *p1, int *p2, int n);

void
align_params(int *params, struct pmm_paramdef_set *pd_set);
int
align_param(int param, struct pmm_paramdef *pd);
int*
init_aligned_params(int *p, struct pmm_paramdef_set *pd_set);

int
param_on_axis(int *p,
              int n,
              struct pmm_paramdef *pd_array);

int
params_within_paramdefs(int *p, int n, struct pmm_paramdef *pd_array);
int
param_within_paramdef(int p, struct pmm_paramdef *pd_array);

int
isequal_paramdef_set(struct pmm_paramdef_set *pds_a,
                     struct pmm_paramdef_set *pds_b);
int
isequal_paramdef_array(struct pmm_paramdef *pd_array_a,
                       struct pmm_paramdef *pd_array_b, int n_p);
int
isequal_paramdef(struct pmm_paramdef *a, struct pmm_paramdef *b);

int
set_params_step_between_params(int *params, int *start, int *end,
                               int step, struct pmm_paramdef_set *pd_set);

void print_params(const char *output, int *p, int n);
void print_paramdef_set(const char *output, struct pmm_paramdef_set *pd_set);
void print_paramdef_array(const char *output, struct pmm_paramdef *pd_array, int n);
void print_paramdef(const char *output, struct pmm_paramdef *pd);

void free_paramdef_set(struct pmm_paramdef_set **pd_set);
void free_paramdef_array(struct pmm_paramdef **pd_array, int n_p);

#endif /*PMM_PARAM_H_*/

