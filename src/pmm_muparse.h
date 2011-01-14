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
 * @file    pmm_muparse.h
 * @brief   muparser structures
 *
 */
#ifndef PMM_MUPARSE_H_
#define PMM_MUPARSE_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


#ifndef __cplusplus
typedef struct Parser {} Parser;
#else
#include <muParser/muParser.h>
using namespace mu;
#endif

/*!
 * Structure to hold muparser object and variable array
 */
typedef struct pmm_param_constraint_muparser {
    Parser p;       /*!< muparser parser object */
    double *vars;   /*!< variable array that will be linked to parser object */
    int n_p;        /*!< length of variable array */
} PMM_Param_Constraint_Muparser;

int
create_param_constraint_muparser(struct pmm_paramdef_set *pd_set);

int
evaluate_constraint_with_params(struct pmm_param_constraint_muparser* pc_parser,
                                int* params, double *value);

int
evaluate_constraint(struct pmm_param_constraint_muparser* pc_parser,
                    double *value);
#ifdef __cplusplus
} /* for extern "C" */
#endif


#endif /* PMM_MUPARSE_H_ */

