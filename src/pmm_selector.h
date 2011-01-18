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
#ifndef PMM_SELECTOR_H_
#define PMM_SELECTOR_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "pmm_interval.h"
#include "pmm_load.h"


/*
int gbbp_select_new_bench(struct pmm_routine *r);
int gbbp_bench_from_interval(struct pmm_routine *r,
		                               struct pmm_interval *i);
void gbbp_insert_bench(struct pmm_loadhistory *h, struct pmm_routine *r,
		                   struct pmm_benchmark *b);
int naive_select_new_bench(struct pmm_routine *r);
*/

int* multi_random_select_new_bench(struct pmm_routine *r);
int rand_between(int min, int max);

int*
naive_1d_bisect_select_new_bench(struct pmm_routine *r);
int
init_naive_1d_intervals(struct pmm_routine *r);
int
naive_1d_bisect_insert_bench(struct pmm_routine *r, struct pmm_benchmark *b);

int*
multi_naive_select_new_bench(struct pmm_routine *r);
int
multi_naive_insert_bench(struct pmm_routine *r, struct pmm_benchmark *b);
int
naive_process_interval_list(struct pmm_routine *r, struct pmm_benchmark *b);
int
naive_step_interval(struct pmm_routine *r, struct pmm_interval *interval);

int*
multi_gbbp_naive_select_new_bench(struct pmm_routine *r);
int
init_gbbp_naive_intervals(struct pmm_routine *r);

int*
multi_gbbp_diagonal_select_new_bench(struct pmm_routine *r);
int
init_gbbp_diagonal_interval(struct pmm_routine *r);

int
isfuzzy_at_interval_end(struct pmm_interval *i, struct pmm_paramdef *pd_array,
                           int n);
int
adjust_interval_with_param_constraint_max(struct pmm_interval *i,
                                          struct pmm_paramdef_set *pd_set);
int
adjust_interval_with_param_constraint_min(struct pmm_interval *i,
                                          struct pmm_paramdef_set *pd_set);
int
isnonzero_at_interval_end(struct pmm_interval *i,
                          struct pmm_paramdef_set *pd_set);
int
project_diagonal_intervals(struct pmm_model *m);
struct pmm_interval*
new_projection_interval(int *p, struct pmm_paramdef *pd, int d,
                         int n);

int* multi_gbbp_select_new_bench(struct pmm_routine *r);
int
init_gbbp_boundary_intervals(struct pmm_routine *r);
int multi_gbbp_insert_bench(struct pmm_loadhistory *h, struct pmm_routine *r,
                            struct pmm_benchmark *b);

int
find_interval_matching_bench(struct pmm_routine *r, struct pmm_benchmark *b,
                             struct pmm_loadhistory *h,
                             struct pmm_interval **found_i);
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
process_gbbp_bisect(struct pmm_routine *r, struct pmm_interval *i,
                    struct pmm_benchmark *b, struct pmm_loadhistory *h);
int
process_gbbp_inflect(struct pmm_routine *r, struct pmm_interval *i,
                     struct pmm_benchmark *b, struct pmm_loadhistory *h);
int
process_it_point(struct pmm_routine *r, struct pmm_interval *i);

int
is_interval_divisible(struct pmm_interval *i, struct pmm_routine *r);

int
set_params_step_between_params(int *params, int *start, int *end,
                               int step, struct pmm_paramdef_set *pd_set);
int
set_params_step_along_climb_interval(int *params, int step, 
                             struct pmm_interval *i,
                             struct pmm_paramdef_set *pd_set);
int
multi_gbbp_bench_from_interval(struct pmm_routine *r,
		                       struct pmm_interval *interval,
                               int *params);
void
set_params_interval_midpoint(int *p, struct pmm_interval *i);

void mesh_boundary_models(struct pmm_model *m);
void recurse_mesh(struct pmm_model *m, int *p, int plane, int n_p);

#endif /*PMM_SELECTOR_H_*/

