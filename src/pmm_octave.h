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
#ifndef PMM_OCTAVE_H_
#define PMM_OCTAVE_H_

#if HAVE_CONFIG_H
#include "config.h"

/* undefine variables that are also defined in oct.h */
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
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#endif

typedef struct pmm_octave_data {
    Matrix x;
    ColumnVector y;
    Matrix tri;
} PMM_Octave_Data;


void octave_init();
struct pmm_octave_data*
fill_octave_input_matrices(struct pmm_model *m);
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
