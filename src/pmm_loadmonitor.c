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
 * @file    pmm_loadmonitor.c
 * @brief   Code for monitoring the load on a machine
 *
 * Contains the load monitoring thread of pmm
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif


#include <pthread.h>   // for pthreads
#include <stdio.h>      //for perror
#include <stdlib.h>     // for getloadavg/exit

#include "pmm_load.h"
#include "pmm_log.h"
#include "pmm_cfgparser.h"

extern int signal_quit;
extern pthread_mutex_t signal_quit_mutex;

/*!
 * load monitor thread
 *
 * monitors and writes load to disk until a quit signal is detected
 *
 * @param   loadhistory     void pointer to the load history structue
 *
 * @return void pointer to integer describing return status, 0 for success
 * -1 for failure
 */
//TODO exit gracefully on failure
void*
loadmonitor(void *loadhistory)
{
    struct pmm_loadhistory *h;
    struct pmm_load l;
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

        if(getloadavg(l.load, 3) != 3) {
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

