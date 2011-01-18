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
 * @file   pmm_model.h
 * @brief  general PMM data structures
 *
 * Data structures representing models, routines and the benchmarking
 * server configuration are described here, along with functions that
 * operate on them.
 *
 */

#ifndef PMM_DATA_H_
#define PMM_DATA_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>           // for timeval

#include "pmm_interval.h"
#include "pmm_param.h"
#include "pmm_load.h"

/*!
 * enumeration fo different model construction conditions that must be
 * satisified to conduct benchmarking
 */
typedef enum pmm_construction_condition {
    CC_INVALID,     /*< invalid status */
    CC_NOW,         /*< build model immidiately */
    CC_UNTIL,       /*< build model as much as up to a certain time */
    CC_PERIODIC,    /*< build model during a specific period of time only */
    CC_IDLE,        /*< build model only when machine is considered idle */
    CC_NOUSERS      /*< build model only when no users are logged in */
} PMM_Construction_Condition;



/*!
 * structure to hold the configuration of the benchmarking server
 */
typedef struct pmm_config {
    struct pmm_routine **routines;  /**< pointer to array of routines to be
                                         benchmarked */
    int allocated;                  /**< number of allocated elements in the
                                         routines array */
    int used;                       /**< number of used elements in the
                                         routines array */

	struct pmm_loadhistory *loadhistory;  /**< pointer to load history */

    int daemon;                     /**< toggle whether to go to background */
    int build_only;                 /**< toggle whether to build, then exit */

    struct timespec ts_main_sleep_period;   /**< sleep period of scheduling
                                              loop */
    int time_spend_threshold;               /**< threshold for time spend
                                                 benchmarking before writing
                                                 models to disk */
    int num_execs_threshold;                /**< threshold for number of
                                                 benchmark executions before
                                                 writing models to disk */

	char *logfile;                          /**< log file name */
	char *configfile;                       /**< configuartion filename */

    int pause;                              /**< toggle pause after a
                                                 benchmark */

} PMM_Config;

/*!
 * enumeration of different model construction methods
 */
typedef enum pmm_construction_method {
    CM_NAIVE,          /*!< construct by testing every problem size from start
                            to finish */
    CM_NAIVE_BISECT,   /*!< construct by testing every problem size but use
                            bisection of problem size range to get quicker
                            coverage */
    CM_RAND,           /*!< construct using random sampling */
    CM_GBBP,           /*!< construct using optimised Geometric Bisection
                             Building Procedure */
	CM_GBBP_NAIVE,     /*!< construct using GPPB but with a naive initial
                            period */
    CM_INVALID         /*!< invalid construction method */
} PMM_Construction_Method;


/*!
 * Benchmark structure, storing information routine tests.
 *
 * Details the on the parameters and execution results of a benchmark
 * execution. Also pointers to next/previous benchmarks should the one in
 * question is a member of a list.
 */
typedef struct pmm_benchmark {
	int n_p;                    //!< number of parameters
	int *p;                     //!< array of parameters 

    //TODO calculate complexity from problem size variables with muparser
	long long int complexity;   //!< complexity of benchmark (no. of float ops.)
	double flops;               //!< speed of benchmark execution
	double seconds;             //!< benchmark execution time in seconds

	struct timeval wall_t;      //!< wall clock execution time (timeval)

	struct timeval used_t;      //!< kernel and user mode execution time summed

	struct pmm_benchmark *previous; //!< pointer to previous bench in list
	struct pmm_benchmark *next;     //!< pointer to next bench in list
} PMM_Benchmark;


/*!
 * structure describing a list of benchmarks
 *
 * Benchmarks are doubly linked.
 */
typedef struct pmm_bench_list {
	int size;                       /*!< number of benchmarks stored in the list */
	int n_p;                        /*!< number of parameters the benchmarks have */

	struct pmm_benchmark *first;    /*!< first element of the list */
	struct pmm_benchmark *last;     /*!< last element of the list */

	struct pmm_model *parent_model; /*!< model to which the benchmarks belong */
} PMM_Bench_List;


/*!
 * functional performance model of a routine
 */
