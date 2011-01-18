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
 * @file pmm_executor.c
 *
 * @brief Functions for executing a benchmark
 *
 * This file contains code for calling a benchmark executable from the 
 * pmm daemon, dealing with the system level requirements of spawning
 * the benchmark process and parsing the output of said benchmark.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

//#include <string.h>
//#include <stdio.h>
#include <stdlib.h>         // for free
#include <signal.h>         // for sigset
#include <sys/signal.h>     // for sigset
#include <pthread.h>        // for pthreads/etc.
// TODO platform specific code follows, need to fix this
#include <sys/time.h>       // for select
#include <sys/types.h>      // for select, waitpid
#include <time.h>           // for timeval
#include <paths.h>
#include <sys/wait.h>       // for waitpid
#include <fcntl.h>          // for fcntl
#include <unistd.h>         // for fcntl, select
#include <errno.h>          // for perror, errno, etc
#include <libgen.h>         // for basename

#include "pmm_model.h"
#include "pmm_selector.h"
#include "pmm_cfgparser.h"
#include "pmm_log.h"
#include "pmm_util.h"

//! daemon executing variable or not
extern int executing_benchmark;                     
//! mutex for accessing executing_benchmark variable
extern pthread_mutex_t executing_benchmark_mutex;   

/* TODO 
extern volatile sig_atomic_t sig_cleanup_received; 
extern volatile sig_atomic_t sig_pause_received;
extern volatile sig_atomic_t sig_unpause_received;
*/

//! signal to indicate benchmark should be terminated
extern int signal_quit;
//! mutex for accessing singal_quit
extern pthread_mutex_t signal_quit_mutex;


/* local functions */
int my_popen(char *cmd, char **args, int n, pid_t *pid);
int set_non_blocking(int fd);
struct pmm_benchmark* parse_bench_output(char *output, int n_p, int *rargs);
                                         

int read_benchmark_output(int fd, char **output_p, pid_t bench_pid);

void sig_childexit(int sig);
double calculate_flops(struct timeval tv, long long int complexity);
double timeval_to_seconds(struct timeval tv);
void set_executing_benchmark(int i);
char*
pmm_bench_exit_status_to_string(int bench_status);

/*!
 * a roughly portable way of setting a file descriptor for non-blocking I/O.
 *
 * @param   fd      file descriptor to modify
 *
 * @return -1 on error, 0 on success
 */
int set_non_blocking(int fd) {
	int flags;

	/* If they have O_NONBLOCK, use the Posix way to do it
	 * Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x
	 * and AIX 3.2.5.
	 */
#if defined(O_NONBLOCK)

	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;

	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);

#else

	/* Otherwise, use the old way of doing it */
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);

#endif

}

/*!
 * Forks a process that executes and external program with a an argument string
 * and returns a file distriptor and sets the child processes pid
 *
 * @param   cmd     string with the full path of the program
 * @param   args    array of strings with all arguments of the program
 * @param   n       number of arguments in arg vector
 * @param   pid     pointer to the pid which will be set to the pid of the
 *                  external program
 *
 * @return file descriptor of the pipe opened to the command cmd.
 *
 * TODO test for file not found
 *
 */
int
my_popen(char *cmd, char **args, int n, pid_t *pid)
{
	int p[2]; // this is our pipe of file descriptors
	char **argv;
    int i;

	if(pipe(p) < 0) {
		ERRPRINTF("Error creating pipe.\n");
		return -1;
	}


	/* build up argv for calling command */
    argv = malloc((n+2) * sizeof *argv);

    argv[0] = basename(cmd); //TODO threadsafe????

    DBGPRINTF("cmd:%s base:%s\n", cmd, argv[0]);

    for(i=1; i<n+1; i++) {
        DBGPRINTF("%s\n", args[i-1]);
        argv[i] = args[i-1];
    }

    argv[i] = (char *)NULL;



	sigset(SIGCHLD, &sig_childexit);

	if((*pid = fork()) > 0) { // parent

		close(p[1]); // close write pipe

        free(argv); //this has been copied into the child so we can free
        argv = NULL;

		return p[0]; // return read pipe
	}
	else if(*pid  == 0) { // child


		close(p[0]); // close read pipe

		// duplicate STDOUT to write pipe (for parent to read)
		dup2(p[1], STDOUT_FILENO);

		// duplicate STDERR to write pipe (for parent to read)
		dup2(p[1], STDERR_FILENO);

		close(p[1]);


		// call the command 'cmd' using argv[]
		execv(cmd, argv);

		// if we reach this exec has filed
		ERRPRINTF("child: exec failure, exiting.\n");
		exit(EXIT_FAILURE);

	}
	else { // error
		close(p[0]);
		close(p[1]);
		ERRPRINTF("Error forking.\n");
		return -1;
	}
}

