/*
    Copyright (C) 2008-2010 Robert Higgins
        Author: Robert Higgins <robert.higgins@ucd.ie>
        Author: David Clarke <dave.p.clarke@gmail.com>

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
 * @file    pmm_scheduler.c
 * @brief   Choose next routine to benchmark
 *
 * Code to select next routine that should be benchmarked.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "pmm_model.h"
#include "pmm_cond.h"

/*!
 * Function handles the chosing of the next routine to benchmark. 
 *
 * @param   scheduled       pointer to pointer describing routine picked for
 *                          scheduling
 * @param   r               pointer to array of routines from which to pick
 * @param   n               number of routines in array
 *
 * @return 0 if all routines are already complete (so we don't need to try
 * to schedule them any longer), 1 if there is a schedulable routine, 2 if
 * there are no schedulable routines at present (but some are still incomplete)
 *
 */
int
schedule_routine(struct pmm_routine** scheduled, struct pmm_routine** r, int n) {

	int i, status = 0;
	*scheduled = NULL;


	//iterate over r
	for(i=0; i<n; i++) {

	    check_conds(r[i]);

		//check routine is executable and model is not complete
		if(r[i]->executable &&
		   !r[i]->model->complete) { //TODO possibly rearrange these checks
			//if no routine is scheduled
			if(*scheduled == NULL) {
				*scheduled = r[i];
				status = 1;
			}

			//if priority is greater change scheduled
			else if(r[i]->priority > (*scheduled)->priority) {
				*scheduled = r[i];
			}

			//if priorities are equal then keep the least complete (or
			//do nothing if completion is equal.
			else if(r[i]->priority == (*scheduled)->priority) {
				if(r[i]->model->completion < (*scheduled)->model->completion) {
					*scheduled = r[i];
				}
			}
		}
		//check if there is routines incomplete and not executable
		if(!r[i]->model->complete && status == 0)
			status = 2;
	}

	return status;
}
