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
#ifndef PMM_ARGPARSER_H_
#define PMM_ARGPARSER_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#define PMM_VIEW_NULL 0
#define PMM_VIEW_PRINT_LIST 1
#define PMM_VIEW_DISPLAY_ROUTINE 2
#define PMM_VIEW_DISPLAY_FILE 3

typedef struct pmm_view_options {
    int action;
    int wait_period;
    int enter_interactive;
    int plot_average;
    int plot_intervals;
    int plot_params[2];
    int plot_params_index;

    int *slice_i_arr;
    long long int *slice_val_arr;
    int slice_arr_size;

    char *model_file;
    char *routine_name;
    char *config_file;
    char *plot_output_file;
} PMM_View_Options;

void usage();
void parse_args(struct pmm_config *cfg, int argc, char **argv);

void usage_pmm_view();
int
parse_pmm_view_args(struct pmm_view_options *options,
		                 int argc, char **argv);

#endif /*PMM_ARGPARSER_H_*/

