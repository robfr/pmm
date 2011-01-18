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
 * @file pmm_cond.c
 *
 * @brief Functions to test system conditions
 *
 * this file contains the code for testing system conditions which may limit
 * or permit the construction of models by the pmm daemon
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>         // for getloadavg
#include <stdio.h>          // for snprintf
#include <utmp.h>           // for utmp
#include <paths.h>          // for _UTMP_PATH
#include <sys/types.h>      // for stat
#include <sys/stat.h>       // for stat
#include <unistd.h>         // for stat
#include <sys/param.h>      // for MAXPATHLEN

#include "pmm_model.h"
#include "pmm_cond.h"

int ttystat(char *line, int sz);
int num_users();

/*!
 * checks that the tty a user is logged into (as per utmp) exists in /dev
 *
 * This indicates whether a user has an interactive session or not
 *
 *
 * code comes from FreeBSD /usr/src/usr.bin/w/w.c
 *
 * @param   line    pointer to line of the utmp file
 * @param   sz      size of the line
 *
 * @return 1 if user has a tty, 0 if user does not.
 *
 */
int
ttystat(char *line, int sz)
{
    // TODO portable ?
    static struct stat sb;
    char ttybuf[MAXPATHLEN];

    //create the string /dev/<terminal user is logged into>
    (void)snprintf(ttybuf, sizeof(ttybuf), "%s%.*s", _PATH_DEV, sz, line);

    //check that the terminal/file exists by stat'ing it
    if(stat(ttybuf, &sb) == 0) {
        return 1; // terminal exists, great
    }
    else {
        return 0; // terminal does not exist, corrupt utmp line
    }
}


/*!
 * counts the number of users logged into a system by reading utmp returns
 * the number of users logged in.
 *
 * code comes from FreeBSD /usr/src/usr.bin/w/w.c
 *
 * @return number of users logged in
 */
int
num_users()
{
    //TODO portable on windows/NON-posix ?
    FILE *ut;
    struct utmp utmp;
    int nusers;

    //open utmp file stream
    if((ut = fopen(_PATH_UTMP, "r")) == NULL) {
        printf("Error calculating users logged in, could not read file %s\n",
                _PATH_UTMP);
        exit(EXIT_FAILURE);
    }

    nusers=0;
    for(;fread(&utmp, sizeof(utmp), 1, ut);) {

        if(utmp.ut_name[0] == '\0') {
            continue;
        }

        //check that the tty if the user exists in /dev
        if(!ttystat(utmp.ut_line, UT_LINESIZE)) {
            continue; //corrupted record
        }

        nusers++;
    }
    fclose(ut);

    return nusers;
}

/*
 * Test system for users
 *
 * @return 1 if users are logged into system, 0 if not
 */
int cond_users() {
    if(num_users() > 0) {
        return 1;
    } else {
        return 0;
    }
}

/*!
 * test system for idleness
 *
 * true if 5 minute load is below 0.1
 *
 * return 0 if not idle, 1 if idle
 */
int cond_idle() {
    double loadavg[3];

    if(!getloadavg(loadavg, 3)) {
        printf("Error, could not retreive system load averages.\n");
        exit(EXIT_FAILURE);
    }

    // if load is below 0.10, i.e. 10% untilisation
    if(loadavg[1] < 0.10) {
        return 1;
    }
    else {
        return 0;
    }
}

/*!
 * check if conditions for execution of a routine are satisfied and
 * set executable parameter of routine accordingly
 *
 * @param   r   pointer to the routine
 *
 * @return 0 if routine is not execuable based on conditions, 1 if
 * it is
 */
int
check_conds(struct pmm_routine *r)
{
    // TODO add CC_UNTIL and CC_PERIODIC to this function ...
    // TODO permit multiple conditions in one routine
    // TODO permit system wide conditions applicable to all routines
    //
    r->executable = 0;
    if(r->condition == CC_NOW) {
        r->executable = 1;

    }
    else if(r->condition == CC_IDLE) {
        if(cond_idle()) {
            r->executable = 1;
        }
    }
    else if(r->condition == CC_NOUSERS) {
        if(!cond_users()) {
            r->executable = 1;
        }
    }

    return r->executable;
}

