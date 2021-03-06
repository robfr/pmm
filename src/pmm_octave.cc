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
/*!
 * @file    pmm_octave.cc
 * @brief   interface between octave and pmm
 *
 * Contains code to permit pmm to call interpolation functions written in
 * octave
 */
#if HAVE_CONFIG_H
#include "config.h"

/* undefine variables that are also defined in oct.h */
/* TODO rearrange octave headers so we don't need this so many places */
#undef PACKAGE
#undef PACKAGE_URL
#undef VERSION
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
#undef PACKAGE
#undef PACKAGE_URL
#undef VERSION
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "pmm_octave.h"

extern "C" {
    #include "pmm_model.h"
    #include "pmm_log.h"
}

static int is_octave_initialised = 0; /*!< global initialization switch */


/*!
 * Intialize octave
 *
 * Initialize octave and load the custom griddatan octave function file
 *
 */
extern "C"
void octave_init()
{
    octave_value_list grid_in;
    string_vector argv (2);
    argv(0) = "octave_feval";
    argv(1) = "-q";

    octave_main (2, argv.c_str_vec(), 1);


    // source custom griddatan file
    grid_in(0) = PKGDATADIR "/pmm_griddatan.m";
    feval("source", grid_in, 1);

    is_octave_initialised = 1;
}

/*!
 * Add a benchmark to octave matrix and vector objects
 *
 * @param   pos     row in @a x and @a y to add benchmark to
 * @param   n       number of elements in parameter array of benchmark and
 *                  columns in @a x
 * @param   x       reference to matrix containing parameter values
 * @param   y       reference to column vector containing performance for
 *                  parameter values in @a x
 */
void add_benchmark_to_xy(int pos, int n, struct pmm_benchmark *b, Matrix &x,
                         ColumnVector &y)
{
    int i;

    for(i=0; i<n; i++) {
        x(pos, i) = b->p[i];
    }
    y(pos) = b->flops;

}


/*!
 *
 * Populate the members of an octave data structure with a model
 *
 * @param   m       pointer to model which will populate octave data objects
 * @param   mode    defines mode of operation, either adding all raw data
 *                  points in model (@a mode=0) or adding only averaged
 *                  data points in model (@a mode!=0)
 *
 * @returns a populated octave data structure
 */
