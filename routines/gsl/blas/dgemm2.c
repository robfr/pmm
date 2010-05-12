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

#define NARGS 2

/*
 * this version of the dgemm benchmark is for the multiplciation of two
 * matrices where their sizes are transpositions of eachother. I.e. if
 * Matrix A's size is MxN, then Matrix B's size is NxM and their product
 * will give a matrix of size MxM
 */
int main(int argc, char **argv) {


	/* declare variables */
	gsl_matrix *A, *B, *C;
	double arg[NARGS];
	size_t m, n;
	long long int c;


	/* parse arguments */
	if(argc != NARGS+1) {
		return PMM_EXIT_ARGFAIL;
	}
	if(sscanf(argv[1], "%lf", &(arg[0])) == 0 ||
       sscanf(argv[2], "%lf", &(arg[1])) == 0) {
		return PMM_EXIT_ARGPARSEFAIL;
	}

	m = (size_t)arg[0];
	n = (size_t)arg[1];

	/* calculate complexity */
	c = 2*m*n*(long long int)m;

	/* initialise data */
	A = gsl_matrix_alloc(m, n);
	B = gsl_matrix_alloc(n, m);
	C = gsl_matrix_alloc(m, m);


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