/*!
 *  SIGCHLD signal handler
 *  
 *  @param   sign    the signal
 *
 *  @warning Do not use *PRINTF logging facility in this thread, it is not
 * 'async-signal-safe' (though it is threadsafe).
 */
void sig_childexit(int sig)
{

    (void)sig; //TODO unused

    printf("received SIGCHLD\n");
    // if we wait here we miss the status in the "benchmark" function
	//wait(0);
}

/*!
 * Given the output of a benchmark execution parse and create a new
 * benchmark structure storing all the information gained
 *
 * @param   output  pointer to benchmark output string
 * @param   n_p     number of parameters of benchmark
 * @param   rargs   arguments passed to the benchmarked routine
 *
 * @return pointer to newly allocated benchmark reflecting the parsed output
 */
struct pmm_benchmark*
parse_bench_output(char *output, int n_p, int *rargs)
{
    //TODO seperate into seperate call
    //TODO permit detection of routine that benchmark pertains to
    //TODO change rargs to p or something reflecting the reset of the code
	struct timeval wall_t, used_t;
	long long complexity;
	int rc;
	struct pmm_benchmark *b;

	rc = sscanf(output, "%ld %ld\n%ld %ld\n%lld", &(wall_t.tv_sec),
                &(wall_t.tv_usec), &(used_t.tv_sec), &(used_t.tv_usec),
                &complexity);
	           
	if(rc != 5) {
		ERRPRINTF("Error parsing output\n");
		return NULL;
	}

	LOGPRINTF("wall secs: %ld usecs: %ld\n", wall_t.tv_sec, wall_t.tv_usec);
	LOGPRINTF("used secs: %ld usecs: %ld\n", used_t.tv_sec, used_t.tv_usec);

    if(wall_t.tv_sec == 0 && wall_t.tv_usec == 0) {
        ERRPRINTF("Zero execution wall-time parsed from bench output.\n");
        return NULL;
    }

    if(complexity == 0) {
        ERRPRINTF("Zero complexity value parsed from bench output.\n");
        return NULL;
    }

	b = new_benchmark();

	if(b == NULL) {
		ERRPRINTF("Errory allocating benchmark structure.\n");
		return NULL;
	}

	b->n_p = n_p;

    b->p = init_param_array_copy(rargs, b->n_p);
	if(b->p == NULL) {
		ERRPRINTF("Error copying benchmark parameters.\n");
		return NULL;
	}

	copy_timeval(&b->wall_t, &wall_t);
	copy_timeval(&b->used_t, &used_t);

	b->next = (void *)NULL; //this will be delt with elswhere
	b->previous = (void *)NULL;
	b->complexity = complexity;
	b->flops = calculate_flops(b->wall_t, (b->complexity));
	b->seconds = timeval_to_seconds(b->wall_t);

    LOGPRINTF("flops: %f\n", b->flops);

	return b;
}


/*!
 * calculate (FL)OPS (floating point operations per second or any other
 * type of operations per second)
 *
 * @param   tv          timeval structure representing duration of computation
 * @param   complexity  complexity of computation (volume of operations)
 *
 * @returns flops as a double
 */
double calculate_flops(struct timeval tv, long long int complexity)
{
	return complexity/timeval_to_seconds(tv);
}

/*!
 * convert a timeval to second in double
 *
 * @param   tv  timeval structure
 *
 * @return timeval as seconds
 */
double timeval_to_seconds(struct timeval tv)
{
	return (double)tv.tv_sec + (double)tv.tv_usec/1000000.0;
}

/*!
 * Convert params to an array of strings and call my_popen
 *
 * @param   r           pointer to routine that will be executed
 * @param   params      pointer to array of parameters
 * @param   bench_pid   pointer to pid_t to store spawned process pid
 *
 * @return int index of file descriptor that points to the stdout of the
 * spawned process, or -1 if there is an error
 */