typedef struct pmm_model {
    char *model_path;               /*!< path to model filee */
    int unwritten_num_execs;        /*!< number of executions since last write */
    double unwritten_time_spend;    /*!< benchmarking time spend since last
                                         write */
    time_t mtime;                   /*!< modified time of the model file */

	int n_p;                        /*!< number of parameters of the model */

	int completion;                 /*!< completion state of the model */
	int complete;                   /*!< is model complete */
    int unique_benches;             /*!< number of unique benchmarks made to
                                         build model */

	struct pmm_bench_list *bench_list;  /*! pointer to benchmark list */

    double peak_flops;                  /*!< peak speed observed over all
                                             benchmarks */

    struct pmm_interval_list *interval_list; /*!< intervals describing
                                                  unfinished parts of the model */

    struct pmm_paramdef_set *pd_set;    /*!< set of parameter definition for
                                             the routine */

    struct pmm_routine *parent_routine; /*!< routine to which the model
                                             belongs */
} PMM_Model;

/*!
 * structure describing a routine to be benchmarked by pmm
 */
typedef struct pmm_routine {

	char *name;         /*!< name of routine */
	char *exe_path;     /*!< path to benchmark executable */
    char *exe_args;     /*!< extra arguments of benchmark */

    struct pmm_paramdef_set *pd_set; /*!< set of parameter defintions */

	//struct pmm_policy *policy; TODO implement policies
	enum pmm_construction_condition condition;  /*!< benchmarking condition */
	int priority;       /*!< benchmarking priority */
	int executable;     /*!< toggle for executability */

	enum pmm_construction_method construction_method; /*!< model construction method */
    int min_sample_num;     /*!< minimum samples for each model point */
    int min_sample_time;    /*!< minimum time to spend benchmarking each model
                                 point */
    int max_completion;     /*!< maximum number of model points */

	struct pmm_model *model;            /*!< pointer to model */

	struct pmm_routine *next_routine;   /*!< next routine in routine array/list */

    struct pmm_config *parent_config;   /*!< configuration of host */

} PMM_Routine;



char*
construction_method_to_string(enum pmm_construction_method method);
char*
construction_condition_to_string(enum pmm_construction_condition condition);
char*
interval_type_to_string(enum pmm_interval_type type);


/*
typedef struct pmm_benchmark {
	long long int size; //TODO support multiple problem size variables
	long long int complexity; //TODO calculate complexity from problem size variables
	double flops;
	double seconds;

	//wall time for benchmark execution
	struct timeval wall_t;

	//user+system time for benchmark execution
	struct timeval used_t;

	struct pmm_benchmark *previous;
	struct pmm_benchmark *next;
} PMM_Benchmark;
*/


/*
 * new_ functions typically allocate memory and set values to failsafes (-1)
 */
struct pmm_config* new_config();
struct pmm_routine* new_routine();
struct pmm_model* new_model();
struct pmm_benchmark* new_benchmark();

struct pmm_bench_list*
new_bench_list(struct pmm_model *m,
               int n_p);

/*
 * init_ functions initialize structures with initial condition data values
 */

int
init_bench_list(struct pmm_model *m, struct pmm_paramdef_set *pd_set);
struct pmm_benchmark*
init_zero_benchmark(int *params, int n_p);

int isempty_model(struct pmm_model *m);

int add_routine(struct pmm_config *c, struct pmm_routine *r);
int insert_bench(struct pmm_model *m, struct pmm_benchmark *b);
int
remove_benchmarks_at_param(struct pmm_model *m, int *p,
                           struct pmm_benchmark ***removed_array);

int copy_benchmark(struct pmm_benchmark *dst, struct pmm_benchmark *src);

void add_bench(struct pmm_benchmark *a, struct pmm_benchmark *b,
               struct pmm_benchmark *res);
void div_bench(struct pmm_benchmark *b, double d, struct pmm_benchmark *res);

int sum_bench_list(struct pmm_benchmark *start, struct pmm_benchmark *end,
               struct pmm_benchmark *sum);

void avg_bench_list(struct pmm_benchmark *start, struct pmm_benchmark *end,
                   struct pmm_benchmark *avg_b);

void copy_timeval(struct timeval *src, struct timeval *dst);
double timeval_to_double(struct timeval *tv);
void double_to_timeval(double d, struct timeval *tv);
void double_to_timespec(double d, struct timespec *ts);
void timeval_add(struct timeval *tv_a, struct timeval *tv_b,
                 struct timeval *tv_res);
void timeval_div(struct timeval *tv, double d);

int
count_benchmarks_in_model(struct pmm_model *m);
int
count_benchmarks_in_bench_list(struct pmm_bench_list *bl);

