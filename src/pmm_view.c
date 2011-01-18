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
 * @file pmm_view.c
 * @author Robert Higgins
 * @date Sept 2009
 * @brief Basic viewer program for models constructed by PMM
 *
 * Program overview:
 *
 * Arguments:
 *   -r routine
 *   -c configuration file
 *   -m
 *   -h
 *
 *
 * Structure:
 *   Parse arguments, read configuration, find routine, plot history or model
 *
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

// TODO platform specific code follows, need to fix this
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/stat.h> // for stat
#include <fcntl.h>
#include <pthread.h>
#include <libgen.h>
#include <string.h>

#include "pmm_data.h"
#include "pmm_octave.h"
#include "pmm_interval.h"
#include "pmm_log.h"
#include "pmm_argparser.h"
#include "pmm_cfgparser.h"
#include "gnuplot_i.h"


void plot_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
           struct pmm_view_options *options);
void
plot_slice_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
            struct pmm_view_options *options);
void splot_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
            struct pmm_view_options *options);
void
plot_slice_interp_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
                        struct pmm_view_options *options);

int
bench_in_slice(struct pmm_view_options *options, struct pmm_benchmark *b);
void
splot_slice_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
            struct pmm_view_options *options);

void
set_splot_labels_ranges(gnuplot_ctrl *plot_handle, struct pmm_model *model,
                        char **plot_title_buf);
void
set_plot_labels_ranges(gnuplot_ctrl *plot_handle, struct pmm_model *model,
                       char **plot_title_buf);

int draw_plot_intervals(gnuplot_ctrl *plot_handle, struct pmm_model *model);
int draw_splot_intervals(gnuplot_ctrl *plot_handle, struct pmm_model *model);

void empty_model(struct pmm_model *m);

int
test_model_files_modified(struct pmm_model **m_array, int n);
int
test_model_file_modified(struct pmm_model *m);

// TODO remove reference to this global variable in pmm_data.c and put the
// history maniuplation in a seperate file that can be included only where
// needed (i.e. not here!)
pthread_rwlock_t history_rwlock;


/*!
 * Program first parses command line args and to determine the action to take
 * which is either:
 *
 * print list of models available for plotting (according to config file)
 * plot a model
 * print out the raw data of a model
 *
 */