int
spawn_benchmark_process(struct pmm_routine *r, int *params,
                        pid_t *bench_pid)
{

    char **arg_strings;
    int arg_n;
    int i, j;
    int fd;

    LOGPRINTF("exe_path:%s\n", r->exe_path);
    print_params(PMM_LOG, params, r->pd_set->n_p);

    // if we have some static args specified we need to count them and add
    // them to the arg array
    if(r->exe_args != NULL) {
        char *tmp_args;
        int tmp_args_len;
        char *sep = " "; // tokenise on space, no quoted arguments supported
        char *last = NULL;
        char *tok;
        int count;

        // copy the static args as strtok* modifies its inputs
        tmp_args = strdup(r->exe_args);
        if(tmp_args == NULL) {
            ERRPRINTF("Error duplicating exe arg string.\n");
            return -1;
        }

        // check length of string for later
        tmp_args_len = (int)strlen(tmp_args) +1;

        // tokenise string, counting as we go
        count=0;
        tok = strtok_r(tmp_args, sep, &last);
        DBGPRINTF("token %d:%s\n", count, tok);
        while(tok != NULL) {
            count++;

            tok = strtok_r(NULL, sep, &last);
        }

        // allocate array for number of static args (from tokens) and parameters
        // of model
        arg_n = r->pd_set->n_p+count;
        arg_strings = malloc(arg_n * sizeof *arg_strings);
        if(arg_strings == NULL) {
            ERRPRINTF("Error allocating argument array.\n");
            return -1;
        }

        DBGPRINTF("arg_n:%d\n", arg_n);

        // strtok_r will have placed '\0' (null) characters in the tmp_args
        // character array, uses these to find the tokens and copy each token
        // to an element of the arg array
        int new_find = 0;
        j = 0;
        for(i=0; i<tmp_args_len; i++) {
            if(new_find == 0) {
               if(tmp_args[i] != '\0') {
                   arg_strings[j++] = strdup(&tmp_args[i]);
               }
               new_find = 1;
            }
            if(new_find == 1) {
                if(tmp_args[i] == '\0') {
                    new_find = 0;
                }
            }

            DBGPRINTF("%d:%c\n", i, tmp_args[i]);
        }

        //check we got expected number of tokens
        if(j!=count) {
            free(arg_strings);
            ERRPRINTF("Error copying tokens into array.\n");
            return -1;
        }

        free(tmp_args);
        tmp_args = NULL;

        //copy the routine parameters in the array after the static arguments
        for(i=0; i<arg_n; i++, j++) {
            arg_strings[j] = malloc((1+snprintf(NULL, 0, "%d", params[i])) *
                                    sizeof *arg_strings[j]);

            sprintf(arg_strings[j], "%d", params[i]);

            DBGPRINTF("arg_strings[%d]:%s\n", j, arg_strings[j]);
        }

    }
    else {
        arg_n = r->pd_set->n_p;

        //build argument array strings to pass to the command
        arg_strings = malloc(arg_n * sizeof *arg_strings);
        for(i=0; i<arg_n; i++) {

            arg_strings[i] = malloc((1+snprintf(NULL, 0, "%d", params[i]))
                    * sizeof *arg_strings[i]);

            sprintf(arg_strings[i], "%d", params[i]);

        }
    }

    fd = my_popen(r->exe_path, arg_strings, arg_n, bench_pid);

    for(i=0; i>arg_n; i++) {
        free(arg_strings[i]);
        arg_strings[i] = NULL;
    }

    free(arg_strings);
    arg_strings = NULL;

    return fd;
}

/*!
 * clean up a benchmark process
 *
 * closes file handles and kills the process
 *
 * @param   fp              file pointer to the benchmarks standard output
 * @param   bench_pid       benchmark process id
 *
 */
void
cleanup_benchmark_process(FILE *fp, pid_t bench_pid)
{
    //send kill don't bother calling waitpid on benchmark
    if(kill(bench_pid, SIGKILL) != 0) {
        ERRPRINTF("error sending kill to benchmark.\n");
    }

    if(fclose(fp) < 0) {
        ERRPRINTF("Error closing file pointer to command\n");
    }
}

/*!
 *
 * Read the output of a benchmark process
 *
 * @param   fd          benchmark process output file descriptor
 * @param   output_p    pointer to a character array where output will be
 *                      stored
 * @param   bench_pid   process id of the benchmark process
 *
 * @return 0 on success, -1 if there was an error reading (output_p
 * also set to NULL) 1 if a quit signal was received while waiting for output
 */
