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
#if HAVE_CONFIG_H
#include "config.h"

/* undefine variables that are also defined in oct.h */
/* TODO rearrange octave headers so we don't need this so many places */
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#endif

#ifdef ENABLE_OCTAVE

#include <octave/oct.h>
#include <octave/octave.h>
#include <octave/parse.h>
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "pmm_octave.h"

extern "C" {
	#include "pmm_data.h"
	#include "pmm_log.h"
}

static int is_octave_initialised = 0;

void add_benchmark_to_xy(int pos, int n, struct pmm_benchmark *b, Matrix &x,
                         ColumnVector &y)
{
	int i;
	
	for(i=0; i<n; i++) {
		x(pos, i) = b->p[i];
	}
	y(pos) = b->flops;
	
}

extern "C"
struct pmm_octave_data*
fill_octave_input_matrices(struct pmm_model *m)
{
    int size;
    struct pmm_octave_data *oct_data;
    struct pmm_benchmark *b;

    oct_data = new PMM_Octave_Data;
    if(oct_data == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    size = count_benchmarks_in_model(m);

    oct_data->x = Matrix(size, m->n_p);
    oct_data->y = ColumnVector(size);

	//
	// fill x & y
	//
	int c = 0;
	
	
	// add points from the bench list
	b = m->bench_list->first;
	while(b != NULL && c <= size) {
		
		add_benchmark_to_xy(c, m->n_p, b, oct_data->x, oct_data->y);
		
		c++;
		b = b->next;
	}
	if(b != NULL) {
        ERRPRINTF("size mismatch, added %d of max %d benches (last:%p)\n",
                c, size, b);
        print_benchmark(b);
		return NULL;
	}

    return oct_data;

}

extern "C"
double
octave_interpolate(struct pmm_octave_data *oct_data, int *p, int n)
{

	int i;
    double flops;
	
	// init octave
    if(is_octave_initialised == 0) {
        string_vector argv (2);
        argv(0) = "octave_feval";
        argv(1) = "-q";

        octave_main (2, argv.c_str_vec(), 1);

        is_octave_initialised = 1;
    }
	
	// create a two row matrix for the lookup point
	// TODO for some reason we cannot interpolate at a single point successfully
	// interpolate at two identical points as a workaround
	Matrix xi = Matrix(2, n);
	
	for(i=0; i<n; i++) {
		xi(0, i) = p[i];
		xi(1, i) = p[i];
	}
	
	//set up input and output objects for call to griddatan octave call
	octave_value_list grid_in, grid_out;
	
    DBGPRINTF("Setting up grid_in(0)\n");
	grid_in(0) = octave_value(oct_data->x);
    DBGPRINTF("Done.\nSetting ut grid_in(1)\n");
	grid_in(1) = octave_value(oct_data->y);
    DBGPRINTF("Done.\nSetting up grid_in(2)\n");
	grid_in(2) = octave_value(xi);
    DBGPRINTF("Done.\nSetting up grid_in(3)\n");
	grid_in(3) = octave_value("linear");
    DBGPRINTF("Done.\n");
	
	
	//execute octave function
    DBGPRINTF("feval griddatan ...\n");
	grid_out = feval("griddatan", grid_in, 1);
    DBGPRINTF("Done.\n");
	
	Matrix yi;
	if(!error_state && grid_out.length() > 0) {
		yi = grid_out(0).matrix_value();

		flops =  yi(0);
        DBGPRINTF("-------- INTERPOLATED FLOPS --------\n");
        for(i=0; i<n; i++) {
            printf("p:%d:%d ", i, p[i]);
        }
        printf("flops:%f (%f)\n", flops, yi(1));

	}
	else {
		ERRPRINTF("Error calling griddatan in octave.\n");
		return -1.0;
	}

    return flops;
}

extern "C"
struct pmm_benchmark*
interpolate_griddatan(struct pmm_model *m, int *p)
{
	struct pmm_benchmark *ret_b;

    struct pmm_octave_data *oct_data;
	
	int i;
	
	// init octave
    if(is_octave_initialised == 0) {
        string_vector argv (2);
        argv(0) = "octave_feval";
        argv(1) = "-q";

        octave_main (2, argv.c_str_vec(), 1);

        is_octave_initialised = 1;
    }
	
    oct_data = fill_octave_input_matrices(m);

	
	// create a two row matrix for the lookup point
	// TODO for some reason we cannot interpolate at a single point successfully
	// interpolate at two identical points as a workaround
	Matrix xi = Matrix(2, m->n_p);
	
	//fill xi and the return benchmark
	ret_b = new_benchmark();
	ret_b->n_p = m->n_p;
	ret_b->p = (int*) malloc(ret_b->n_p * sizeof *(ret_b->p));

	for(i=0; i<m->n_p; i++) {
		xi(0, i) = p[i];
		xi(1, i) = p[i];
		
		ret_b->p[i] = p[i];
	}
	
	//set up input and output objects for call to griddatan octave call
	octave_value_list grid_in, grid_out;
	
	grid_in(0) = octave_value(oct_data->x);
	grid_in(1) = octave_value(oct_data->y);
	grid_in(2) = octave_value(xi);
	grid_in(3) = octave_value("linear");
	
	
	//execute octave function
	grid_out = feval("griddatan", grid_in, 1);
	
	Matrix yi;
	if(!error_state && grid_out.length() > 0) {
		yi = grid_out(0).matrix_value();

		ret_b->flops =  yi(0);
        DBGPRINTF("-------- INTERPOLATED BENCH --------\n");
        print_benchmark(ret_b);

		return ret_b;
	}
	else {
		ERRPRINTF("Error calling griddatan in octave.\n");
		free_benchmark(ret_b);
		return NULL;
	}
}
#endif /* ENABLE_OCTAVE */
