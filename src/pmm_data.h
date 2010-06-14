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
#ifndef PMM_DATA_H_
#define PMM_DATA_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#include <pthread.h>

#define PMM_INVALID -1
#define PMM_NOW 0
#define PMM_BEFORE 1
#define PMM_PERIODIC 2
#define PMM_IDLE 3
#define PMM_NOUSERS 4


typedef struct pmm_config {
	struct pmm_routine **routines;
	int allocated;
	int used;

	struct pmm_loadhistory *loadhistory;

	int daemon;
    int build_only;

    struct timespec ts_main_sleep_period;
    int time_spend_threshold;
    int num_execs_threshold;

	char *logfile;
	char *configfile;

} PMM_Config;

typedef enum pmm_construction_method {
	CM_NAIVE,
    CM_RAND,
	CM_GBBP,
    CM_INVALID
} PMM_Construction_Method;

typedef struct pmm_paramdef_list {
	int n_param;
	struct pmm_paramdef *first;
	struct pmm_paramdef *last;
} PMM_Paramdef_List;

//
//TODO allow definition of a mathematical sequence to govern permissable
//parameters. At present stride and offset allow a sequence equvilent to:
//  a*x + b
//where a = stride and b = offset
//
typedef struct pmm_paramdef {
	char *name;
	int type;
	int order;
    int fuzzy_max;
	int max;
	int min;
    int stride;
    int offset;
} PMM_Paramdef;


/*! \struct pmm_benchmark
 *
 * Benchmark structure, storing information on the parameters and execution
 * results of a benchmark execution. Also pointers to next/previous benchmarks
 * should the one in question is a member of a list.
 *
 * @param   n_p         number of parameters
 * @param   p           pointer to parameter array
 * @param   complexity  
 */
typedef struct pmm_benchmark {
	int n_p;                    //!< number of parameters
	int *p;           //!< array of parameters 

    //TODO calculate complexity from problem size variables
	long long int complexity;   //!< complexity of benchmark (no. of float ops.)
	double flops;               //!< speed of benchmark execution
	double seconds;             //!< benchmark execution time in seconds

	//wall time for benchmark execution
	struct timeval wall_t;      //!< wall clock execution time (timeval)

	//user+system time for benchmark execution
	struct timeval used_t;      //!< kernel and user mode execution time summed

	struct pmm_benchmark *previous; //!< pointer to previous bench in list
	struct pmm_benchmark *next; //!< pointer to next bench in list
} PMM_Benchmark;


typedef struct pmm_bench_list {
	int size;
	int n_p; // number of parameters

	struct pmm_benchmark *first;
	struct pmm_benchmark *last;

	struct pmm_model *parent_model;
} PMM_Bench_List;


typedef struct pmm_model {
	char *model_path;
    int unwritten_num_execs;
    double unwritten_time_spend;

	int n_p; /* number of parameters of the model */

	int completion;
	int complete;

	struct pmm_bench_list *bench_list; 

	struct pmm_interval_list *interval_list;

	struct pmm_routine *parent_routine;
} PMM_Model;

typedef enum pmm_interval_type {
    IT_NULL,
	IT_BOUNDARY_COMPLETE,
	IT_GBBP_EMPTY,
	IT_GBBP_CLIMB,
	IT_GBBP_BISECT,
	IT_GBBP_INFLECT,
	IT_POINT,
    IT_COMPLETE
} PMM_Interval_Type;

/*
static char *pmm_interval_type_str[] = { "NULL",
                             "BOUNDARY_COMPLETE",
                             "GBBP_EMPTY",
                             "GBBP_CLIMB",
                             "GBBP_BISECT",
                             "GBBP_INFLECT",
                             "POINT",
                             "COMPLETE" };
*/


typedef struct pmm_interval {
	enum pmm_interval_type type;

	int plane; // index of the plane that this interval pertains to
    int n_p;
	int *start;
	int *end;

    int climb_step; //index of the step along the interval that we are currently
                    //constructing for, when the interval type is IT_GBBP_CLIMB

	struct pmm_interval *next; // pointer to the next interval in the stack
	struct pmm_interval *previous;
} PMM_Interval;

typedef struct pmm_interval_list {
	struct pmm_interval *top;
	struct pmm_interval *bottom;
	int size;

} PMM_Interval_List;

typedef struct pmm_routine {

	char *name;
	char *exe_path;

	int n_p; /* number of parameters of the model */
	struct pmm_paramdef *paramdef_array; /* array of model parameters */
    int param_product_constraint; /* maximum product of all parameters */


	//struct pmm_policy *policy; TODO implement policies
	int condition;
	int priority;
	int executable;

	enum pmm_construction_method construction_method;
    int min_sample_num;
    int min_sample_time;

	struct pmm_model *model;

	struct pmm_routine *next_routine;

    struct pmm_config *parent_config;

} PMM_Routine;


struct pmm_interval* new_interval();
struct pmm_interval_list* new_interval_list();

int
isempty_interval_list(struct pmm_interval_list *l);

void
print_interval_list(struct pmm_interval_list *l);

char*
interval_type_to_string(enum pmm_interval_type type);

