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
#include <cblas.h>

#define NARGS 1

/*
 * this version of the dgemm benchmark is for the multiplciation of two square
 * matrices of the same exact size, i.e. the complexit is in terms of 1
 * parameter
 */

int main(int argc, char **argv) {


	/* declare variables */
	double *a, *b, *c;
	double arg;
	size_t n;
    int i;
	long long complexity;


	/* parse arguments */
	if(argc != NARGS+1) {
		return PMM_EXIT_ARGFAIL;
	}
	if(sscanf(argv[1], "%lf", &arg) == 0) {
		return PMM_EXIT_ARGPARSEFAIL;
	}

	n = (size_t)arg;

	/* calculate complexity */
	complexity = 3*n*n*(long long)n;

	/* initialise data */
    a = malloc(n*n * sizeof *a);
    b = malloc(n*n * sizeof *b);
    c = malloc(n*n * sizeof *c);


    for(i=0; i<n*n; i++) {
        a[i] = n/2.0;
        b[i] = n/1.0;
        c[i] = 0.0;
    }

	/* initialise timer */
	pmm_timer_init(complexity);

	/* start timer */
	pmm_timer_start();

	/* execute routine */
	cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, n, n, n, 1.0,
                   a, n, b, n, 0.0, c, n);

	/* stop timer */
	pmm_timer_stop();

	/* get timing result */
	pmm_timer_result();

	/* destroy timer */
	pmm_timer_destroy();

    free(a);
    free(b);
    free(c);

	return PMM_EXIT_SUCCESS;
}