int
read_benchmark_output(int fd, char **output_p, pid_t bench_pid)
{
    FILE *fp;
    int reading;

    char *output;
    char *tmp_output;
    int output_index;
    int output_size;
    int bs = 512;
    int r_count;

    // variables for select()
    struct timeval tv_select_wait;
    fd_set read_set;
    int select_ret;

    set_non_blocking(fd);

    fp = (void *)NULL;

    // get file pointer from file descriptor
    fp = fdopen(fd, "r");
    if(fp == NULL) {
        ERRPRINTF("Error opening file descriptor\n");

        *output_p = NULL;

        return -1;
    }

    // allocate some memory for the output string
    output = calloc(bs, sizeof *output);
    output_index = 0;
    output_size=bs;

    reading = 1;
    while(reading) {

        // select overwrites its parameters so we need to initalise them
        // inside the loop
        FD_ZERO(&read_set);
        FD_SET(fd, &read_set);
        tv_select_wait.tv_sec = 5;
        tv_select_wait.tv_usec = 0;

        //DBGPRINTF("select() waiting on file descriptor for %ld.%ld secs.\n",
        //        tv_select_wait.tv_sec, tv_select_wait.tv_usec);

        // select on file descriptor
        select_ret = select(fd+1, &read_set, NULL, NULL, &tv_select_wait);
        if(select_ret < 0) { // error
            if(errno == EINTR)
                continue;

            ERRPRINTF("Error waiting for benchmark output.\n");

            cleanup_benchmark_process(fp, bench_pid);

            free(output);
            output = NULL;

            *output_p = NULL;

            return -1;

        }
        else if(select_ret == 0) { // no data is available to read

            //DBGPRINTF("no benchmark output available: %d\n", select_ret);

        }
        else if(select_ret > 0) { // data available to read

            //DBGPRINTF("select() file descriptor ready %d\n", select_ret);

            //TODO check we are growing buffer correctly

            while( !feof(fp) ) {

                // try a read, if it is empty break back to the select loop
                r_count = fread(&output[output_index], sizeof *output,
                                bs-1, fp);

                if(r_count == 0) { // finished reading back to select
                    //DBGPRINTF("read 0 chars, breaking to eof check.\n");
                    break;
                }
                else { // if it is not empty process the read
                    //DBGPRINTF("read %d, allocating:%d to output read buffer.\n",
                    //          r_count, output_size+r_count);

                    tmp_output = realloc(output,
                                        output_size + r_count * sizeof *output);

                    if(tmp_output == NULL) {

                        ERRPRINTF("Error reallocating mem. to read buffer.\n");

                        cleanup_benchmark_process(fp, bench_pid);

                        free(output);
                        output = NULL;

                        *output_p = NULL;

                        return -1;

                    }
                    else {
                        output = tmp_output;
                    }

                    //DBGPRINTF("output:%p\n", output);
                    //DBGPRINTF("output_size:%d sizeof(output):%lu\n",
                    //          output_size, sizeof(output[0]));
                    //DBGPRINTF("output_index (old):%d\n", output_index);
                    output_index += r_count;
                    output_size += r_count;
                    //DBGPRINTF("output_index (new):%d\n", output_index);
                }
            }

            if(feof(fp)) {
                //DBGPRINTF("reached end of benchmark output\n");
                output[output_index] = '\0';
                reading = 0;
            } else {
                //DBGPRINTF("going into another select loop.\n");
            }
        }

        /*
           if(sig_pause_received) {
        // send pause signal to benchmark process
        if(kill(bench_pid, SIGSTOP) != 0) {
        perror("[benchmark]"); //TODO
        ERRPRINTF("error sending SIGSTOP to benchmark process:%d", bench_pid);
        }
        }

        if(sig_unpause_received) {
        // send unpause signal to benchmark process
        if(kill(bench_pid, SIGCONT) != 0) {
        perror("[benchmark]"); //TODO
        ERRPRINTF("error sending SIGCONT to benchmark process:%d", bench_pid);
        }
        }
        */

        //check that global shutdown variable has not been called before
        //returning to select at start of while
        pthread_mutex_lock(&signal_quit_mutex);
        if(signal_quit) {
            pthread_mutex_unlock(&signal_quit_mutex);

            LOGPRINTF("signal_quit set, closing files and freeing"
                    " memory\n");
            LOGPRINTF("killing benchmark process pid:%d\n", (int)bench_pid);

            cleanup_benchmark_process(fp, bench_pid);

            free(output);
            output = NULL;

            *output_p = NULL;

            return 1;
        }
        pthread_mutex_unlock(&signal_quit_mutex);

    //go back to select
    }

    //finished reading now, close file

    if(fclose(fp) < 0) {
        ERRPRINTF("Error closing file pointer to command\n");
    }

    *output_p = output;
    return 0;
}