int
count_unique_benchmarks_in_sorted_list(struct pmm_benchmark *first);

int
insert_bench_into_sorted_list(struct pmm_benchmark **list_start,
                              struct pmm_benchmark **list_end,
                              struct pmm_benchmark *b);
int
insert_bench_into_sorted_list_before(struct pmm_benchmark **list_first,
                                     struct pmm_benchmark **list_last,
		                             struct pmm_benchmark *before,
                                     struct pmm_benchmark *b);
int
insert_bench_into_sorted_list_after(struct pmm_benchmark **list_first,
                                    struct pmm_benchmark **list_last,
		                            struct pmm_benchmark *after,
                                    struct pmm_benchmark *b);
int
insert_bench_into_list(struct pmm_bench_list *bl,
                       struct pmm_benchmark *b);

int
remove_bench_from_sorted_list(struct pmm_benchmark **list_first,
                              struct pmm_benchmark **list_last,
                              struct pmm_benchmark *b);
int
remove_bench_from_bench_list(struct pmm_bench_list *bl,
                             struct pmm_benchmark *b);

int benchmark_on_axis(struct pmm_model *m, struct pmm_benchmark *b);

int is_benchmark_at_origin(int n, struct pmm_paramdef *paramdef_array,
                           struct pmm_benchmark *b);


int isequal_benchmarks(struct pmm_benchmark *b1, struct pmm_benchmark *b2);

struct pmm_benchmark*
get_avg_aligned_bench(struct pmm_model *m, int *param);

struct pmm_benchmark*
get_avg_bench(struct pmm_model *m, int *param);

struct pmm_benchmark*
get_avg_bench_from_sorted_bench_list(struct pmm_benchmark *start,
                                     int *param);
struct pmm_benchmark*
find_max_bench_in_sorted_bench_list(struct pmm_benchmark *start,
                                     int *param);
int
search_sorted_bench_list(int direction,
                         struct pmm_benchmark *start,
                         int *param,
                         int n_p,
                         struct pmm_benchmark **first,
                         struct pmm_benchmark **last);

struct pmm_benchmark*
get_next_different_bench(struct pmm_benchmark *b);

struct pmm_benchmark*
get_previous_different_bench(struct pmm_benchmark *b);

struct pmm_benchmark*
get_first_bench(struct pmm_model *m, int *param);

struct pmm_benchmark*
get_first_bench_from_bench_list(struct pmm_bench_list *bl,
		                        int *p);

void
calc_bench_exec_stats(struct pmm_model *m, int *param,
                      double *time_spent, int *num_execs);
double
calc_model_stats(struct pmm_model *m);

struct pmm_benchmark *
find_oldapprox(struct pmm_model *m, int *p);
struct pmm_benchmark* lookup_model(struct pmm_model *m, int *p);
struct pmm_benchmark* interpolate_1d_model(struct pmm_bench_list *bl,
                                                 int *p);


int bench_cut_contains(struct pmm_loadhistory *h, struct pmm_benchmark *b1,
		               struct pmm_benchmark *b2);
int bench_cut_intersects(struct pmm_loadhistory *h, struct pmm_benchmark *b1,
		                 struct pmm_benchmark *b2);
int bench_cut_greater(struct pmm_loadhistory *h, struct pmm_benchmark *b1,
		              struct pmm_benchmark *b2);
int bench_cut_less(struct pmm_loadhistory *h, struct pmm_benchmark *b1,
		           struct pmm_benchmark *b2);

void print_routine(const char *output, struct pmm_routine *r);
void print_model(const char *output, struct pmm_model *m);
void print_bench_list(const char *output, struct pmm_bench_list *bl);
void print_benchmark(const char *output, struct pmm_benchmark *b);
int set_str(char **dst, char *src);

time_t parseISO8601Date(char *date);

int check_routine(struct pmm_routine *r);

void print_config(const char *output, struct pmm_config *cfg);
void free_model(struct pmm_model **m);
void free_bench_list(struct pmm_bench_list **bl);
void free_benchmark_list_backwards(struct pmm_benchmark **first_b);
void free_benchmark_list_forwards(struct pmm_benchmark **last_b);
void free_benchmark(struct pmm_benchmark **b);
void free_routine(struct pmm_routine **r);
void free_config(struct pmm_config **cfg);
void free_loadhistory(struct pmm_loadhistory **h);

#endif /*PMM_DATA_H_*/

