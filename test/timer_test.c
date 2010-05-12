#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "pmm_util.h"

#define NARGS 1

int main(int argc, char **argv) {


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

	printf("%f\n", i);

	/* initialise data */
	a = M_PI;
	b = M_E;

	/* initialise timer */
	pmm_timer_init();

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
