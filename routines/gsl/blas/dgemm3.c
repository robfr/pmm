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
#include <stdlib.h>
#include <stdio.h>
#include "pmm_util.h"
//#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>

#define NARGS 3

/*
 * this version of the dgemm benchmark is for the multiplciation of two
 * matrices where their sizes are uninhibited. As a result the
 * complexity of the routine is dependent on 3 input paramateres
 */
int main(int argc, char **argv) {


    /* declare variables */
    gsl_matrix *A, *B, *C;
    double arg[NARGS];
    size_t m, n, k;
    long long c;
    int i;


    /* parse arguments */
    if(argc != NARGS+1) {
        return PMM_EXIT_ARGFAIL;
    }
    for(i=0; i<NARGS; i++) {
        if(sscanf(argv[i+1], "%lf", &arg[i]) == 0) {
            return PMM_EXIT_ARGPARSEFAIL;
        }
    }

    m = (size_t)arg[0];
    n = (size_t)arg[1];
    k = (size_t)arg[2];

    /* calculate complexity */
    c = 2*m*n*(long long)k;

    /* initialise data */
    A = gsl_matrix_alloc(m, n);
    B = gsl_matrix_alloc(n, k);
    C = gsl_matrix_alloc(m, k);


    gsl_matrix_set_all(A, 2.5);
    gsl_matrix_set_all(B, 4.9);
    gsl_matrix_set_zero(C);

    /* initialise timer */
    pmm_timer_init(c);

    /* start timer */
    pmm_timer_start();

    /* execute routine */
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, A, B, 0.0, C);

    /* stop timer */
    pmm_timer_stop();

    /* get timing result */
    pmm_timer_result();

    /* destroy timer */
    pmm_timer_destroy();

    gsl_matrix_free(A);
    gsl_matrix_free(B);
    gsl_matrix_free(C);

    return PMM_EXIT_SUCCESS;
}