struct pmm_interval*
read_top_interval(struct pmm_interval_list *l);

int
add_top_interval(struct pmm_interval_list *l, struct pmm_interval *i);

int
add_bottom_interval(struct pmm_interval_list *l, struct pmm_interval *i);

int
remove_top_interval(struct pmm_interval_list *l);

int
remove_interval(struct pmm_interval_list *l, struct pmm_interval *i);

void print_interval(struct pmm_interval *i);

/*
int interval_contains_bench(struct pmm_routine *r, struct pmm_interval *i,
		                    struct pmm_benchmark *b);
*/

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
 * this is a circular array of load history, size determined at run time
 */
typedef struct pmm_loadhistory {
	int write_period;

	struct pmm_load *history;
	int size;
	int size_mod;

	struct pmm_load *start, *end;
	int start_i, end_i;

	char *load_path;

	pthread_rwlock_t history_rwlock;
} PMM_Loadhistory;

typedef struct pmm_load {
	time_t time; //TODO use proper datatype
	double load[3]; /* 1, 5 & 15 minute load averages TODO use float? */
} PMM_Load;

/*
 * new_ functions typically allocate memory and set values to failsafes (-1)
 */
struct pmm_config* new_config();
struct pmm_routine* new_routine();
struct pmm_model* new_model();
struct pmm_benchmark* new_benchmark();
struct pmm_load* new_load();
struct pmm_loadhistory* new_loadhistory();

struct pmm_bench_list*
new_bench_list(struct pmm_model *m,
               int n_p);

/*
 * init_ functions initialize structures with initial condition data values
 */
int init_loadhistory(struct pmm_loadhistory *h, int size);

int
init_bench_list(struct pmm_model *m, struct pmm_paramdef *pd_array, int n_p);

struct pmm_interval* init_interval(int plane,
                                  int n_p,
                                  enum pmm_interval_type type,
                                  int *start,
                                  int *end);
int*
init_param_array_min(struct pmm_paramdef *pd_array, int n);
int*
init_param_array_max(struct pmm_paramdef *pd_array, int n);
void
set_param_array_min(int *p, struct pmm_paramdef *pd_array, int n);
void
set_param_array_max(int *p, struct pmm_paramdef *pd_array, int n);

int isempty_model(struct pmm_model *m);

void add_load(struct pmm_loadhistory *h, struct pmm_load *l);
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

int
param_product(int *p, int n);

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
copy_paramdef(struct pmm_paramdef *dst, struct pmm_paramdef *src);

int*
init_param_array_copy(int *src, int n);
void
set_param_array_copy(int *dst, int *src, int n);

int params_cmp(int *p1, int *p2, int n);

void
align_params(int *params, struct pmm_paramdef *pd_array, int n_p);
int
align_param(int param, struct pmm_paramdef *pd);
int*
init_aligned_params(int *p, struct pmm_paramdef *pd_array, int n_p);


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

int
param_on_axis(int *p,
              int n,
              struct pmm_paramdef *pd_array);

int benchmark_on_axis(struct pmm_model *m, struct pmm_benchmark *b);

int is_benchmark_at_origin(int n, struct pmm_paramdef *paramdef_array,
                           struct pmm_benchmark *b);


int isequal_benchmarks(struct pmm_benchmark *b1, struct pmm_benchmark *b2);
int
isequal_paramdef(struct pmm_paramdef *a, struct pmm_paramdef *b);

struct pmm_benchmark*
get_avg_aligned_bench(struct pmm_model *m, int *param);

struct pmm_benchmark*
get_avg_bench(struct pmm_model *m, int *param);

struct pmm_benchmark*
get_avg_bench_from_sorted_bench_list(struct pmm_benchmark *start,
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

void print_routine(struct pmm_routine *r);
void print_model(struct pmm_model *m);
void print_bench_list(struct pmm_bench_list *bl);
void print_benchmark(struct pmm_benchmark *b);
int set_str(char **dst, char *src);

time_t parseISO8601Date(char *date);

int check_routine(struct pmm_routine *r);
int check_loadhistory(struct pmm_loadhistory *h);

void print_params(int *p, int n);
void print_paramdef_array(struct pmm_paramdef *pd_array, int n);
void print_paramdef(struct pmm_paramdef *pd);
void print_config(struct pmm_config *cfg);
void print_loadhistory(struct pmm_loadhistory *h);
void print_load(struct pmm_load *l);

void free_model(struct pmm_model **m);
void free_interval_list(struct pmm_interval_list **il);
void free_interval(struct pmm_interval **i);
void free_bench_list(struct pmm_bench_list **bl);
void free_benchmark_list_backwards(struct pmm_benchmark **first_b);
void free_benchmark_list_forwards(struct pmm_benchmark **last_b);
void free_benchmark(struct pmm_benchmark **b);
void free_paramdefs(struct pmm_paramdef **pd_array, int n_p);
void free_routine(struct pmm_routine **r);
void free_config(struct pmm_config **cfg);
void free_loadhistory(struct pmm_loadhistory **h);

#endif /*PMM_DATA_H_*/

