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



/*
 * This file serves as a template to illustrate how a routine is to be called
 * inside a compiled program for use with the Performance Model Manager
 *
 * Program accepts one integer 'problem_size' as a parameter, no more no less.
 * This integer must be used to assign data and ultimately computations of a
 * proportional volume.
 *
 * In terms of Smart/Grid/Net-Solve, the integer relates to the complexity
 * of the routine. This program should determine the parameters with which
 * to call the routine using this integer.
 */


#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "pmm_util.h"

#define NARGS 1

int main(int argc, char **argv) {
	long long complexity;

	/* declare variables */
	double a;
	double b;
	double c;
	double i;

	/* parse arguments */
	if(argc != NARGS+1) {
		return PMM_EXIT_ARGFAIL;
	}
	if(sscanf(argv[1], "%lf", &i) == 0) {
		return PMM_EXIT_ARGPARSEFAIL;
	}

	/* calculate complexity */
	complexity = 2*(long long)i;

	/* initialise data */
	a = M_PI;
	b = M_E;

	/* initialise timer */
	pmm_timer_init(complexity);

	/* start timer */
	pmm_timer_start();

	/* execute routine */
	while(i>0.0) {
		c = a * b;
		i = i - 1.0;
	}

	/* stop timer */
	pmm_timer_stop();

	/* get timing result */
	pmm_timer_result();

	/* destroy timer */
	pmm_timer_destroy();

	return PMM_EXIT_SUCCESS;
}
