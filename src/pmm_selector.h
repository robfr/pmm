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
#include "pmm_model.h"
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

int*
naive_1d_bisect_select_new_bench(struct pmm_routine *r);
int
naive_1d_bisect_insert_bench(struct pmm_routine *r, struct pmm_benchmark *b);

int*
multi_naive_select_new_bench(struct pmm_routine *r);
int
multi_naive_insert_bench(struct pmm_routine *r, struct pmm_benchmark *b);

int*
multi_gbbp_naive_select_new_bench(struct pmm_routine *r);
int*
multi_gbbp_diagonal_select_new_bench(struct pmm_routine *r);
int*
multi_gbbp_select_new_bench(struct pmm_routine *r);

int
multi_gbbp_insert_bench(struct pmm_loadhistory *h, struct pmm_routine *r,
                        struct pmm_benchmark *b);





#endif /*PMM_SELECTOR_H_*/

