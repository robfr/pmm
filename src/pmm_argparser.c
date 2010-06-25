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
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "pmm_data.h"
#include "pmm_argparser.h"
#include "pmm_log.h"

/*******************************************************************************
 *
 */
int
parse_slice_str(char *slice_str, struct pmm_view_options *options);
int
count_tokens_in_str(char *str, char *delimiters);

void usage() {
	printf("Usage: pmmd [-dh] [-c file] [-l file]\n");
	printf("Options:\n");
	printf("  -c file    : specify config file\n");
	printf("  -l file    : specify log file\n");
	printf("  -d         : run in daemon (background) mode\n");
	printf("  -h         : print this help\n");
    printf("  -b         : exit after all models are built\n");
	printf("\n");
}

/*
 * function takes pointer to config structure and parses argument array using
 * getopt_long
 */
void parse_args(struct pmm_config *cfg, int argc, char **argv) {
	int c;
	int option_index;

	while(1) {
		static struct option long_options[] =
		{
			{"daemon", no_argument, 0, 'd'},
			{"config-file", required_argument, 0, 'c'},
			{"log-file", required_argument, 0, 'l'},
			{"help", no_argument, 0, 'h'},
            {"build-only", no_argument, 0, 'b'},
			{0, 0, 0, 0}
		};

		option_index = 0;

		c = getopt_long(argc, argv, "dc:l:hb", long_options, &option_index);

		// getopt_long returns -1 when arg list is exhausted
		if(c == -1) {
			break;
		}

		switch(c) {
		case 'd':
			cfg->daemon = 1;
			break;

		case 'c':
			cfg->configfile = optarg;
			break;

		case 'l':
			cfg->logfile = optarg;
			break;

        case 'b':
            cfg->build_only = 1;
            break;

		case 'h':
		default:
			usage();
			exit(EXIT_SUCCESS);
		}
	}

    if(optind < argc) {
        usage_pmm_view();
        exit(EXIT_FAILURE);
    }
}

void usage_pmm_view()
{
	printf("Usage: pmm_view -h | -c file [ -l | -r routine  [-p param -p ...] "
           "[-ai] [-I|-w wait]]\n");
	printf("Options:\n");
	printf("  -c file    : specify config file\n");
    printf("  -f model   : specify model file to plot\n");
	printf("  -h         : print this help\n");
    printf("  -l         : list routines\n");
	printf("  -r routine : specify routine to plot\n");
    printf("  -a         : plot averaged datapoints\n");
    printf("  -i         : plot intervals (not supported in slice plot)\n");
    printf("  -I         : enter interactive mode after plotting\n");
    printf("  -m         : plot maximum speed datapoints only\n");
    printf("  -p param   : specify a parameter axis to plot (max 2)\n");
    printf("  -P         : plot using palette\n");
    printf("  -s slice   : specify a slice of model to plot (see man page)\n");
	printf("  -w wait    : replot model every 'wait' seconds\n");
    printf("  -o file    : write plot to file (with png or ps extension)\n");
	printf("\n");
}

