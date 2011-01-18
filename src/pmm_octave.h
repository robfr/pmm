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
 * @file    pmm_octave.h
 * @brief   data structure for interaction with octave
 */
#ifndef PMM_OCTAVE_H_
#define PMM_OCTAVE_H_

#if HAVE_CONFIG_H
#include "config.h"

/* undefine variables that are also defined in oct.h */
#undef PACKAGE
#undef PACKAGE_URL
#undef VERSION
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#endif


#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
typedef struct Matrix {} Matrix;
typedef struct ColumnVector {} ColumnVector;
#else
#include <octave/oct.h>
/* undefine variables that are also defined in oct.h */
#undef PACKAGE
#undef PACKAGE_URL
#undef VERSION
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#endif

/*!
 * Structure containing octave objects which are used in interpolation of
 * models
 */
typedef struct pmm_octave_data {
    Matrix x;           /*!< matrix containing the parameter values */
    ColumnVector y;     /*!< vector containing the value of model at @a x */
    Matrix tri;         /*!< matix containing the triangulation of the point
                             cloud described by @a x and @a y */
} PMM_Octave_Data;

#define PMM_ALL 0 /*!< use all points */
#define PMM_AVG 1 /*!< use average of points with identical parameters */
#define PMM_MAX 2 /*!< use maximum of points with identical parameters */

void octave_init();
struct pmm_octave_data*
fill_octave_input_matrices(struct pmm_model *m, int mode);
int
octave_triangulate(struct pmm_octave_data *oct_data);
double
octave_interp(struct pmm_octave_data *oct_data, int *p, int n);
double*
octave_interp_array(struct pmm_octave_data *oct_data, int **p, int n, int l);

double
octave_interpolate(struct pmm_octave_data *oct_data, int *p, int n);

struct pmm_benchmark*
interpolate_griddatan(struct pmm_model *m, int *p);




#ifdef __cplusplus
}
#endif

#endif /*PMM_OCTAVE_H_*/