int
main(int argc, char **argv) {

    struct pmm_view_options options;    // structure containing all options
	struct pmm_config *cfg;             // pointer to pmm configuration

	struct pmm_model **models;          // array of models to plot
    int n_p;                            // number of parameters to models

	gnuplot_ctrl *plot_handle;          // handle to the gnuplot process

    int ret;
	int i, j;

	// parse arguments
	ret = parse_pmm_view_args(&options, argc, argv);
    if(ret < 0) {
        printf("Error parsing arguments.\n");
        usage_pmm_view();
        exit(EXIT_FAILURE);
    }

    xmlparser_init();


    if(options.action == PMM_VIEW_PRINT_LIST) {

        cfg = new_config();

        if(options.config_file != NULL) {
            ret = asprintf(&(cfg->configfile), "%s", options.config_file);
            if(ret < 0) {
                ERRPRINTF("Error: asprintf failed with return code: %d\n", ret);
                exit(EXIT_FAILURE);
            }
        }

        //read config
        parse_config(cfg);

        //print routines
        printf("Routine list:\n");
        for(i=0; i<cfg->used; i++) {
            printf("%s\n", cfg->routines[i]->name);
        }

    }
    else if(options.action == PMM_VIEW_DISPLAY_ROUTINE ||
            options.action == PMM_VIEW_DISPLAY_FILE) {


        models = malloc(options.n_plots * sizeof *models);
        if(models == NULL) {
            ERRPRINTF("Error allocating memory.\n");
            exit(EXIT_FAILURE);
        }

        if(options.action == PMM_VIEW_DISPLAY_ROUTINE) {
            for(i = 0; i < options.n_plots; i++) {
                printf("Displaying model for routine: %s\n",
                        options.routine_names[i]);
            }

            cfg = new_config();

            if(options.config_file != NULL) {
                ret = asprintf(&(cfg->configfile), "%s", options.config_file);
                if(ret < 0) {
                    ERRPRINTF("Error: asprintf failed with return code: %d\n",
                              ret);
                    exit(EXIT_FAILURE);
                }
            }

            //read config
            parse_config(cfg);

            // for each routine specified on command line
            for(j = 0; j < options.n_plots; j++) {

                models[j] = NULL; // set this to null so we can test later

                // search for routine in config
                for(i = 0; i < cfg->used; i++) {

                    if(strcmp(options.routine_names[j], cfg->routines[i]->name)
                          == 0)
                    {
                        models[j] = cfg->routines[i]->model;
                    }
                }

                // if we done fine the routine, exit
                if(models[j] == NULL) {
                    printf("Failed to find model for routine: %s.\n",
                            options.routine_names[j]);
                    exit(EXIT_FAILURE);
                }


            }

        }
        else if(options.action == PMM_VIEW_DISPLAY_FILE) {
            for(i = 0; i < options.n_plots; i++) {
                models[i] = new_model();

                if(models[i] == NULL) {
                    ERRPRINTF("Error allocating model.\n");
                    exit(EXIT_FAILURE);
                }

                // use asprintf as free_model must free model_path pointer
                ret = asprintf(&(models[i]->model_path), "%s",
                               options.model_files[i]);
                if(ret < 0) {
                    ERRPRINTF("Error: asprintf failed with return code: %d\n",
                              ret);
                    exit(EXIT_FAILURE);
                }

                printf("Displaying model from file: %s\n",
                        options.model_files[i]);
            }
        }


        // parse models from files on disk
        n_p=0;
        for(i = 0; i < options.n_plots; i++) {

            // if model has not yet been parsed ...
            if(models[i]->completion == 0) {

                ret = parse_model(models[i]);
                if(ret == -1) {
                    ERRPRINTF("Error file does not exist:%s\n",
                            models[i]->model_path);
                    exit(EXIT_FAILURE);
                }
                else if(ret < -1) {
                    ERRPRINTF("Error parsing models[%d]: %s.\n",
                            i, models[i]->model_path);
                    exit(EXIT_FAILURE);
                }

            }

            // test number of  parameters are all the same and all not greater
            // than 2 (where no slices are specified)
            if(n_p == 0) {
                n_p = models[i]->n_p;

                if(n_p > 2 && options.slice_arr_size == -1) {
                    printf("Cannot plot full models in terms of >2 parameters, "
                           "specify a slice to view.\n");
                    exit(EXIT_FAILURE);
                }
            }
            else if(n_p != models[i]->n_p) {
                ERRPRINTF("Cannot plot models with different numbers of "
                          "parameters together");
                exit(EXIT_FAILURE);
            }

        }




        // init gnuplot handle and plot models
        plot_handle = gnuplot_init();

        if(options.wait_period <= 0) {


            if(options.plot_output_file != NULL) {

                // test extension
                {
                    char *ext;

                    ext = strrchr(options.plot_output_file, '.');
                    if(ext == NULL) {
                        printf("specify an extension (.ps or .png) to your "
                               "output file.\n");
                        exit(EXIT_FAILURE);
                    }

                    if(strcmp(ext, ".ps") == 0) {

                        if(options.plot_output_grayscale == 1) {
                            gnuplot_setterm(plot_handle,
                          "postscript enhanced monochrome font \"Helvetica\" 20");
                        }
                        else {
                            gnuplot_setterm(plot_handle,
                                            "postscript enhanced color font \"Helvetica\" 20");
                        }
                    }
                    else if( strcmp(ext, ".eps") == 0) {
                        if(options.plot_output_grayscale == 1) {
                            gnuplot_setterm(plot_handle,
                          "postscript eps enhanced monochrome font \"Helvetica\" 20");
                        }
                        else {
                            gnuplot_setterm(plot_handle,
                                            "postscript enhanced color font \"Helvetica\" 20");
                        }
                    }
                    else if(strcmp(ext, ".png") == 0) {
                        gnuplot_setterm(plot_handle, "png size 800,600");
                    }
                    else {
                        printf("specify an extension (.ps or .png) to your "
                               "output file.\n");
                        exit(EXIT_FAILURE);
                    }



                    printf("writing to file:%s\n",
                            options.plot_output_file);

                    gnuplot_cmd(plot_handle, "set output \"%s\"",
                            options.plot_output_file);
                }
            }


            if(options.slice_arr_size != -1) {
                if(options.slice_arr_size >= n_p) {
                    ERRPRINTF("Too many parameteres defined in slice (%d) of "
                              "model with %d parameters.\n",
                              options.slice_arr_size,
                              n_p);
                    exit(EXIT_FAILURE);
                }

                if(options.slice_arr_size < n_p-2) {
                    ERRPRINTF("Not enough parameters defined in slice (%d) of "
                              "model with %d parameters.\n",
                              options.slice_arr_size,
                              n_p);
                    exit(EXIT_FAILURE);
                }

                for(i=0; i<options.slice_arr_size; i++) {
                    if(options.slice_i_arr[i] < 0 ||
                       options.slice_i_arr[i] > n_p-1) {
                        ERRPRINTF("Slice parameter index %d out of bounds.\n",
                                  options.slice_i_arr[i]);
                        exit(EXIT_FAILURE);
                    }
                    else if(options.n_plots != 1) {
                        ERRPRINTF("Cannot plot slices for more than 1 model"
                                " at present.\n");
                        exit(EXIT_FAILURE);
                    }

                    if(options.slice_val_arr[i] < 0) {
                        if(options.action == PMM_VIEW_DISPLAY_FILE) {
                            ERRPRINTF("Cannot specify max/min in slice when "
                                      "plotting a model file directly (max/min "
                                      "only defined in config file.\n");
                            exit(EXIT_FAILURE);
                        }
                        else {
                            if(options.slice_val_arr[i] == -1) {
                                options.slice_val_arr[i] = models[0]->parent_routine->pd_set->pd_array[options.slice_i_arr[i]].end;
                            }
                            else if(options.slice_val_arr[i] == -2) {
                                options.slice_val_arr[i] = models[0]->parent_routine->pd_set->pd_array[options.slice_i_arr[i]].start;
                            }
                            else {
                                ERRPRINTF("Slice value should positive.\n");
                                exit(EXIT_FAILURE);
                            }
                        }


                    }
                }

                if(options.slice_arr_size == n_p-1) {
                    if(options.slice_interpolate == 0) {
                        plot_slice_model(plot_handle, models[0], &options);
                    }
                    else {
                        plot_slice_interp_model(plot_handle, models[0],
                                                &options);
                    }
                }
                else {
                    if(options.slice_interpolate == 0) {
                        ERRPRINTF("Interpolation plot of multidimensional "
                                  "model is not supported");
                        exit(EXIT_FAILURE);
                    }
                    else {
                        splot_slice_model(plot_handle, models[0], &options);
                    }
                }
            }
            else if(n_p == 1 || (n_p <= 1 && options.plot_params_index == 0))
            {
                for(i = 0; i < options.n_plots; i++) {
                    plot_model(plot_handle, models[i], &options);
                }

            }
            else if(n_p == 2 || (n_p <= 2 && options.plot_params_index == 1))
            {
                for(i = 0; i < options.n_plots; i++) {
                    splot_model(plot_handle, models[i], &options);
                }
            }


            // now enter interactive mode
            if(options.enter_interactive == 1) {
                gnuplot_prompt(plot_handle);
            }
            // or wait for user to decide termination
            // unless we have printed to file, exit immidiately then ...
            else if(options.plot_output_file == NULL) {
                printf("press any key to close ...\n");
                getchar();
            }
        }
        else {
            // this will initialise all mtimes for the models
            test_model_files_modified(models, options.n_plots);
            for(;;) {

                gnuplot_resetplot(plot_handle);

                if(n_p == 1) {
                    for(i = 0; i < options.n_plots; i++) {
                        plot_model(plot_handle, models[i], &options);
                    }

                }
                else if(n_p == 2) {
                    for(i = 0; i < options.n_plots; i++) {
                        splot_model(plot_handle, models[i], &options);
                    }
                }

                printf("replotting in %d secs (minimum).\n",
                       options.wait_period);

                sleep(options.wait_period);

                // if the models are not modified, don't bother
                // reploting, just sleep for the period until they are
                while((ret = test_model_files_modified(models, options.n_plots))
                       != 1)  {
                    DBGPRINTF("models not updated. Sleeping again ...\n");
                    sleep(options.wait_period);
                }
                if(ret < 0) {
                    ERRPRINTF("Error testing modification of model files\n");
                    exit(EXIT_FAILURE);
                }

                for(i = 0; i < options.n_plots; i++) {
                    empty_model(models[i]);
                    ret = parse_model(models[i]);
                    if(ret == -1) {
                        ERRPRINTF("Error: model file does not exist:%s\n",
                                  models[i]->model_path);
                    }
                    else if(ret < -1) {
                        ERRPRINTF("Error parsing model.\n");
                    }
                }
            }
        }


        if(options.action == PMM_VIEW_DISPLAY_ROUTINE) {
            free_config(&cfg);
        }
        else if(options.action == PMM_VIEW_DISPLAY_FILE) {
            for(i = 0; i < options.n_plots; i++) {
                free_model(&(models[i]));
            }
        }

        gnuplot_close(plot_handle);
    }

    xmlparser_cleanup();

    exit(EXIT_SUCCESS);


}