int
parse_pmm_view_args(struct pmm_view_options *options,
		                 int argc, char **argv)
{
	int c;
	int option_index;


    options->action = PMM_VIEW_NULL;
    options->enter_interactive = 0;
    options->wait_period = 0;
    options->config_file = (void*)NULL;
    options->plot_output_file = (void*)NULL;
    options->plot_average = 0;
    options->plot_intervals = 0;
    options->plot_params_index = -1;
    options->plot_max = 0;
    options->slice_arr_size = -1;
    options->plot_palette = 0;

	while(1) {
		static struct option long_options[] =
		{
			{"config-file", required_argument, 0, 'c'},
            {"model-file", required_argument, 0, 'f'},
			{"help", no_argument, 0, 'h'},
            {"list-routines", no_argument, 0, 'l'},
			{"routine", required_argument, 0, 'r'},
            {"plot-average", no_argument, 0, 'a'},
            {"plot-intervals", no_argument, 0, 'i'},
            {"interactive-mode", no_argument, 0, 'I'},
            {"plot-max", no_argument, 0, 'm'},
            {"param-index", required_argument, 0, 'p'},
            {"palette", no_argument, 0, 'P'},
            {"slice", required_argument, 0, 's'},
            {"wait-period", required_argument, 0, 'w'},
            {"output-file", required_argument, 0, 'o'},
			{0, 0, 0, 0}
		};

		option_index = 0;

		c = getopt_long(argc, argv, "c:f:hlr:aiImp:Ps:w:o:", long_options,
                        &option_index);

		// getopt_long returns -1 when arg list is exhausted
		if(c == -1) {
			break; //exit while loop
		}

		switch(c) {
		case 'c':
            options->config_file = optarg;
			break;

        case 'a':
            if(options->plot_max != 0) {
                ERRPRINTF("Cannot plot average and max at the same time.\n");
                return -1;
            }
            options->plot_average = 1;
            break;

        case 'i':
            options->plot_intervals = 1;
            break;

        case 'I':
            if(options->wait_period != 0) {
                ERRPRINTF("Cannot enter interactive mode when replotting is set"
                          "by -w <period>\n");
                return -1;
            }

            options->enter_interactive = 1;
            break;

		case 'r':
            if(options->action != PMM_VIEW_NULL) { //action should be unset
                return -1;
            }

            options->action = PMM_VIEW_DISPLAY_ROUTINE;
			options->routine_name = optarg;
			break;

        case 'm':
            if(options->plot_average != 0) {
                ERRPRINTF("Cannot plot average and max at the same time.\n");
                return -1;
            }
            options->plot_max = 1;
            break;

        case 'p':
            if(options->plot_params_index >= 1) {
                ERRPRINTF("cannot specify more than 2 parameter boundaries.\n");
                return -1;
            }

            if(options->slice_arr_size != -1) {
                ERRPRINTF("Cannot specify parameter boundary and slice plotting"
                          " at the same time.\n");
                return -1;
            }

            options->plot_params_index++;
            options->plot_params[options->plot_params_index] = atoi(optarg);
            break;

        case 'P':
            options->plot_palette = 1;
            break;

        case 's':
            if(options->plot_params_index != -1) {
                ERRPRINTF("Cannot specify parameter boundary and slice plotting"
                          " at the same time.\n");
                return -1;
            }

            if(parse_slice_str(optarg, options) < 0) {
                ERRPRINTF("Error parsing slice string\n");
                return -1;
            }
            break;

        case 'f':
            if(options->action != PMM_VIEW_NULL) { //action should be unset
                return -1;
            }
            options->action = PMM_VIEW_DISPLAY_FILE;
            options->model_file = optarg;
            break;

        case 'w':
            if(options->enter_interactive != 0) {
                ERRPRINTF("Cannot replot when interactive mode is set by -I\n");
                return -1;
            }
            options->wait_period = atoi(optarg);
            break;

        case 'l':
            if(options->action != PMM_VIEW_NULL) { //action should be unset
                return -1;
            }
            options->action = PMM_VIEW_PRINT_LIST;
            break;
        case 'o':
            options->plot_output_file = optarg;
            break;

		case 'h':
		default:
            return -1;
		}

	}

    if(optind < argc) {
        return -1;
    }

    //make sure an action was specified
    if(options->action == PMM_VIEW_NULL) {
        return -1;
    }

    return 0; //success
}

/*!
 * Count the number of tokens in a atring seperated by a delimiter
 *
 * @param   str         pointer to the string
 * @param   delimiters  pointer to a string listing the delimiters
 *
 * @return number of tokens or -1 on error
 *
 * \warning Not thread safe.
 */
