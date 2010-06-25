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

#include <stdlib.h>
#include <sys/signal.h>
#include <pthread.h>
#include <signal.h>
// TODO platform specific code follows, need to fix this
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <fcntl.h>

#include "pmm_data.h"
#include "pmm_log.h"
#include "pmm_argparser.h"
#include "pmm_cfgparser.h"
#include "pmm_scheduler.h"
#include "pmm_executor.h"

//global variables
int executing_benchmark;
pthread_mutex_t executing_benchmark_mutex;

int signal_quit = 0;
pthread_mutex_t signal_quit_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile sig_atomic_t sig_cleanup_received = 0;
volatile sig_atomic_t sig_pause_received = 0;
volatile sig_atomic_t sig_unpause_received = 0;

void run_as_daemon();
void sig_cleanup(int sig);
void sig_do_nothing();
void *loadmonitor(void *loadhistory);


/*******************************************************************************
 * Daemonize process by forking to the background
 *
 */
void run_as_daemon() {
	signal(SIGTERM, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	switch(fork()) {
		case -1:
			ERRPRINTF("Error: could not fork to background.\n");
			exit(EXIT_FAILURE);
		case 0:
			break;
		default:
			exit(EXIT_SUCCESS);
	}

	if(setsid() == (pid_t) -1) {
		ERRPRINTF("Error: process could not create new session.\n");
		exit(EXIT_FAILURE);
	}

	(void) umask(0);



    //TODO
	//close(fileno(stdin));
	//close(fileno(stdout));
	//close(fileno(stderr));

	return;
}

/*
 * single handler thread that should catch signals and set appropriate
 * gobal variables
 *
 * Do not use *PRINTF logging facility in this thread, it is not
 * 'async-signal-safe' (though it is threadsafe).
 */
void* signal_handler(void* arg)
{
	sigset_t signal_set;
	int signal;

	for(;;) {

		//create a full signal set
		if(sigfillset(&signal_set) != 0) {
            ERRPRINTF("Error filling signal_set.\n");
            exit(EXIT_FAILURE);
        }

        //wait for all signals in signal_set
		sigwait(&signal_set, &signal);


        LOGPRINTF("signal received: %d\n", signal);

		// when we get this far, we've caught a signal
		switch(signal)
		{
			// whatever you need to do on SIGQUIT
			case SIGQUIT:
				if(pthread_mutex_lock(&signal_quit_mutex) != 0) {
                    ERRPRINTF("Error locking signal_quit_mutex\n");
                    exit(EXIT_FAILURE);
                }

				signal_quit = 1;

				if(pthread_mutex_unlock(&signal_quit_mutex) != 0) {
                    ERRPRINTF("Error unlocking signal_quit_mutex\n");
                    exit(EXIT_FAILURE);
                }

                return NULL; //we are done processing signals

				break;
            case SIGINT: //do the same with SIGINT
				if(pthread_mutex_lock(&signal_quit_mutex) != 0) {
                    ERRPRINTF("Error locking signal_quit_mutex\n");
                    exit(EXIT_FAILURE);
                }

				signal_quit = 1;

				if(pthread_mutex_unlock(&signal_quit_mutex) != 0) {
                    ERRPRINTF("Error unlocking signal_quit_mutex\n");
                    exit(EXIT_FAILURE);
                }

                return NULL; //we are done processing signals

                break;

			// whatever you need to do on SIGINT
			//case SIGINT:
			//	pthread_mutex_lock(&signal_mutex);
			//	handled_signal = SIGINT;
			//	pthread_mutex_unlock(&signal_mutex);
			//	break;

			// whatever you need to do for other signals
			default:
				break; //nohing
		}
	}


	return NULL;
}

void sig_cleanup(int sig) {
	LOGPRINTF("received: %d, cleaning up.\n", sig); //TODO enable this!!!
	sig_cleanup_received = 1;

	// after we receive the sigint we are going to catch but do nothing with
	// further sigints, namely the one that will be sent to the load monitoring
	// thread
	signal(SIGINT, sig_do_nothing);
}

void sig_do_nothing() {
	return;
}

void redirect_output(char* logfile) {
	FILE *f;

	f = freopen(logfile, "a", stdout);
	if(f == NULL) {
		ERRPRINTF("Error redirecting output to file: %s\n", logfile);
		exit(EXIT_FAILURE);
	}
}

void* loadmonitor(void *loadhistory) {
	struct pmm_loadhistory *h;
	struct pmm_load l;
	int ret;
    int rc;

    int sleep_for = 60; //number of seconds we wish to sleep for
    int sleep_for_counter = 0; //counter for how long we have slept
    int sleep_for_fraction = 5; //sleep in 5 second fractions

	int write_period = 10*60; //write the history to disk every 10 minutes
	int write_period_counter=0; //counter for writing history to disk

	h = (struct pmm_loadhistory*)loadhistory;

	LOGPRINTF("[loadmonitor]: h:%p\n", h);

	// read load history file
	//if loadfile exists ...
    //

	for(;;) {

		if((l.time = time(NULL)) == (time_t)-1) {
			ERRPRINTF("Error retreiving unix time.\n");
			exit(EXIT_FAILURE);
		}

		if((ret = getloadavg(l.load, 3)) != 3) {
			ERRPRINTF("Error retreiving load averages from getloadavg.\n");
			exit(EXIT_FAILURE);
		}


	    // lock the rwlock for writing
	    if((rc = pthread_rwlock_wrlock(&(h->history_rwlock))) != 0) {
	    	ERRPRINTF("Error aquiring write lock:%d\n", rc);
	    	exit(EXIT_FAILURE);
	    }

		//add load to load history data structure
		add_load(h, &l);

	    // unlock the rwlock
	    rc = pthread_rwlock_unlock(&(h->history_rwlock));

        //sleep for a total of "sleep_for" seconds, in sleep_for_fraction
        //segments ...
        sleep_for_counter = 0;
        while(sleep_for_counter<=sleep_for) {

            //check we have not received the quit signal
            pthread_mutex_lock(&signal_quit_mutex);
            if(signal_quit) {
                pthread_mutex_unlock(&signal_quit_mutex);

                // write load history to file using xml ...
                LOGPRINTF("signal_quit set, writing history file ...\n");
                if(write_loadhistory(h) < 0) {
                    perror("[loadmonitor]"); //TODO
                    ERRPRINTF("Error writing history.\n");
                    exit(EXIT_FAILURE);
                }

                return (void*)0;
            }
            pthread_mutex_unlock(&signal_quit_mutex);

            //sleep for fraction and add to
            sleep(sleep_for_fraction);

            sleep_for_counter += sleep_for_fraction;
        }

        write_period_counter += sleep_for;

        //write history when write_period seconds have elapsed since last write
		if(write_period_counter == write_period) {

			// write load history to file using xml ...
			LOGPRINTF("writing history to file ...\n");
			if(write_loadhistory(h) < 0) {
				perror("[loadmonitor]");
				ERRPRINTF("Error writing history.\n");
				exit(EXIT_FAILURE);
			}

		    write_period_counter = 0;
		}

	}
}

int main(int argc, char **argv) {

	struct pmm_config *cfg;
	struct pmm_routine *scheduled_r = NULL;
    int scheduled_status = 0;

    int rc;

	// benchmark thread variables
	int b_thread_rc;
	pthread_t b_thread_id = 0;
	pthread_attr_t b_thread_attr;
	void *b_thread_return;

	// load monitor thread variables
	int l_thread_rc;
	pthread_t l_thread_id = 0;
	pthread_attr_t l_thread_attr;

	// signal handler thread
    sigset_t signal_set;
	int s_thread_rc;
	pthread_t s_thread_id = 0;
	pthread_attr_t s_thread_attr;

	//register Ctrl+C signal handler (undone if we switch to daemon later)
	//if(signal(SIGINT, sig_cleanup) == SIG_IGN) {
	//	signal(SIGINT, SIG_IGN);
	//}
	//
    //

	// block all signals
	sigfillset(&signal_set);
	pthread_sigmask(SIG_BLOCK, &signal_set, NULL);

	// create signal handling thread
	//
	pthread_attr_init(&s_thread_attr);
	pthread_attr_setdetachstate(&s_thread_attr, PTHREAD_CREATE_JOINABLE);
	s_thread_rc = pthread_create(&s_thread_id, &s_thread_attr,
                                 signal_handler, NULL);
	if(s_thread_rc != 0) {
		ERRPRINTF("Error creating signal handler thread, return code: %d.\n",
		          s_thread_rc);
		exit(EXIT_FAILURE);
	}


	cfg = new_config();

	// parse arguments
	parse_args(cfg, argc, argv);

    xmlparser_init();

	// read configuation files
	rc = parse_config(cfg);
    if(rc < 0) {
        ERRPRINTF("Error parsing config.\n");
        exit(EXIT_FAILURE);
    }


	// load models
	rc = parse_models(cfg);
    if(rc < 0) {
        ERRPRINTF("Error loading models.\n");
        exit(EXIT_FAILURE);
    }

	// print configuration
	print_config(cfg);


	// if running as a deamon ...
	if(cfg->daemon) {
		LOGPRINTF("running as daemon ...\n");
		//redirect_output(cfg.logfile);
		run_as_daemon();
	}

	// read previous history
	rc = parse_history(cfg->loadhistory);
    if(rc < 0) {
        if(rc == -1) {
            //history file empty, start with empty history
        }
        else {
            ERRPRINTF("Error parsing load history.\n");
            exit(EXIT_FAILURE);
        }
    }

	// launch thread to record load history
	pthread_rwlock_init(&(cfg->loadhistory->history_rwlock), NULL);

	pthread_attr_init(&l_thread_attr);
	pthread_attr_setdetachstate(&l_thread_attr, PTHREAD_CREATE_JOINABLE);

	LOGPRINTF("Starting load monitor thread.\n");
	l_thread_rc = pthread_create(&l_thread_id, &l_thread_attr,
								 loadmonitor, (void *)cfg->loadhistory);

	if(l_thread_rc != 0) {
		ERRPRINTF("Error creating thread, return code: %d", l_thread_rc);
		exit(EXIT_FAILURE);
	}


	// possibly launch server thread (listens for requests from an cli utility
	// and from api calls, all over sockets)

	// initialize some benchmarking variables
	executing_benchmark = 0;

	pthread_attr_init(&b_thread_attr);

	pthread_attr_setdetachstate(&b_thread_attr, PTHREAD_CREATE_JOINABLE);
	pthread_mutex_init(&executing_benchmark_mutex, NULL);

	// main loop
	for(;;) {
		//DBGPRINTF("main loop: ...\n");

		//DBGPRINTF("main loop: locking executing_benchmark.\n");
		pthread_mutex_lock(&executing_benchmark_mutex);

		// if no benchmark is executing
		if(!executing_benchmark) {

			//DBGPRINTF("main loop: no benchmark executing.\n");

			// join any previous benchmark thread (should be finished by now)
			if(b_thread_id != 0) {

				DBGPRINTF("main loop: Joining previous benchmark thread.\n");

				b_thread_rc = pthread_join(b_thread_id, &b_thread_return);

				if(b_thread_rc != 0) {
					perror("[main]"); //TODO
					ERRPRINTF("Error joining previous thread.\n");
				}

                if(*(int*)b_thread_return == 1) { //quit signal
                    LOGPRINTF("Benchmark quit successful.\n");
                }
                else if(*(int*)b_thread_return == -1) { //failure
                    ERRPRINTF("Benchmark failure. Shutting down ...\n");
                    //trigger shutdown
                    kill(getpid(), SIGINT);

                    free(b_thread_return);
                    b_thread_return = NULL;

                    break;
                }

                //thread has been finished and cleaned up, reset b_thread_id
                b_thread_id = 0;

                free(b_thread_return);
                b_thread_return = NULL;

                if(cfg->pause == 1) {
                    printf("Press enter to continue ...");
                    getchar();
                }

			}

			scheduled_status = schedule_routine(&scheduled_r, cfg->routines,
                                                cfg->used);

			DBGPRINTF("schedule status: %i\n", scheduled_status);

			if(scheduled_status == 0){

                //if we are only building and not monitor load, terminate
                //program when all models are built
                if(cfg->build_only == 1) {
				    LOGPRINTF("All routines completed.\n");
				    kill(getpid(), SIGINT);
                }

                //TODO if all models are built we no longer need to attempt
                //to schedule one this scheduling loop should be terminated
			}
			else if(scheduled_status > 1) {
                DBGPRINTF("Currently no schedule-able routine.\n");
            }
            else {
                DBGPRINTF("main loop: routine picked for execution: %s\n",
                        scheduled_r->name);
                print_routine(scheduled_r);


                //launch benchmarking thread TODO seperate into function
                b_thread_rc = pthread_create(&b_thread_id, &b_thread_attr,
                                             benchmark, (void *)scheduled_r);
                if(b_thread_rc != 0) {
                    ERRPRINTF("Error pthread_create: rc:%d\n", b_thread_rc);
                } else {
                    //thread was created successfully
                    executing_benchmark = 1;
                }

            }
			//DBGPRINTF("[main]: unlocking executing_benchmark.\n");
			pthread_mutex_unlock(&executing_benchmark_mutex);

		}
		else {
			//DBGPRINTF("[main]: unlocking executing_benchmark.\n");
			pthread_mutex_unlock(&executing_benchmark_mutex);

			//DBGPRINTF("main loop: benchmarking executing, do nothing ...\n");

			/* here we are going to recheck that the currently executing bench
			 * mark still satisfies the executing conditions and pause/unpause
			 * or cancel as required */
		}

		// sleep for a period
        nanosleep(&(cfg->ts_main_sleep_period), NULL);



		//check signals

		// grab the mutex before looking at signal_quit
		pthread_mutex_lock(&signal_quit_mutex);
        if(signal_quit) {
		    pthread_mutex_unlock(&signal_quit_mutex);
            LOGPRINTF("signal_quit set, breaking from main loop ...\n");
            break;
        }
		// remember to release mutex
		pthread_mutex_unlock(&signal_quit_mutex);

	}

	//join benchmark thread, they will see global variable and exit promptly
	if(b_thread_id != 0)pthread_join(b_thread_id, NULL);
	pthread_join(l_thread_id, NULL);
	pthread_join(s_thread_id, NULL);

    //write models
    write_models(cfg);

	pthread_mutex_destroy(&signal_quit_mutex);
	pthread_mutex_destroy(&executing_benchmark_mutex);
	pthread_rwlock_destroy(&(cfg->loadhistory->history_rwlock));
	//pthread_exit(NULL); this allows a thread to continue executing after the
	//main has finished, don't think we want this here ...

	//close files

	//cleanup libxml
	xmlparser_cleanup();

	//free memory
	free_config(&cfg);


}