/*!
 * Test an array of models to see if their model files have been
 * modified. Note that we test all models and update the stored mtime
 * for each.
 *
 * @param   m       pointer to array of models
 * @param   n       number of models in array
 *
 * @return 0 if none of the files have been modified, 1 if any of the
 * model files have been modified, -1 on error.
 */
int
test_model_files_modified(struct pmm_model **m_array, int n)
{
    int i;
    int ret;
    int is_modified = 0;

    for(i=0; i<n; i++) {
        ret = test_model_file_modified(m_array[i]);
        if(ret == 1) {
            is_modified = 1;
        }
        else if(ret < 0) {
            ERRPRINTF("Error testing the mtime of %d-th model file.\n", i);
            print_model(PMM_ERR, m_array[i]);

            return -1;
        }
    }

    return is_modified;
}

/*!
 * Tests if a model file has been modified since the stored mtime
 *
 * @param   m   pointer to the model
 *
 * @return 0 if not modified, 1 if modified and -1 on error
 */
int
test_model_file_modified(struct pmm_model *m)
{

    int ret;
    struct stat file_stats;

    ret = stat(m->model_path, &file_stats);
    if(ret < 0) {
        ERRPRINTF("Error stat-ing file: %s\n", m->model_path);
        return -1;
    }

    if(m->mtime < file_stats.st_mtime) {
        m->mtime = file_stats.st_mtime;
        return 1;
    }
    else {
        return 0;
    }
}

/*!
 * free memeber structures and zero a model
 *
 * @param   m   pointer to model
 */
void
empty_model(struct pmm_model *m)
{

    if(m->bench_list != NULL) {
        free_bench_list(&(m->bench_list));
        m->bench_list = NULL;
    }

    if(m->interval_list != NULL) {
        free_interval_list(&(m->interval_list));
        m->interval_list = new_interval_list();
    }

    m->n_p = -1;
    m->completion = 0;
    m->complete = 0;
}

/*!
 * plot a model which is in terms of 1 parameter
 *
 * @param   plot_handle     pointer to gnuplot handle
 * @param   model           pointer to model to plot
 * @param   options         pointer to options for plot
 *
 */
void
plot_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
           struct pmm_view_options *options)
{
    double *x;
    double *y;
    int n, c;
	char *plot_title_buf;

    struct pmm_benchmark *b, *b_plot;

    if(options->plot_average == 1 ||
       options->plot_max == 1)
    {
        n = count_unique_benchmarks_in_sorted_list(model->bench_list->first);
    }
    else {
        n = model->bench_list->size;
    }

    y = malloc(n * sizeof *y);
    if(y == NULL) {
        ERRPRINTF("Error allocating memory y.\n");
        exit(EXIT_FAILURE);
    }

