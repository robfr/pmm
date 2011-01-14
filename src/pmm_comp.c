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
 *
 * @file pmm_comp.c
 *
 * @brief Program to compare two models
 *
 * This file contains the pmm_comp program, which is a simple utility to
 * compare two models
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <gsl/gsl_statistics_double.h>


#include "pmm_data.h"
#include "pmm_octave.h"
#include "pmm_cfgparser.h"
#include "pmm_log.h"

/*!
 * structure storing options for pmm_comp tool
 */
typedef struct pmm_comp_options {
    char *approx_model_file;
    char *base_model_file;
} PMM_Comp_Options;


/*!
 * print command line usage for pmm_comp tool
 */
void
usage()
{
    printf("Usage: pmm_comp -a model_a -b model_b\n");
    printf("Options:\n");
    printf("  -a model_a     : approximation that will be compared\n");
    printf("  -b model_b     : base model which will be compared to\n");
    printf("\n");
}

/*!
 * parse arguments for pmm_comp tool
 *
 * @param   opts    pointer to options structure
 * @param   argc    number of command line arguments
 * @param   argv    command line arguments character array pointer
 */
void
parse_args(struct pmm_comp_options *opts, int argc, char **argv)
{
    int c;
    int option_index;

    opts->approx_model_file = (void*)NULL;
    opts->base_model_file = (void*)NULL;



    while(1) {
        static struct option long_options[] =
        {
            {"approx-model", required_argument, 0, 'a'},
            {"base-model", required_argument, 0, 'b'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        option_index = 0;

        c = getopt_long(argc, argv, "a:b:h", long_options, &option_index);

        // getopt_long returns -1 when arg list is exhausted
        if(c == -1) {
            break;
        }

        switch(c) {
            case 'a':
                opts->approx_model_file = optarg;
                break;
                
            case 'b':
                opts->base_model_file = optarg;
                break;

            case 'h':
                usage();
                exit(EXIT_SUCCESS);

            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    if(optind < argc) {
        usage();
        exit(EXIT_FAILURE);
    }

    if(opts->approx_model_file == NULL ||
       opts->base_model_file == NULL)
    {
        fprintf(stderr, "Error: model files must be specified.\n");
        usage();
        exit(EXIT_FAILURE);
    }

    return;
}

/*!
 * find the correlation between two models
 *
 * @param   approx_model    pointer to so called approximation model
 * @param   base_model      pointer to base model
 *
 * @return the correlation factor
 */
double
correlate_models(struct pmm_model *approx_model, struct pmm_model *base_model)
{
    int n; /* number of unique points in base model b */
    int c; /* counter */
    int i, j;
    double correlation;
    double *base_speed, *approx_speed;

    int **base_points;

    struct pmm_benchmark *b, *b_avg;
    struct pmm_octave_data *oct_data;

    octave_init();

    b = base_model->bench_list->first;

    n = count_unique_benchmarks_in_sorted_list(b);

    base_points = malloc(n * sizeof *base_points);
    base_speed = malloc(n * sizeof *base_speed);

    if(base_speed == NULL || base_points == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1.0;
    }

    oct_data = fill_octave_input_matrices(approx_model, PMM_ALL);
    if(oct_data == NULL) {
        ERRPRINTF("Error preparing octave input data.\n");
        return -1.0;
    }

    if(octave_triangulate(oct_data) < 0) {
        ERRPRINTF("Error calcuating triangulation of data.\n");
        return -1.0;
    }

    c = 0;
    while(b != NULL && c < n) {
        b_avg = get_avg_bench_from_sorted_bench_list(b, b->p);

        if(b_avg == NULL) {
            ERRPRINTF("Error getting average of benchmark:\n");
            print_benchmark(PMM_ERR, b);
            return -1.0;
        }

        base_speed[c] = b_avg->flops;

        base_points[c] = init_param_array_copy(b_avg->p, b_avg->n_p);

        free_benchmark(&b_avg); 

        b = get_next_different_bench(b);
        c++;
    }

    if(b != NULL && c != n-1) {
        ERRPRINTF("Error, unexpected number of benchmarks\n");
        return -1.0;
    }

    approx_speed = octave_interp_array(oct_data, base_points,
                                       base_model->n_p, n);

    if(approx_speed == NULL) {
        ERRPRINTF("Error interpolating approximation\n");
        return -1.0;
    }

    correlation = gsl_stats_correlation(base_speed, 1, approx_speed, 1, n);

    for(j=0; j<base_model->n_p; j++) 
        printf("p%d ", j);

    printf("base_speed approx_speed diff %%diff\n");

    for(i=0; i<n; i++) {

        for(j=0; j<base_model->n_p; j++)
            printf("%d ", base_points[i][j]);

        printf("%f %f %f %f\n", base_speed[i], approx_speed[i], fabs(base_speed[i]-approx_speed[i]), fabs(base_speed[i]-approx_speed[i])/base_speed[i]);
        free(base_points[i]);
        base_points[i] = NULL;
    }
    free(base_points);
    base_points = NULL;
    free(base_speed);
    base_speed = NULL;
    free(approx_speed);
    approx_speed = NULL;

    return correlation;
}

/*!
 * pmm_comp compares pairs of model files
 *
 * shows statistics such as correlation, number of points, time spent
 * benchmarking, etc.
 */
int
main(int argc, char **argv)
{

    struct pmm_comp_options opts;
    struct pmm_model *approx_model, *base_model;
    int ret;

    double correlation;

    parse_args(&opts, argc, argv);

    printf("approx model:%s\n", opts.approx_model_file);
    printf("base model:%s\n", opts.base_model_file);


    approx_model = new_model();
    base_model = new_model();

    if(approx_model == NULL || base_model == NULL) {
        ERRPRINTF("Error allocating new model.\n");
        exit(EXIT_FAILURE);
    }


    approx_model->model_path = opts.approx_model_file;
    base_model->model_path = opts.base_model_file;


    xmlparser_init();


    ret = parse_model(approx_model);
    if(ret == -1) {
        ERRPRINTF("Error file does not exist:%s\n", approx_model->model_path);
        exit(EXIT_FAILURE);
    }
    else if(ret < -1) {
        ERRPRINTF("Error parsing approx model.\n");
        exit(EXIT_FAILURE);
    }

    ret = parse_model(base_model);
    if(ret == -1) {
        ERRPRINTF("Error file does not exist:%s\n", base_model->model_path);
        exit(EXIT_FAILURE);
    }
    else if(ret < -1) {
        ERRPRINTF("Error base parsing base model.\n");
        exit(EXIT_FAILURE);
    }


    correlation = correlate_models(approx_model, base_model);

    printf("model correlation:%f\n", correlation);
    printf("base model points:%d\n", base_model->completion);
    printf("base model execution time:%f\n", calc_model_stats(base_model));
    printf("approx model points:%d\n", approx_model->completion);
    printf("approx model execution time:%f\n", calc_model_stats(approx_model));

    xmlparser_cleanup();

    return 0;
}