extern "C"
struct pmm_octave_data*
fill_octave_input_matrices(struct pmm_model *m, int mode)
{
    int size;
    struct pmm_octave_data *oct_data;
    struct pmm_benchmark *b;
    struct pmm_benchmark *b_add;

    oct_data = new PMM_Octave_Data;
    if(oct_data == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    if(mode == 0) {
        size = count_benchmarks_in_model(m);
    } else {
        size = count_unique_benchmarks_in_sorted_list(m->bench_list->first);
    }

    oct_data->x = Matrix(size, m->n_p);
    oct_data->y = ColumnVector(size);

    //
    // fill x & y
    //
    int c = 0;


    // add points from the bench list
    b = m->bench_list->first;
    while(b != NULL && c <= size) {
        if(mode == PMM_AVG) {
            b_add = get_avg_bench_from_sorted_bench_list(b, b->p);

            if(b_add == NULL) {
                ERRPRINTF("Error getting average of benchmark:\n");
                print_benchmark(PMM_ERR, b);
                return NULL;
            }
        }
        else if(mode == PMM_MAX) {
            b_add = find_max_bench_in_sorted_bench_list(b, b->p);

            if(b_add == NULL) {
                ERRPRINTF("Error getting max of benchmark:\n");
                print_benchmark(PMM_ERR, b);
                return NULL;
            }
        }
        else {
            b_add = b;
        }

        add_benchmark_to_xy(c, m->n_p, b_add, oct_data->x, oct_data->y);
        c++;

        if(mode == PMM_AVG) {
            free_benchmark(&b_add);
            b = get_next_different_bench(b);
        }
        else if(mode == PMM_MAX) {
            b = get_next_different_bench(b);
        }
        else {
            b = b->next;
        }
    }
    if(b != NULL) {
        ERRPRINTF("size mismatch, added %d of max %d benches (last:%p)\n",
                c, size, b);
        print_benchmark(PMM_ERR, b);
        return NULL;
    }

    return oct_data;

}

/*!
 * Calcuate and store the triangulation of a matrix of points stored in the
 * pmm_octave_data structure using delaunayn octave function
 *
 * @param   oct_data    pointer to octave data structure containing matrix
 *                      of points, values at points and the triangulation
 *
 * @pre octave is initialized and oct_data contains valid matrix x
 * @post triangulation is stored in oct_data structure
 *
 * @return 0 on success, -1 on failure
 */
extern "C"
int
octave_triangulate(struct pmm_octave_data *oct_data)
{
    octave_value_list oct_in, oct_out;

    if(is_octave_initialised == 0) {
        ERRPRINTF("Octave not initialised.\n");
        return -1;
    }

    oct_in(0) = octave_value(oct_data->x);
    oct_in(1) = octave_value("QbB");

    oct_out = feval("delaunayn", oct_in, 1);

    if(!error_state && oct_out.length() > 0) {
        oct_data->tri = oct_out(0).matrix_value();

        return 0;
    }
    else {
        ERRPRINTF("Error calculating triangulation of data.\n");
        return -1;
    }
}

/*!
 * Interpolate a previously calculated triangulation at a set of points defined
 * by the array p using triinterpn octave function
 *
 * @param   oct_data    octave data structure containing data points and the
 *                      calculated triangulation of those points
 * @param   p           pointer to multi-dimensional array of points, each
 *                      element being an interpolation point
 * @param   n           number of dimensions of a single point
 * @param   l           number of points
 *
 * @return array of interpolated values at points described by p
 */
extern "C"
double*
octave_interp_array(struct pmm_octave_data *oct_data, int **p, int n, int l)
{

    int i, j;
    double *flops;

    // init octave
    if(is_octave_initialised == 0) {
        ERRPRINTF("Octave not initialised");
        return NULL;
    }

    flops = new double[l];
    if(flops == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return NULL;
    }

    // create a two row matrix for the lookup point
    // TODO for some reason we cannot interpolate at a single point successfully
    // interpolate at two identical points as a workaround
    Matrix xi = Matrix(l, n);

    for(i=0; i<l; i++) {
        for(j=0; j<n; j++) {
            xi(i, j) = p[i][j];
        }
    }

    //set up input and output objects for call to griddatan octave call
    octave_value_list oct_in, oct_out;

    oct_in(0) = octave_value(oct_data->tri);
    oct_in(1) = octave_value(oct_data->x);
    oct_in(2) = octave_value(oct_data->y);
    oct_in(3) = octave_value(xi);
    oct_in(4) = octave_value("linear");


    //execute octave function
    oct_out = feval("pmm_triinterpn", oct_in, 1);

    Matrix yi;
    if(!error_state && oct_out.length() > 0) {
        yi = oct_out(0).matrix_value();

        for(i=0; i<l; i++) {
            flops[i] = yi(i);
        }
    }
    else {
        ERRPRINTF("Error calling pmm_triinterpn in octave.\n");
        delete [] flops;
        return NULL;
    }

    return flops;
}

/*!
 * Interpolate a previously calculated triangulation at a single point
 * using triinterpn octave function
 *
 * @param   oct_data    octave data structure containing data points and the
 *                      calculated triangulation of those points
 * @param   p           pointer to an interpolation point array
 * @param   n           number of dimensions of the interpolation point
 *
 * @return value of interpolation at point @a p
 */
extern "C"
double
octave_interp(struct pmm_octave_data *oct_data, int *p, int n)
{

    int i;
    double flops;

    // init octave
    if(is_octave_initialised == 0) {
        ERRPRINTF("Octave not initialised");
        return -1.0;
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
    octave_value_list oct_in, oct_out;

    oct_in(0) = octave_value(oct_data->tri);
    oct_in(1) = octave_value(oct_data->x);
    oct_in(2) = octave_value(oct_data->y);
    oct_in(3) = octave_value(xi);
    oct_in(4) = octave_value("linear");


    //execute octave function
    oct_out = feval("pmm_triinterpn", oct_in, 1);

    Matrix yi;
    if(!error_state && oct_out.length() > 0) {
        yi = oct_out(0).matrix_value();

        flops =  yi(0);
        DBGPRINTF("-------- INTERPOLATED FLOPS --------\n");
        for(i=0; i<n; i++) {
            printf("p:%d:%d ", i, p[i]);
        }
        printf("flops:%f (%f)\n", flops, yi(1));

    }
    else {
        ERRPRINTF("Error calling pmm_triinterpn in octave.\n");
        return -1.0;
    }

    return flops;
}

/*!
 * interpolate the data stored in octave data structure at a
 * particular point using griddatan octave function
 *
 * @param   oct_data    pointer to data structure containing a point cloud
 * @param   p           pointer to an array describing point at which to
 *                      interpolate point cloud
 * @param   n           number of parameters of point
 */
extern "C"
double
octave_interpolate(struct pmm_octave_data *oct_data, int *p, int n)
{

    int i;
    double flops;

    // init octave
    if(is_octave_initialised == 0) {
        octave_init();
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
    DBGPRINTF("Done.\nSetting up grid_in(4)\n");
    grid_in(4) = octave_value("QbB");
    DBGPRINTF("Done.\n");


    //execute octave function
    DBGPRINTF("feval pmm_griddatan ...\n");
    grid_out = feval("pmm_griddatan", grid_in, 1);
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
        ERRPRINTF("Error calling pmm_griddatan in octave.\n");
        return -1.0;
    }

    return flops;
}

/*!
 * Create octave data structures from a model, interpolate that model and
 * look up the interpolation at a point using griddatan octave function
 *
 * @param   m       pointer to the model
 * @param   p       pointer to the parameter array describing the look up point
 *
 * @return pointer to a newly allocated benchmark representing the value of
 * the model at the lookup point or NULL on failure
 */
extern "C"
struct pmm_benchmark*
interpolate_griddatan(struct pmm_model *m, int *p)
{
    struct pmm_benchmark *ret_b;

    struct pmm_octave_data *oct_data;

    int i;

    // init octave
    if(is_octave_initialised == 0) {
        octave_init();
    }

    oct_data = fill_octave_input_matrices(m, PMM_ALL);


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
    grid_in(4) = octave_value("QbB");


    //execute octave function
    grid_out = feval("pmm_griddatan", grid_in, 1);

    Matrix yi;
    if(!error_state && grid_out.length() > 0) {
        yi = grid_out(0).matrix_value();

        ret_b->flops =  yi(0);
        DBGPRINTF("-------- INTERPOLATED BENCH --------\n");
        print_benchmark(PMM_DBG, ret_b);

        return ret_b;
    }
    else {
        ERRPRINTF("Error calling pmm_griddatan in octave.\n");
        free_benchmark(&ret_b);
        return NULL;
    }
}
#endif /* ENABLE_OCTAVE */