    x = malloc(n * sizeof *x);
    if(x == NULL) {
        ERRPRINTF("Error allocating memory to x.\n");
        exit(EXIT_FAILURE);
    }

    b = model->bench_list->first;


    c = 0;
    while(b != NULL && c < n) {

        if(options->plot_average == 1) {
            b_plot = get_avg_bench_from_sorted_bench_list(b, b->p);

            if(b_plot == NULL) {
                ERRPRINTF("Error getting average of benchmark:\n");
                print_benchmark(PMM_ERR, b);
                exit(EXIT_FAILURE);
            }
        }
        else if(options->plot_max == 1) {
            b_plot = NULL;
            b_plot = find_max_bench_in_sorted_bench_list(b, b->p);

            if(b_plot == NULL) {
                ERRPRINTF("Error getting max of benchmark:\n");
                print_benchmark(PMM_ERR, b);
                exit(EXIT_FAILURE);
            }
        }
        else {
            b_plot = b;
        }

        x[c] = (double)b->p[0];
        y[c] = b_plot->flops;

        printf("c:%d/%d x:%f y:%f\n", c, n, x[c], y[c]);

        c++;

        if(options->plot_average == 1)
        {
            free_benchmark(&b_plot);
            b = get_next_different_bench(b);
        }
        else if(options->plot_max == 1) {
            b = get_next_different_bench(b);
        }
        else {
            b = b->next;
        }

    }


    if(b != NULL && c != n-1) {
        printf("Error, unexpected number of benchmarks\n");
        exit(EXIT_FAILURE);
    }

    set_plot_labels_ranges(plot_handle, model, &plot_title_buf);

    if(options->plot_palette == 1) {
        gnuplot_cmd(plot_handle, "set palette");
        gnuplot_setstyle(plot_handle, "points palette");
    }
    else if(options->plot_style != NULL) {
        gnuplot_setstyle(plot_handle, options->plot_style);
    }

    gnuplot_plot_xy(plot_handle, x, y, n, plot_title_buf);

    if(options->plot_intervals == 1) {
        draw_plot_intervals(plot_handle, model);
    }

    free(plot_title_buf);
    plot_title_buf = NULL;

    free(x);
    x = NULL;

    free(y);
    y = NULL;
}

/*!
 * plot an interpolated one dimensional slice of a 2 parameter model
 *
 * @param   plot_handle     pointer to gnuplot handle
 * @param   model           pointer to model
 * @param   options         pointer to options
 *
 */
void
plot_slice_interp_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
                        struct pmm_view_options *options)
{
    int i, j;
    double *x;
    double *y;
    int n;
	char *plot_title_buf;
    int ret;

    int pfound;
    int p0; // the planes that we will plot (should those not speficied in
            // the slice
            //
    int max, min;


    struct pmm_octave_data *oct_data;
    double *approx_speeds;
    int **base_points;
    int mode;