/*!
 * Given a routine to benchmark, select a point, execute the benchmark and
 * insert the new point into the model. Set the global 'executing_benchark' to
 * false/0 when the operation is complete (or has failed).
 *
 * @param   scheduled_r     void pointer to a routine structure that has been
 *                          scheduled for a benchmark execution
 *
 * @return void pointer to an integer with value of: 0 on success, 1 on quit from signal, -1 on failure
 */
void*
benchmark(void *scheduled_r)
{
    // TODO decompose some of the fuctionality of this to make it more legible
	int *ret;
    int temp_ret;
	struct pmm_routine *r;
	struct pmm_benchmark *bmark;
	int *rargs = NULL; //new benchmark point

    pid_t bench_pid;
    int bench_status;

    int fd;
    char *output;

	r = (struct pmm_routine*)scheduled_r;

    ret = (int*)malloc(sizeof *ret);

	//evaluate current performance model approximation and pick new
	//point on the approximation to measure with benchmark, TODO if model
	//proves to be complete set complete status and return immidiately
	if(r->construction_method == CM_NAIVE) {
		rargs = multi_naive_select_new_bench(r);
	}
    else if(r->construction_method == CM_NAIVE_BISECT) {
        rargs = naive_1d_bisect_select_new_bench(r);
    }
	else if(r->construction_method == CM_GBBP) {
	//	rargs = multi_gbbp_select_new_bench(r);
		rargs = multi_gbbp_diagonal_select_new_bench(r);
	}
    else if(r->construction_method == CM_GBBP_NAIVE) {
		rargs = multi_gbbp_naive_select_new_bench(r);
    }
	else if(r->construction_method == CM_RAND) {
		rargs = multi_random_select_new_bench(r);
	}
	else { // default
		//rargs = naive_select_new_bench(r);
	}

	if(rargs == NULL) {
		ERRPRINTF("Error selecting new benchmark point.\n");
        set_executing_benchmark(0);
        *ret = -1;

		return (void *)ret;
	}

	// take routine and execute it passing in the parameters
	// of the performance model coordinate experiment at
    //
	//LOGPRINTF("benchmark thread: started\n");


    //LOGPRINTF("exe_path:%s\n", r->exe_path);
    //print_params(rargs, r->n_p);


    fd = spawn_benchmark_process(r, rargs, &bench_pid);
    if(fd == -1) {
        ERRPRINTF("Error spawning benchmark process, fd:%d pid:%d\n", (int)fd,
                   bench_pid);

        free(rargs);
        rargs = NULL;

        //TODO send kill to bench_pid, just to be sure?

        set_executing_benchmark(0);
        *ret = -1; //failure

        return (void *)ret;
    }


    //LOGPRINTF("filedescriptor returned from popen: %d pid:%d\n", (int)fd,
    //        bench_pid);


    temp_ret = read_benchmark_output(fd, &output, bench_pid);
    if(temp_ret == -1) {
        ERRPRINTF("Error reading benchmark output.\n");

        free(rargs);
        rargs = NULL;

        set_executing_benchmark(0);
        *ret = -1; //failure

        return (void *)ret;
    }
    else if(temp_ret == 1) {
        DBGPRINTF("Recieved quit while reading benchmark output.\n");

        free(rargs);
        rargs = NULL;

        set_executing_benchmark(0);
        *ret = 1; //sigquit received

        return (void *)ret;
    }


    //wait for bench to terminate (and retrieve exit status)
    if(waitpid(bench_pid, &bench_status, 0) != bench_pid) {
        ERRPRINTF("Error waiting for benchmark to terminate.\n");
    }


    //check exit status
    if(WEXITSTATUS(bench_status) != PMM_EXIT_SUCCESS) {
        ERRPRINTF("benchmark exited with abnormal status:%s.\n",
                  pmm_bench_exit_status_to_string(WEXITSTATUS(bench_status)));

        ERRPRINTF("output was:\n--------\n%s---------\n", output);

        free(output);
        output = NULL;

        free(rargs);
        rargs = NULL;

        set_executing_benchmark(0);
        *ret = -1; //failure

        return (void *)ret;

    }

    DBGPRINTF("---output---\n%s------------\n", output);


    //parse benchmark output
    if((bmark = parse_bench_output(output, r->pd_set->n_p, rargs)) == NULL) {
        ERRPRINTF("Error parsing benchmark output\n");

        free(output);
        output = NULL;

        free(rargs);
        rargs = NULL;

        set_executing_benchmark(0);
        *ret = -1; //failure

        return (void *)ret;
    }


    free(output);
    output = NULL;

    //DBGPRINTF("bmark:%p\n", bmark);

    //TODO might need a mutex here
    temp_ret = 0;
    if(r->construction_method == CM_NAIVE) {

        DBGPRINTF("Inserting benchmark with naive construction method.\n");

        temp_ret = multi_naive_insert_bench(r, bmark);

    }

    if(r->construction_method == CM_NAIVE_BISECT) {
        temp_ret = naive_1d_bisect_insert_bench(r, bmark);
    }
    else if(r->construction_method == CM_GBBP ||
            r->construction_method == CM_GBBP_NAIVE) {
        // note, we can use the same insertion methods for GBBP Diagonal and
        // GBBP Naive because all steps in the insertion phase are common to
        // both methods.

        DBGPRINTF("Inserting benchmark with GBBP.\n");

        temp_ret = multi_gbbp_insert_bench(NULL, r, bmark);

    }
    else { // default, including CM_RAND
        insert_bench(r->model, bmark);
    }

    // check if benchmark insertion failed
    if(temp_ret < 0) {
        //interval processing failed, bench added though so we will write
        //the model to save the result of the execution
        if(temp_ret == -1) {
            ERRPRINTF("Interval error when inserting new benchmark.\n");
            write_model(r->model);
        }
        else {
            ERRPRINTF("Error inserting new benchmark.\n");
        }

        free(rargs);
        rargs = NULL;

        set_executing_benchmark(0);

        *ret = -1; //failure

        return (void *)ret;
    }


    // test if model has a max_completion
    if(r->max_completion != -1) {
        // set model complete if completion has exceeded or equaled max
        if(r->model->unique_benches >= r->max_completion) {
            r->model->complete = 1;
        }
    }

    //update number of unwritten benchmarks and the time spent benchmarking
    //since last write
    r->model->unwritten_time_spend += timeval_to_double(&(bmark->wall_t));
    r->model->unwritten_num_execs += 1;

    //TODO write model here? why not!
    if(r->model->complete == 1 ||
       r->model->unwritten_num_execs >= r->parent_config->num_execs_threshold ||
       r->model->unwritten_time_spend >= r->parent_config->time_spend_threshold)
    {
        DBGPRINTF("Writing model ...\n");
        temp_ret = write_model(r->model);
        if(temp_ret < 0) {
            ERRPRINTF("Error writing model to disk.\n");

            free(rargs);
            rargs = NULL;

            set_executing_benchmark(0);
            *ret = -1; //failure

            return (void *)ret;
        }
    }
    else {
        DBGPRINTF("Not writing model "
                  "(unwritten_num_execs/thres:%d/%d "
                  "unwritten_time_spend/thres:%f/%d) ...\n",
                  r->model->unwritten_num_execs,
                  r->parent_config->num_execs_threshold,
                  r->model->unwritten_time_spend,
                  r->parent_config->time_spend_threshold);
    }

    free(rargs);
    rargs = NULL;

    LOGPRINTF("benchmark thread: finished.\n");

    set_executing_benchmark(0);
	*ret = 1;

	return (void *)ret;
}

/*!
 * set the executing_benchmark variable via mutex
 *
 * @param   i   value to set variable with
 */
void
set_executing_benchmark(int i)
{
	//TODO check return codes
	LOGPRINTF("locking executing_benchmark.\n");
	pthread_mutex_lock (&executing_benchmark_mutex);
	executing_benchmark = i;
	LOGPRINTF("unlocking executing_benchmark.\n");
	pthread_mutex_unlock (&executing_benchmark_mutex);
}

/*!
 * convert benchmark exit status to a string
 *
 * @param   bench_status    benchmark status represented by an int
 *
 * @return pointer to a character string
 */
char*
pmm_bench_exit_status_to_string(int bench_status)
{
    switch(bench_status) {
        case PMM_EXIT_SUCCESS :
            return "success";

        case PMM_EXIT_ARGFAIL :
            return "argument number fail";

        case PMM_EXIT_ARGPARSEFAIL :
            return "argument parse failure";

        case PMM_EXIT_ALLOCFAIL :
            return "memory allocation failutre";

        case PMM_EXIT_EXEFAIL :
            return "routine execution failure";

        case PMM_EXIT_GENFAIL :
            return "general failure";

        default:
            return "unknown exit status";
    }
}
