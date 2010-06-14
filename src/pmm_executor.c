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

//#include <string.h> //for basename
//#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/signal.h>
#include <pthread.h>
// TODO platform specific code follows, need to fix this
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include <libgen.h> // for basename

#include "pmm_data.h"
#include "pmm_selector.h"
#include "pmm_cfgparser.h"
#include "pmm_log.h"
#include "pmm_util.h"

/*******************************************************************************
 * global variables
 */
extern int executing_benchmark;
extern pthread_mutex_t executing_benchmark_mutex;
extern volatile sig_atomic_t sig_cleanup_received;
extern volatile sig_atomic_t sig_pause_received;
extern volatile sig_atomic_t sig_unpause_received;

extern int signal_quit;
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

/*
 * this is a roughly portable way of setting a file descriptor for non-blocking
 * I/O.
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

/*
 *
 *  Do not use *PRINTF logging facility in this thread, it is not
 * 'async-signal-safe' (though it is threadsafe).
 */
void sig_childexit(int sig)
{
    printf("received SIGCHLD\n");
    // if we wait here we miss the status in the "benchmark" function
	//wait(0);
}

//TODO seperate into seperate call
//TODO permit detection of routine that benchmark pertains to
struct pmm_benchmark* parse_bench_output(char *output, int n_p, int *rargs)
{
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

	return b;
}

double calculate_flops(struct timeval tv, long long int complexity)
{

	return complexity/timeval_to_seconds(tv);


}

double timeval_to_seconds(struct timeval tv)
{
	return (double)tv.tv_sec + (double)tv.tv_usec/1000000.0;
}

/*!
 * Convert params to an array of strings and call my_popen
 *
 * @param   path        path to executable as a string
 * @param   n           number of parameters
 * @param   params      pointer to array of parameters
 * @param   bench_pid   pointer to pid_t to store spawned process pid
 *
 * @return int index of file descriptor that points to the stdout of the
 * spawned process, or -1 if there is an error
 */
int
spawn_benchmark_process(char *path, int n, int *params,
                        pid_t *bench_pid)
{

    char **arg_strings;
    int i;
    int fd;

    LOGPRINTF("exe_path:%s\n", path);
    print_params(params, n);


    //build argument array strings to pass to the command
    arg_strings = malloc(n * sizeof *arg_strings);
    for(i=0; i<n; i++) {

        arg_strings[i] = malloc((1+snprintf(NULL, 0, "%d", params[i]))
                * sizeof *arg_strings[i]);

        sprintf(arg_strings[i], "%d", params[i]);

    }

    fd = my_popen(path, arg_strings, n, bench_pid);

    for(i=0; i<n; i++) {
        free(arg_strings[i]);
        arg_strings[i] = NULL;
    }

    free(arg_strings);
    arg_strings = NULL;

    return fd;
}

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
 * Given a routine to benchmark, select a point, execute the benchmark
 * and insert the new point into the model. Set the global 'executing_benchark'
 * to false/0 when the operation is complete (failed).
 *
 * @return 0 on success, 1 on quit from signal, -1 on failure
 * TODO decompose some of the fuctionality of this to make it more legible
 */
void *benchmark(void *scheduled_r) {
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
	else if(r->construction_method == CM_GBBP) {
	//	rargs = multi_gbbp_select_new_bench(r);
		rargs = multi_gbbp_diagonal_select_new_bench(r);
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


    fd = spawn_benchmark_process(r->exe_path,r->pd_set->n_p, rargs, &bench_pid);
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

    //DBGPRINTF("bmark:%p\n", bmark);

    //TODO might need a mutex here
    if(r->construction_method == CM_NAIVE) {

        //DBGPRINTF("Inserting benchmark with naive construction method.\n");
        temp_ret = multi_naive_insert_bench(r, bmark);

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

            set_executing_benchmark(0);

            *ret = -1; //failure

            return (void *)ret;
        }

    }
    else if(r->construction_method == CM_GBBP) {

        //DBGPRINTF("Inserting benchmark with GBBP.\n");

        temp_ret = multi_gbbp_insert_bench(NULL, r, bmark);
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

            set_executing_benchmark(0);

            *ret = -1; //failure

            return (void *)ret;
        }
    }
    else if(r->construction_method == CM_RAND) {
        insert_bench(r->model, bmark);
    }
    else { // default
        insert_bench(r->model, bmark);
    }
    //print_model(r->model);
    //


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
        DBGPRINTF("Not writing model ...\n");
    }

    free(rargs);
    rargs = NULL;

    LOGPRINTF("benchmark thread: finished.\n");

    set_executing_benchmark(0);
	*ret = 1;

	return (void *)ret;
}

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