int
count_tokens_in_str(char *str, char *delimiters)
{
    char *str_copy;
    char *token;
    int count;

    count = 0;

    str_copy = strdup(str);
    if(str_copy == NULL) {
        ERRPRINTF("Error duplicating string.\n");
        return -1;
    }

    token = strtok(str_copy, delimiters);
    while(token != NULL) {

        token = strtok(NULL, delimiters);
        count++;

    }

    free(str_copy);
    str_copy = NULL;
    
    return count;
}

/*!
 * Parse the slice string into a arrays of parameter index and parameter value
 * of a pmm_view_options structure.
 *
 * The slice string format is a comma seperated list of pairs. Each pair is
 * seperated by a colon. The first element of the pair describes a parameter
 * index in the form pN, where N is a number. The second element of the pair
 * is either min, max or a integer value, describing the value that parameter
 * should be fixed at.
 *
 * To specify a slice all but 1 or 2 parameters must have their values fixed
 * as specified by the string. If 1 parameter is left free the model will
 * be a 2d plot, if 2 parameters are left free the plot will be a 3d surface.
 *
 * @param   slice_str   pointer to the slice string
 * @param   options     pointer to the pmm_view_options structure
 *
 * @return 0 on success, -1 on failure
 *
 * \warning Not thread safe.
 */
int
parse_slice_str(char *slice_str, struct pmm_view_options *options)
{

    char *str_copy;
    const char *delimiters = ":,";
    char *token;
    enum states {SLICE_PINDEX, SLICE_PVALUE} state;


    int i;
    int n; //number of tokens
    int p_index;
    int p_value;



    n = count_tokens_in_str(slice_str, ",");
    if(n == -1) {
        ERRPRINTF("Error counting tokens in slice string.\n");
        return -1;
    }
    if(n == 0) {
        ERRPRINTF("No tokens in slice string.\n");
        return -1;
    }

    options->slice_i_arr = malloc(n * sizeof *(options->slice_i_arr));
    options->slice_val_arr = malloc(n * sizeof *(options->slice_val_arr));

    if(options->slice_i_arr == NULL || options->slice_val_arr == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1;
    }

    options->slice_arr_size = n;

    str_copy = strdup(slice_str);
    if(str_copy == NULL) {
        ERRPRINTF("Error duplicating slice string.\n");
        return -1;
    }

    state = SLICE_PINDEX;
    token = strtok(str_copy, delimiters);

    i=0;
    while(token != NULL) {
        DBGPRINTF("token:%s\n", token);
        switch(state) {
            case SLICE_PINDEX :
                if(strncmp(token, "p", 1) == 0) {
                    p_index = atoi(&(token[1]));
                    state = SLICE_PVALUE;
                }
                else {
                    ERRPRINTF("Error parsing slice string.\n");

                    free(str_copy);
                    str_copy = NULL;

                    return -1;
                }
                break;
                
            case SLICE_PVALUE :
                if(strcmp(token, "max") == 0) {
                    options->slice_i_arr[i] = p_index;
                    options->slice_val_arr[i] = -1;
                    i++;
                }
                else if(strcmp(token, "min") == 0) {
                    options->slice_i_arr[i] = p_index;
                    options->slice_val_arr[i] = -2;
                    i++;
                }
                else {

                    p_value = atoi(token);

                    options->slice_i_arr[i] = p_index;
                    options->slice_val_arr[i] = p_value;
                    i++;
                }

                DBGPRINTF("slice_i:%d slice_val:%d i:%d\n",
                           options->slice_i_arr[i-1],
                           options->slice_val_arr[i-1],
                           i-1);

                state = SLICE_PINDEX;

                break;

            default :
                break;
        }

        token = strtok(NULL, delimiters);

    }

    if(i != n) {
        ERRPRINTF("Unexpected number of slice tokens parsed (i:%d n:%d).\n",
                  i, n);

        free(str_copy);
        str_copy = NULL;

        return -1;
    }


    return 0;
}
