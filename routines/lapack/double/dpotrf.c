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
#include <time.h>
#include <cblas.h>
#include "pmm_util.h"

#ifndef ATLAS_ORDER
   #define ATLAS_ORDER CBLAS_ORDER
#endif
#ifndef ATLAS_UPLO
   #define ATLAS_UPLO CBLAS_UPLO
#endif
#ifndef ATLAS_DIAG
   #define ATLAS_DIAG CBLAS_DIAG
#endif

#include <clapack.h>

#define NARGS 1

/*
 * this benchmark is for the dpotrf cholesky factorization function of lapack
 */

int main(int argc, char **argv) {


	/* declare variables */
	double *A;              /* random matrix A */
	double *B;              /* postive definite matrix B (derived from A) */
	long long int c;        /* complexity */
    int n;                  /* matrix size of side */
    int i, j;               /* indexes */
    int ret;                /* dpotrf return status */
    char uplo;
    char trans;
    double alpha, beta;


	/* parse arguments */
	if(argc != NARGS+1) {
		return PMM_EXIT_ARGFAIL;
	}
	if(sscanf(argv[1], "%d", &n) == 0) {
		return PMM_EXIT_ARGPARSEFAIL;
	}

	/* calculate complexity */
	c = (n*n*(long long int)n)/3;


    /* allocate array */
    A = malloc((n * n) * sizeof *A);
    B = malloc((n * n) * sizeof *B);
    if(A == NULL || B == NULL) {
        printf("Could not allocate memory.\n");
        return PMM_EXIT_ALLOCFAIL;
    }

	/* initialise data */
    //srand(time(NULL));
    for(i=0; i<n; i++) {

        for(j=0;j<n; j++) { 
            /* column major order */
            A[n*j+i] = 10.0*(rand()/((double)RAND_MAX+1)); 
        }
    }
    uplo = 'l';
    trans = 't';
    alpha=1.0;
    beta=0.0;

    /* making positive definite matrix ... */ 
    /* dsyrk_(&uplo, &trans, &n, &n, &alpha, A, &n, &beta, B, &n); */
    cblas_dsyrk(CblasColMajor, CblasUpper, CblasTrans, n, n, alpha, A, n, beta, B, n);

    /*
        for(i=0; i<n; i++) {
            for(j=0; j<n; j++) {
                printf("%d:%f ", n*j+1, B[n*j+i]);
            }
            printf("\n");
        }
        */


    free(A);

	/* initialise timer */
	pmm_timer_init(c);

	/* start timer */
	pmm_timer_start();

	/* execute routine */
    /* dpotrf_(&uplo, &n, B, &n, &ret); */
    ret = clapack_dpotrf(CblasColMajor, CblasUpper, n, B, n);

	/* stop timer */
	pmm_timer_stop();

    /* check dpotrf return */
/*
        for(i=0; i<n; i++) {
            for(j=0; j<n; j++) {
                printf("%lf ", B[n*j+i]);
            }
            printf("\n");
        }
*/

    if(ret != 0) {
        printf("Error calling dpotrf: %d\n", ret);
        free(B);

        return PMM_EXIT_EXEFAIL;
    }

	/* get timing result */
	pmm_timer_result();

	/* destroy timer */
	pmm_timer_destroy();

	free(B);

	return PMM_EXIT_SUCCESS;
}
