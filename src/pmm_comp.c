/*
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <gsl/gsl_statistics_double.h>


#include "pmm_data.h"
#include "pmm_octave.h"
#include "pmm_cfgparser.h"
#include "pmm_log.h"

typedef struct pmm_comp_options {
    char *approx_model_file;
    char *base_model_file;
} PMM_Comp_Options;


void usage() {
    printf("Usage: pmm_comp -a model_a -b model_b\n");
    printf("Options:\n");
    printf("  -a model_a     : approximation that will be compared\n");
    printf("  -b model_b     : base model which will be compared to\n");
    printf("\n");
}

void parse_args(struct pmm_comp_options *opts, int argc, char **argv) {
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

double
correlate_models(struct pmm_model *approx_model, struct pmm_model *base_model)
{
    int n; /* number of unique points in base model b */
    int c; /* counter */
    double *base_speed, *approx_speed;
    struct pmm_benchmark *b, *b_avg;
    struct pmm_octave_data *oct_data;

    b = base_model->bench_list->first;

    n = count_unique_benchmarks_in_sorted_list(b);


    base_speed = malloc(n * sizeof *base_speed);
    approx_speed = malloc(n * sizeof *approx_speed);

    if(base_speed == NULL || approx_speed == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1.0;
    }

    oct_data = fill_octave_input_matrices(approx_model);
    if(oct_data == NULL) {
        ERRPRINTF("Error preparing octave input data.\n");
        return -1.0;
    }


    c = 0;
    while(b != NULL && c < n) {
        b_avg = get_avg_bench_from_sorted_bench_list(b, b->p);

        if(b_avg == NULL) {
            ERRPRINTF("Error getting average of benchmark:\n");
            print_benchmark(b);
            return -1.0;
        }

        base_speed[c] = b_avg->flops;

        approx_speed[c] = octave_interpolate(oct_data, b_avg->p, b_avg->n_p);


        free(b_avg); 

        b = get_next_different_bench(b);
        c++;
    }

    if(b != NULL && c != n-1) {
        ERRPRINTF("Error, unexpected number of benchmarks\n");
        return -1.0;
    }


    return gsl_stats_correlation(base_speed, 1, approx_speed, 1, n);
}

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


    correlation = correlate_models(base_model, approx_model);

    printf("model correlation:%f\n", correlation);

    xmlparser_cleanup();

    return 0;
}


