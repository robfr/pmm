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
#include <cblas.h>

#include "pmm_util.h"

#define NARGS 2

/*
 * this version of the dgemm benchmark is for the multiplciation of two
 * matrices simulating a striped decomposition. A is an mxn (rowsxcols) stripe
 * of some hypothetical nxn matrix, B is a full nxn matric, and the result C is
 * an mxn stripe of the hypothetical result.
 *
 * The first input parameter is the size of the full B matrix, n (of nxn). The
 * second parameter is the size of the stripe of A/C m (of mxn)
 */
int main(int argc, char **argv) {


    /* declare variables */
    double *a, *b, *c;
    double arg[NARGS];
    size_t m, n;
    unsigned int i;
    long long int complexity;

    /* parse arguments */
    if(argc != NARGS+1) {
        return PMM_EXIT_ARGFAIL;
    }
    if(sscanf(argv[1], "%lf", &(arg[0])) == 0 ||
       sscanf(argv[2], "%lf", &(arg[1])) == 0) {
        return PMM_EXIT_ARGPARSEFAIL;
    }

    n = (size_t)arg[0];
    m = (size_t)arg[1];

    if(n<m) {
        printf("Size of matrix N must be greater/equal to size of stripe.\n");
        return PMM_EXIT_ARGFAIL;
    }

    /* calculate complexity */
    complexity = 2*n*m*(long long int)n;

    /* initialise data */
    a = malloc(m*n * sizeof *a);
    b = malloc(n*n * sizeof *b);
    c = malloc(m*n * sizeof *c);

    // fill the stripes
    for(i=0; i<m*n; i++) {
        a[i] = 2.0;
        b[i] = 1.0;
        c[i] = 0.0;
    }
    // fill the rest of b
    for(;i<n*n; i++) {
        b[i] = 1.0;
    }

    /* initialise timer */
    pmm_timer_init(complexity);

    /* start timer */
    pmm_timer_start();

    /* execute routine */
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, n, 1.0, a, n, b, n, 0.0, c, n);

    /* stop timer */
    pmm_timer_stop();

    /* get timing result */
    pmm_timer_result();

    /* destroy timer */
    pmm_timer_destroy();

    free(a);
    a = NULL;
    free(b);
    b = NULL;
    free(c);
    c = NULL;

    return PMM_EXIT_SUCCESS;
}