    // find the free parameter of the model (which the plot will be
    // in terms of)
    p0=-1;
    for(i=0; i<model->n_p; i++) {
        pfound = 0;
        for(j=0; j<options->slice_arr_size; j++) {
            if(i==options->slice_i_arr[j]) {
                pfound = 1;
                break;
            }
        }

        if(pfound == 0) {
            if(p0 < 0) {
                p0 = i;
            }
            else {
                ERRPRINTF("Found too many free parameters.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(p0 < 0) {
        ERRPRINTF("Not found enough free parameters.\n");
    }


    // set max and min of parameter we will interpolate over
    min = model->pd_set->pd_array[p0].start;
    max = model->pd_set->pd_array[p0].end;

    n = options->slice_interpolate;

    mode = PMM_ALL;
    if(options->plot_average == 1) {
        mode = PMM_AVG;
    }
    else if(options->plot_max == 1) {
        mode = PMM_MAX;
    }

    base_points = malloc(n * sizeof *base_points);
    approx_speeds = malloc(n * sizeof *approx_speeds);
    x = malloc(n * sizeof *x);

    if(x == NULL || base_points == NULL || approx_speeds == NULL) {
        ERRPRINTF("Error allocating memory (n:%d).\n", n);
        exit(EXIT_FAILURE);
    }

    octave_init();

    // TODO fill with max, avg or all points in model
    oct_data = fill_octave_input_matrices(model, mode);
    if(oct_data == NULL) {
        ERRPRINTF("Error preparing octave input data.\n");
        exit(EXIT_FAILURE);
    }

    if(octave_triangulate(oct_data) < 0) {
        ERRPRINTF("Error calculating triangulation of data.\n");
        exit(EXIT_FAILURE);
    }

    // prep the interpolation array
    for(i=0; i<n; i++) {
        base_points[i] = malloc(model->n_p * sizeof(int));
        for(j=0; j<options->slice_arr_size; j++) {
            base_points[i][options->slice_i_arr[j]] = options->slice_val_arr[j];
        }

        x[i] = (double)min + (double)i*((max-min)/(double)n);
        base_points[i][p0] = x[i];
    }


    approx_speeds = octave_interp_array(oct_data, base_points, model->n_p, n);

    if(approx_speeds == NULL) {
        ERRPRINTF("Error interpolating model.\n");
        exit(EXIT_FAILURE);
    }

    y = approx_speeds;

    if(model->parent_routine != NULL) {
        ret = asprintf(&plot_title_buf, "%s model", model->parent_routine->name);
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }
    else {
        ret = asprintf(&plot_title_buf, "%s", basename(model->model_path));
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }

    if(options->plot_palette == 1) {
        gnuplot_cmd(plot_handle, "set palette");
        gnuplot_setstyle(plot_handle, "points palette");
    }
    else if(options->plot_style != NULL) {
        gnuplot_setstyle(plot_handle, options->plot_style);
    }

    gnuplot_plot_xy(plot_handle, x, y, n, plot_title_buf);

    free(plot_title_buf);
    plot_title_buf = NULL;

    free(x);
    x = NULL;

    free(approx_speeds);
    y = NULL; approx_speeds = NULL;

    for(i=0; i<n; i++) {
        free(base_points[i]);
        base_points[i] = NULL;
    }
    free(base_points);
    base_points = NULL;

}


/*!
 * plot a 2-d projection of a higher-d model
 *
 * @param   plot_handle     pointer to gnuplot control structure
 * @param   model           pointer to model to plot
 * @param   options         pointer to options structure
 */
void
plot_slice_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
            struct pmm_view_options *options)
{
    int i, j, c;
    double *x;
    double *y;
    int n;
	char *plot_title_buf;
    int ret;

    int pfound;
    int p0; // the planes that we will plot (should those not speficied in
            // the slice

    struct pmm_benchmark *b, *b_plot;


    // find the free parameter of the model (which the plot will be
    // in terms of)
    p0=-1;
    for(i=0; i<model->n_p; i++) {
        pfound = 0;
        for(j=0; j<options->slice_arr_size; j++) {
            if(i==options->slice_i_arr[j]) {
                pfound = 1;
                break;
            }
        }

        if(pfound == 0) {
            if(p0 < 0) {
                p0 = i;
            }
            else {
                ERRPRINTF("Found too many free parameters.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(p0 < 0) {
        ERRPRINTF("Not found enough free parameters.\n");
    }

    if(options->plot_average == 1 ||
       options->plot_max == 1)
    {
        n = count_unique_benchmarks_in_sorted_list(model->bench_list->first);
    }
    else {
        n = model->bench_list->size;
    }

    x = malloc(n * sizeof *x);
    y = malloc(n * sizeof *y);
    if(x == NULL || y == NULL) {
        ERRPRINTF("Error allocating memory (n:%d).\n", n);
        exit(EXIT_FAILURE);
    }

    c=0;
    b = model->bench_list->first;
    while(b != NULL) {

        // if bench b is not in the slice defined by options, get the next b
        while(b != NULL && bench_in_slice(options, b) == 0) {
            if(options->plot_average == 1 ||
               options->plot_max == 1)
            {
                b = get_next_different_bench(b);
            }
            else {
                b = b->next;
            }
        }
        if(b == NULL) {
            break;
        }


        if(options->plot_average == 1) {
            b_plot = get_avg_bench_from_sorted_bench_list(b, b->p);

            if(b_plot == NULL) {
                ERRPRINTF("Error getting average of benchmark!\n");
                print_benchmark(PMM_ERR, b);
                exit(EXIT_FAILURE);
            }
        }
        else if(options->plot_max == 1) {
            b_plot = NULL;
            b_plot = find_max_bench_in_sorted_bench_list(b, b->p);

            if(b_plot == NULL) {
                ERRPRINTF("Error getting max of benchmark:\n");
                print_benchmark(PMM_ERR, b);
                exit(EXIT_FAILURE);
            }
        }
        else {
            b_plot = b;
        }

        x[c] = (double)b->p[p0];
        y[c] = b_plot->flops;

        printf("c:%d/%d x:%f y:%f\n", c, n, x[c], y[c]);

        c++;

        if(options->plot_average == 1) {
            free_benchmark(&b_plot);
            b = get_next_different_bench(b);
        }
        else if(options->plot_max == 1) {
            b = get_next_different_bench(b);
        }
        else {
            b = b->next;
        }
    }

    if(c == 0) {
        ERRPRINTF("No benchmarks found along specified slice.\n");
        free(x);
        x = NULL;

        free(y);
        y = NULL;

        return;
    }

    if(model->parent_routine != NULL) {
        ret = asprintf(&plot_title_buf, "%s model", model->parent_routine->name);
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }
    else {
        ret = asprintf(&plot_title_buf, "%s", basename(model->model_path));
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }

    if(options->plot_palette == 1) {
        gnuplot_cmd(plot_handle, "set palette");
        gnuplot_setstyle(plot_handle, "points palette");
    }
    else if(options->plot_style != NULL) {
        gnuplot_setstyle(plot_handle, options->plot_style);
    }

    gnuplot_plot_xy(plot_handle, x, y, c, plot_title_buf);

    free(plot_title_buf);
    plot_title_buf = NULL;

    free(x);
    x = NULL;

    free(y);
    y = NULL;
}

/*!
 * plot a 3-d projection of a higher-d model
 *
 * @param   plot_handle     pointer to gnuplot control structure
 * @param   model           pointer to model to plot
 * @param   options         pointer to options structure
 */
void
splot_slice_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
            struct pmm_view_options *options)
{
    int i, j, c;
    double *x;
    double *y;
    double *z;
    int n;
	char *plot_title_buf;
    int ret;

    int pfound;
    int p0, p1; // the planes that we will plot (should those not speficied in
                // the slice

    struct pmm_benchmark *b, *b_plot;


    p0=-1; p1=-1;
    for(i=0; i<model->n_p; i++) {
        pfound = 0;
        for(j=0; j<options->slice_arr_size; j++) {
            if(i==options->slice_i_arr[j]) {
                pfound = 1;
                break;
            }
        }

        if(pfound == 0) {
            if(p0 < 0) {
                p0 = i;
            }
            else if (p1 < 0) {
                p1 = i;
            }
            else {
                ERRPRINTF("Found too many free parameters.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    if(p0 < 0 || p1 < 0) {
        ERRPRINTF("Not found enough free parameters.\n");
    }

    if(options->plot_average == 1 ||
       options->plot_max == 1)
    {
        n = count_unique_benchmarks_in_sorted_list(model->bench_list->first);

    }
    else {
        n = model->bench_list->size;
    }

    x = malloc(n * sizeof *x);
    y = malloc(n * sizeof *y);
    z = malloc(n * sizeof *z);
    if(x == NULL || y == NULL || z == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        exit(EXIT_FAILURE);
    }

    c=0;
    b = model->bench_list->first;
    while(b != NULL) {

        while(b != NULL && bench_in_slice(options, b) == 0) {
            if(options->plot_average == 1 ||
               options->plot_max == 1)
            {
                b = get_next_different_bench(b);
            }
            else {
                b = b->next;
            }
        }
        if(b == NULL) {
            break;
        }


        if(options->plot_average == 1) {
            b_plot = get_avg_bench_from_sorted_bench_list(b, b->p);

            if(b_plot == NULL) {
                ERRPRINTF("Error getting average of benchmark!\n");
                print_benchmark(PMM_ERR, b);
                exit(EXIT_FAILURE);
            }
        }
        else if(options->plot_max == 1) {
            b_plot = NULL;
            b_plot = find_max_bench_in_sorted_bench_list(b, b->p);

            if(b_plot == NULL) {
                ERRPRINTF("Error getting max of benchmark:\n");
                print_benchmark(PMM_ERR, b);
                exit(EXIT_FAILURE);
            }
        }
        else {
            b_plot = b;
        }

        x[c] = (double)b->p[p0];
        y[c] = (double)b->p[p1];
        z[c] = b_plot->flops;

        printf("c:%d/%d x:%f y:%f z:%f\n", c, n, x[c], y[c], z[c]);

        c++;

        if(options->plot_average == 1) {
            free_benchmark(&b_plot);
            b = get_next_different_bench(b);
        }
        else if(options->plot_max == 1) {
            b = get_next_different_bench(b);
        }
        else {
            b = b->next;
        }
    }

    if(c == 0) {
        ERRPRINTF("No benchmarks found along specified slice.\n");
        free(x);
        x = NULL;

        free(y);
        y = NULL;

        free(z);
        z = NULL;
        return;
    }

    if(model->parent_routine != NULL) {
        ret = asprintf(&plot_title_buf, "%s model", model->parent_routine->name);
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }
    else {
        ret = asprintf(&plot_title_buf, "%s", basename(model->model_path));
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }

    if(options->plot_palette == 1) {
        gnuplot_cmd(plot_handle, "set palette");
        gnuplot_setstyle(plot_handle, "points palette");
    }
    else if(options->plot_style != NULL) {
        gnuplot_setstyle(plot_handle, options->plot_style);
    }

    gnuplot_splot(plot_handle, x, y, z, c, plot_title_buf);

    free(plot_title_buf);
    plot_title_buf = NULL;

    free(x);
    x = NULL;

    free(y);
    y = NULL;

    free(z);
    z = NULL;

    return;
}

/*!
 * plot a 3-d model
 *
 * @param   plot_handle     pointer to gnuplot control structure
 * @param   model           pointer to model to plot
 * @param   options         pointer to options structure
 */
void
splot_model(gnuplot_ctrl *plot_handle, struct pmm_model *model,
            struct pmm_view_options *options)
{
    int c;
    double *x;
    double *y;
    double *z;
    int n;
	char *plot_title_buf;

    struct pmm_benchmark *b, *b_plot;

    // decide how many points will be in plot
    if(options->plot_average == 1 ||
       options->plot_max == 1)
    {
        n = count_unique_benchmarks_in_sorted_list(model->bench_list->first);
    }
    else {
        n = model->bench_list->size;
    }

    // allocate
    x = malloc(n * sizeof *x);
    y = malloc(n * sizeof *y);
    z = malloc(n * sizeof *z);
    if(x == NULL || y == NULL || z == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        exit(EXIT_FAILURE);
    }

    // read model into x,y,z
    c=0;
    b = model->bench_list->first;
    while(b != NULL) {

        // do we plot an average/max/raw benchmark
        if(options->plot_average == 1) {
            b_plot = get_avg_bench_from_sorted_bench_list(b, b->p);

            if(b_plot == NULL) {
                ERRPRINTF("Error getting average of benchmark!\n");
                print_benchmark(PMM_ERR, b);
                exit(EXIT_FAILURE);
            }
        }
        else if(options->plot_max == 1) {
            b_plot = NULL;
            b_plot = find_max_bench_in_sorted_bench_list(b, b->p);

            if(b_plot == NULL) {
                ERRPRINTF("Error getting max of benchmark:\n");
                print_benchmark(PMM_ERR, b);
                exit(EXIT_FAILURE);
            }
        }
        else {
            b_plot = b;
        }

        x[c] = (double)b->p[0];
        y[c] = (double)b->p[1];
        z[c] = b_plot->flops;

        printf("c:%d/%d x:%f y:%f z:%f\n", c, n, x[c], y[c], z[c]);

        c++;

        if(options->plot_average == 1) {
            free_benchmark(&b_plot);
            b = get_next_different_bench(b);
        }
        else if(options->plot_max == 1) {
            b = get_next_different_bench(b);
        }
        else {
            b = b->next;
        }
    }

    set_splot_labels_ranges(plot_handle, model, &plot_title_buf);

    if(options->plot_palette == 1) {
        gnuplot_cmd(plot_handle, "set palette");
        gnuplot_setstyle(plot_handle, "points palette");
    }
    else if(options->plot_style != NULL) {
        gnuplot_setstyle(plot_handle, options->plot_style);
    }

    //gnuplot_cmd(plot_handle, "set multiplot");

    // call the gnuplot plotting function
    gnuplot_splot(plot_handle, x, y, z, n, plot_title_buf);

    // intervals are not part of the plot but overlayed so must be done after
    if(options->plot_intervals == 1) {

        draw_splot_intervals(plot_handle, model);
    }

    free(plot_title_buf);
    plot_title_buf = NULL;

    free(x);
    x = NULL;

    free(y);
    y = NULL;

    free(z);
    z = NULL;
}

/*!
 * set labels, ranges and title for a 2-d plot
 *
 * @param   plot_handle     pointer to gnuplot structure
 * @param   pmm_model       pointer to model that is being plot
 * @param   plot_title_buf  pointer to character buffer that will contain title
 *
 * @return TODO fails with error but no return
 */
void
set_plot_labels_ranges(gnuplot_ctrl *plot_handle, struct pmm_model *model,
                       char **plot_title_buf)
{
    int ret;

    if(model->parent_routine != NULL) {

        gnuplot_set_xlabel(plot_handle,
                           model->parent_routine->pd_set->pd_array[0].name);

        gnuplot_cmd(plot_handle, "set xrange[0:%d]",
                    model->parent_routine->pd_set->pd_array[0].end >
                    model->parent_routine->pd_set->pd_array[0].start ?
                    model->parent_routine->pd_set->pd_array[0].end :
                    model->parent_routine->pd_set->pd_array[0].start);

        ret = asprintf(plot_title_buf, "%s model", model->parent_routine->name);
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }

    }
    else {
        ret = asprintf(plot_title_buf, "%s", basename(model->model_path));
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }

    gnuplot_cmd(plot_handle, "set yrange[-10:]");
    gnuplot_set_ylabel(plot_handle, "flops");
}

/*!
 * set labels, ranges and title for a 3-d plot
 *
 * @param   plot_handle     pointer to gnuplot structure
 * @param   pmm_model       pointer to model that is being plot
 * @param   plot_title_buf  pointer to character buffer that will contain title
 *
 * @return TODO fails with error but no return
 */
void
set_splot_labels_ranges(gnuplot_ctrl *plot_handle, struct pmm_model *model,
                        char **plot_title_buf)
{
    int ret;

    if(model->parent_routine != NULL) {
        gnuplot_set_xlabel(plot_handle,
                           model->parent_routine->pd_set->pd_array[0].name);
        gnuplot_set_ylabel(plot_handle,
                           model->parent_routine->pd_set->pd_array[1].name);

        gnuplot_cmd(plot_handle, "set xrange[0:%d]",
                    model->parent_routine->pd_set->pd_array[0].end >
                    model->parent_routine->pd_set->pd_array[0].start ?
                    model->parent_routine->pd_set->pd_array[0].end :
                    model->parent_routine->pd_set->pd_array[0].start);

        gnuplot_cmd(plot_handle, "set yrange[-10:%d]",
                    model->parent_routine->pd_set->pd_array[1].end >
                    model->parent_routine->pd_set->pd_array[1].start ?
                    model->parent_routine->pd_set->pd_array[1].end :
                    model->parent_routine->pd_set->pd_array[1].start);

        ret = asprintf(plot_title_buf, "%s model", model->parent_routine->name);
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }
    else {
        ret = asprintf(plot_title_buf, "%s", basename(model->model_path));
        if(ret < 0) {
            ERRPRINTF("Error setting plot title.\n");
        }
    }

    gnuplot_cmd(plot_handle, "set zrange[0:]");
    gnuplot_set_zlabel(plot_handle, "flops");
}

/*!
 * draw construction intervals on 2-d plots
 *
 * @param   plot_handle     pointer to gnuplot_ctrl structure
 * @param   model           pointer to model who's construction intervals are
 *                          to be plotted
 *
 * @return 0 on success (never fails TODO)
 */
int
draw_plot_intervals(gnuplot_ctrl *plot_handle, struct pmm_model *model)
{


    struct pmm_interval *interval;

    // clear arrows and labels
    gnuplot_cmd(plot_handle, "unset label");
    gnuplot_cmd(plot_handle, "unset arrow");

    gnuplot_cmd(plot_handle, "set label \'point\' at screen 0.1,0.10 right");
    gnuplot_cmd(plot_handle, "set label \' \' at screen 0.2,0.10 point ps 1");
    gnuplot_cmd(plot_handle, "set label \'empty\' at screen 0.1,0.08 right");
    gnuplot_cmd(plot_handle, "set arrow from screen 0.2,0.08 to "
                              "screen 0.3,0.08 nohead ls %d", IT_GBBP_EMPTY);

    gnuplot_cmd(plot_handle, "set label \'climb\' at screen 0.1,0.06 right");
    gnuplot_cmd(plot_handle, "set arrow from screen 0.2,0.06 to "
                              "screen 0.3,0.06 nohead ls %d", IT_GBBP_CLIMB);

    gnuplot_cmd(plot_handle, "set label \'bisect\' at screen 0.1,0.04 right");
    gnuplot_cmd(plot_handle, "set arrow from screen 0.2,0.04 to "
                              "screen 0.3,0.04 nohead ls %d", IT_GBBP_BISECT);

    gnuplot_cmd(plot_handle, "set label \'inflect\' at screen 0.1,0.02 right");
    gnuplot_cmd(plot_handle, "set arrow from screen 0.2,0.02 to "
                              "screen 0.3,0.02 nohead ls %d", IT_GBBP_INFLECT);

    interval = model->interval_list->top;
    while(interval != NULL) {

        print_interval(PMM_DBG, interval);
        switch(interval->type) {
            case IT_POINT:

                gnuplot_cmd(plot_handle, "set label \' \' at %f,%f "
                        "point ps 1",
                        (double)interval->start[0],
                        0);
                break;

            case IT_GBBP_EMPTY:
            case IT_GBBP_CLIMB:
            case IT_GBBP_BISECT:
            case IT_GBBP_INFLECT:

                gnuplot_cmd(plot_handle, "set arrow from %f,%f to "
                        "%f,%f nohead ls %d",
                        (double)interval->start[0],
                        0.0,
                        (double)interval->end[0],
                        0.0,
                        interval->type);

                break;

            default:
                break;
        }

        interval=interval->previous;
    }
    gnuplot_cmd(plot_handle, "replot");

    return 0; //success
}


/*!
 * draw construction intervals on 3-d plots
 *
 * @param   plot_handle     pointer to gnuplot_ctrl structure
 * @param   model           pointer to model who's construction intervals are
 *                          to be plotted
 *
 * @return 0 on success (never fails TODO)
 */
int
draw_splot_intervals(gnuplot_ctrl *plot_handle, struct pmm_model *model)
{


    struct pmm_interval *interval;

    // clear arrows and labels
    gnuplot_cmd(plot_handle, "unset label");
    gnuplot_cmd(plot_handle, "unset arrow");

    gnuplot_cmd(plot_handle, "set label \'point\' at screen 0.1,0.10 right");
    gnuplot_cmd(plot_handle, "set label \' \' at screen 0.2,0.10 point ps 1");
    gnuplot_cmd(plot_handle, "set label \'empty\' at screen 0.1,0.08 right");
    gnuplot_cmd(plot_handle, "set arrow from screen 0.2,0.08 to "
                              "screen 0.3,0.08 nohead ls %d", IT_GBBP_EMPTY);

    gnuplot_cmd(plot_handle, "set label \'climb\' at screen 0.1,0.06 right");
    gnuplot_cmd(plot_handle, "set arrow from screen 0.2,0.06 to "
                              "screen 0.3,0.06 nohead ls %d", IT_GBBP_CLIMB);

    gnuplot_cmd(plot_handle, "set label \'bisect\' at screen 0.1,0.04 right");
    gnuplot_cmd(plot_handle, "set arrow from screen 0.2,0.04 to "
                              "screen 0.3,0.04 nohead ls %d", IT_GBBP_BISECT);

    gnuplot_cmd(plot_handle, "set label \'inflect\' at screen 0.1,0.02 right");
    gnuplot_cmd(plot_handle, "set arrow from screen 0.2,0.02 to "
                              "screen 0.3,0.02 nohead ls %d", IT_GBBP_INFLECT);

    interval = model->interval_list->top;
    while(interval != NULL) {

        print_interval(PMM_DBG, interval);
        switch(interval->type) {
            case IT_POINT:

                gnuplot_cmd(plot_handle, "set label \' \' at %f,%f,%f "
                        "point ps 1",
                        (double)interval->start[0],
                        (double)interval->start[1],
                        0);
                break;

            case IT_GBBP_EMPTY:
            case IT_GBBP_CLIMB:
            case IT_GBBP_BISECT:
            case IT_GBBP_INFLECT:

                gnuplot_cmd(plot_handle, "set arrow from %f,%f,%f to "
                        "%f,%f,%f nohead ls %d",
                        (double)interval->start[0],
                        (double)interval->start[1],
                        0.0,
                        (double)interval->end[0],
                        (double)interval->end[1],
                        0.0,
                        interval->type);

                break;

            default:
                break;
        }

        interval=interval->previous;
    }
    gnuplot_cmd(plot_handle, "replot");

    return 0; //success
}
/*!
 * check if a benchmark belongs to a slice defined in a pmm_view_options
 * structure
 *
 * @param   options     pointer to the options structure containing slice def.
 * @param   b           pointer to the benchmark
 *
 * @return 0 if benchmark does not belong, 1 if benchmark does belong
 */
int
bench_in_slice(struct pmm_view_options *options, struct pmm_benchmark *b)
{
    int i;

    for(i=0; i<options->slice_arr_size; i++) {

        DBGPRINTF("i:%d slice_i_arr[i]:%d b-p[]:%d slice_val:%d\n",
                  i, options->slice_i_arr[i], b->p[options->slice_i_arr[i]],
                  options->slice_val_arr[i]);

        if(b->p[options->slice_i_arr[i]] == options->slice_val_arr[i]) {

        }
        else {
            return 0; //false
        }
    }

    return 1; //true
}

