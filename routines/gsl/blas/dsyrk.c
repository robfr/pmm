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
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "pmm_util.h"
//#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>

#ifdef HAVE_SETAFFINITY
    #include <sched.h>
#endif

#define NARGS 1

int main(int argc, char **argv) {


    /* declare variables */
    gsl_matrix *A, *C;
    double arg;
    size_t n;
    unsigned int i,j;
    long long complexity;

#ifdef HAVE_SETAFFINITY
    cpu_set_t aff_mask;
#endif


    /* parse arguments */
    if(argc != NARGS+1) {
        return PMM_EXIT_ARGFAIL;
    }
    if(sscanf(argv[1], "%lf", &arg) == 0) {
        return PMM_EXIT_ARGPARSEFAIL;
    }

    n = (size_t)arg;

    /* calculate complexity */
    complexity = n*n*(long long)n;

#ifdef HAVE_SETAFFINITY
    /* set processor affinity */
    CPU_ZERO(&aff_mask);
    CPU_SET(0, &aff_mask);

    if(sched_setaffinity(0, sizeof(aff_mask), &aff_mask) < 0) {
        printf("could not set affinity!\n");
        return PMM_EXIT_ARGFAIL;
    }
#endif



    /* initialise data */
    A = gsl_matrix_alloc(n, n);
    C = gsl_matrix_alloc(n, n);


    for(i=0; i<n; i++) {
        for(j=0; j<n; j++) {
            gsl_matrix_set(A, i, j, 10.0*(rand()/((double)RAND_MAX+1)));

            if(j<n-i) {
                gsl_matrix_set(C, i, j, 10.0*(rand()/((double)RAND_MAX+1)));
            }
        }
    }
    //gsl_matrix_set_all(A, 2.5);
    //gsl_matrix_set_all(C, 7.3);

    /* initialise timer */
    pmm_timer_init(complexity);

    /* start timer */
    pmm_timer_start();

    /* execute routine */
    gsl_blas_dsyrk(CblasUpper, CblasNoTrans, 1.0, A, 1.0, C);

    /* stop timer */
    pmm_timer_stop();

    /* get timing result */
    pmm_timer_result();

    /* destroy timer */
    pmm_timer_destroy();

    gsl_matrix_free(A);
    gsl_matrix_free(C);

    return PMM_EXIT_SUCCESS;
}
