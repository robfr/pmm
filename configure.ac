#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
#   Copyright (C) 2008-2010 Robert Higgins
#       Author: Robert Higgins <robert.higgins@ucd.ie>
#
#   This file is part of PMM.
#
#   PMM is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   PMM is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with PMM.  If not, see <http://www.gnu.org/licenses/>.
#

# AC_PROG_CC_C99 requires 2.59
# LT_INIT requires AC 2.60 and LT <something>
AC_PREREQ(2.60) 
LT_PREREQ([2.2.6])
AC_INIT([pmm],[0.0.1],[robert_higgins@iol.ie])
AC_CONFIG_MACRO_DIR([m4]) # include macros
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wno-portability])

AM_MAINTAINER_MODE

# Checks for programs.
LT_INIT
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AM_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_INSTALL

# PKG_PROG_PKG_CONFIG

AC_DEFINE([_GNU_SOURCE], 1,
          [Define 1 _GNU_SOURCE so we get necessary prototypes])


AX_PTHREAD()


AX_CHECK_GNU_MAKE()
#if test "x$_cv_gnu_make_command" = "x"; then
#    AC_MSG_ERROR([GNU make not found. Please install it or correct your path.])
#fi


AC_CHECK_LIB([m], [floor], ,AC_MSG_ERROR([No math library found]))


AC_ARG_ENABLE(debug,
    [   --enable-debug          enable debug flags and messages],,
    enable_debug=no)
# AM_CONDITIONAL(ENABLE_DEBUG, test "x$enable_debug" = xyes)
if test "x$enable_debug" = "xyes"; then
    CFLAGS="-g -O0 -Wall -Wextra $CFLAGS"    # reset C[XX]FLAGS to -g -O0 for debugging
    CXXFLAGS="-g -O0 -Wall -Wextra $CXXFLAGS"
    CPPFLAGS="$CPPFLAGS -DDEBUG"
    AC_DEFINE(ENABLE_DEBUG,1,[Enable debug flags and messages])
else
    CFLAGS="`tools/gcccpuopt 2>/dev/null` -O2 $CFLAGS"
    CXXFLAGS="`tools/gcccpuopt 2>/dev/null` -O2 $CXXFLAGS"
fi

AC_PATH_PROG(GNUPLOT, gnuplot)
if test "x$GNUPLOT" = "x"; then
    AC_MSG_WARN([gnuplot not found, model viewing tool will not be built])
    HAVE_GNUPLOT=no
else
    HAVE_GNUPLOT=yes
fi
AM_CONDITIONAL([HAVE_GNUPLOT], [test x$HAVE_GNUPLOT = xyes])
AC_SUBST(HAVE_GNUPLOT)

AC_PATH_PROG(PERL, perl)
if test "x$PERL" = "x"; then
    AC_MSG_ERROR([perl not found])
fi


dnl Choose working __FUNCTION__, __func__ or ""
AC_CACHE_CHECK(if __FUNCTION__ is defined,ac_cv_have_cpp___function__macro,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[printf("%s", __FUNCTION__);]])],[ac_cv_have_cpp___function__macro=yes],[ac_cv_have_cpp___function__macro=no])])
AC_CACHE_CHECK(if __func__ is defined,ac_cv_have_cpp___func__macro,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]], [[printf("%s", __func__);]])],[ac_cv_have_cpp___func__macro=yes],[ac_cv_have_cpp___func__macro=no])])
if test "x$ac_cv_have_cpp___function__macro" = "xyes"; then
  functionname="__FUNCTION__"
elif test "x$ac_cv_have_cpp___func__macro" = "xyes"; then
  functionname="__func__"
else
  functionname=" "
fi
AC_DEFINE_UNQUOTED(__FUNCNAME__,${functionname}, [Define to working version of __FUNCTION__, __func__ or blank])



dnl check for flavours of varargs macros (test from GLib)
AC_MSG_CHECKING(for ISO C99 varargs macros in C)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
int a(int p1, int p2, int p3);
#define call_a(...) a(1,__VA_ARGS__)
call_a(2,3);
]])],[dbus_have_iso_c_varargs=yes],[dbus_have_iso_c_varargs=no])
AC_MSG_RESULT($dbus_have_iso_c_varargs)

AC_MSG_CHECKING(for GNUC varargs macros)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
int a(int p1, int p2, int p3);
#define call_a(params...) a(1,params)
call_a(2,3);
]])],[dbus_have_gnuc_varargs=yes],[dbus_have_gnuc_varargs=no])
AC_MSG_RESULT($dbus_have_gnuc_varargs)

dnl Output varargs tests
if test "x$dbus_have_iso_c_varargs" = "xyes"; then
    AC_DEFINE(HAVE_ISO_VARARGS,1,[Have ISO C99 varargs macros])
fi
if test "x$dbus_have_gnuc_varargs" = "xyes"; then
    AC_DEFINE(HAVE_GNUC_VARARGS,1,[Have GNU-style varargs macros])
fi


AC_ARG_WITH(papi,
    [  --with-papi[=path]   specify use of (and optional path to) PAPI],,
    with_papi=no)
if test "x$with_papi" != "xno"; then #if papi is enabled
    if test "x$with_papi" != "xyes"; then #if a path to papi is specified
        PAPI_CPPFLAGS="-I$with_papi/include"
        PAPI_LDFLAGS="-L$with_papi/lib -lpapi"
    else
        PAPI_CPPFAGS=""
        PAPI_LDFLAGS="-lpapi"
    fi

    
    OLD_LIBS=$LIBS
    OLD_CPPFLAGS=$CPPFLAGS

    LIBS="$PAPI_LDFLAGS $LIBS"
    CPPFLAGS="$CPPFLAGS $PAPI_CPPFLAGS"

    AC_CHECK_HEADERS([papi.h],,AC_MSG_ERROR([papi.h header not found]))
    AC_CHECK_LIB([papi], [PAPI_library_init],,
                 AC_MSG_ERROR([No PAPI library found]))

    AC_MSG_CHECKING([for functionality of PAPI_flops()])
    AC_RUN_IFELSE(
        [AC_LANG_PROGRAM([[#include <papi.h>
                           float ireal_t,iproc_t,imflops;
                           long long iflpops;]],
                         [[return PAPI_flops(&ireal_t,&iproc_t,&iflpops,
                                             &imflops) == PAPI_OK ? 0 : 1;]],
                         )],
                         [papi_flops_works=yes],
                         [papi_flops_works=no],
                         [papi_flops_works=cross])
    AC_MSG_RESULT($papi_flops_works)

    if test "x$papi_flops_works" = "xno"; then
        AC_MSG_ERROR([PAPI_flops() failed, your platform may not support floating point operation event counting.]);
    fi


    LIBS=$OLD_LIBS
    CPPFLAGS=$OLD_CPPFLAGS

    AC_DEFINE(HAVE_PAPI,1,[Have PAPI])

fi
AC_SUBST(PAPI_CPPFLAGS)
AC_SUBST(PAPI_LDFLAGS)

# TODO this should be AC_ARG_WITH ...
AC_ARG_ENABLE(octave,
    [  --disable-octave   do not use octave (disables 2+ dimensional models],
    enable_octave=$enableval, enable_octave=yes)
AM_CONDITIONAL(ENABLE_OCTAVE, test "x$enable_octave" = "xyes")
if test "x$enable_octave" = "xyes"; then

    # check for liboctave credit: octave-gui-4.0
    AC_MSG_CHECKING(for mkoctfile)
    AC_TRY_EVAL(mkoctfile)
    if test "x$ac_status" != "x0"; then
        AC_MSG_ERROR([mkoctfile is not in the path])
    fi
    AC_MSG_RESULT(yes)

    AC_MSG_CHECKING(for liboctave compile options)
    c1=`mkoctfile -p FPICFLAG`
    c2=`mkoctfile -p INCFLAGS`
    c3=`mkoctfile -p XTRA_CXXFLAGS`

    l1=`mkoctfile -p RDYNAMIC_FLAG`
    l2=`mkoctfile -p RLD_FLAG`
    l3=`mkoctfile -p LFLAGS`
    l4=`mkoctfile -p OCTAVE_LIBS`
    l5=`mkoctfile -p FLIBS`
    OCTAVE_CXXFLAGS="$c1 $c2 $c3"
    #FOO="$l1 $l2 $l3 $l4 $l5"
    #BAR=`echo "$FOO" | sed 's/c:/ ass /'`
    #echo $FOO
    #echo $BAR
    OCTAVE_LIBS="$l1 $l2 $l3 $l4 $l5"
    AC_MSG_RESULT(ok)

    AC_MSG_CHECKING(whether we can build a liboctave program)
    AC_LANG(C++)
    OLD_LIBS="$LIBS"
    LIBS="$OCTAVE_LIBS $LIBS"
    OLD_CXXFLAGS="$CXXFLAGS"
    CXXFLAGS="$OCTAVE_CXXFLAGS $CXXFLAGS"
    AC_LINK_IFELSE([AC_LANG_SOURCE([
            #include <octave/oct.h>
            int main(void) { string_vector foo; }
            ])],,AC_MSG_ERROR([Cannot link against liboctave]))
    AC_MSG_RESULT(yes)

    # successfully tested for octave, ...
    AC_LANG(C)
    LIBS="$OLD_LIBS"
    CXXFLAGS="$OLD_CXXFLAGS"
    AC_SUBST(OCTAVE_CXXFLAGS)
    AC_SUBST(OCTAVE_LIBS)
    ENABLE_OCTAVE=yes
    AC_DEFINE(ENABLE_OCTAVE,1,[Have Octave])
fi
AC_SUBST(ENABLE_OCTAVE)


# test for muParser library
AC_ARG_WITH(muparser,
    [  --with-muparser[=path]   specify use of (and optional path to) muParser],
    ,with_muparser=check)
if test "x$with_muparser" != "xno"; then # if muParser is not disabled

    if test "x$with_muparser" != "xcheck" &&
       test "x$with_muparser" != "xyes";
    then # if path for muPaser is spec'd
        MUPARSER_CPPFLAGS="-I$with_muparser/include"
        MUPARSER_LDFLAGS="-L$with_muparser/lib"
    else
        MUPARSER_CPPFLAGS=""
        MUPARSER_LDFLAGS=""
    fi


    OLD_CPPFLAGS="$CPPFLAGS"
    OLD_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$MUPARSER_CPPFLAGS $CPPFLAGS"
    LDFLAGS="$MUPARSER_LDFLAGS $LDFLAGS"

    AC_CHECK_HEADERS([muParser/muParserDLL.h],
    [

        AC_CHECK_LIB([muparser], [_init],
        [
            MUPARSER_LIBS="-lmuparser"
            HAVE_MUPARSER=yes
            AC_DEFINE(HAVE_MUPARSER,1,[Have muParser])
        ],
        [
            # fail if with-muparser was explicity set
            if test "x$with_muparser" != "xcheck"; then
                AC_MSG_ERROR([muParser not found.])
            fi
         ],)
    ],
    [
        # fail if with-muparser was explicity set
        if test "x$with_muparser" != "xcheck"; then
            AC_MSG_ERROR([muParser not found.])
        fi
    ])

    CPPFLAGS="$OLD_CPPFLAGS"
    LDFLAGS="$OLD_LDFLAGS"
fi
AM_CONDITIONAL([HAVE_MUPARSER], [test x$HAVE_MUPARSER = xyes])
AC_SUBST(MUPARSER_LIBS)
AC_SUBST(MUPARSER_CPPFLAGS)
AC_SUBST(MUPARSER_LDFLAGS)
AC_SUBST(HAVE_MUPARSER)

# test for GotoBLAS2
AC_ARG_WITH(gotoblas2,
    [ --with-gotoblas2[=path] specify use of GotoBLAS2 benchmarks],,
    with_gotoblas2=check)
if test "x$with_gotoblas2" != "xno"; then # if gotoblas2 is not disabled
    if test "x$with_gotoblas2" != "xcheck" &&
       test "x$with_gotoblas2" != "xyes";
    then # if a path has been specified
        GOTOBLAS2_CPPFLAGS="-I$with_gotoblas2/include"
        GOTOBLAS2_LDFLAGS="-L$with_gotoblas2/lib";
    else
        GOTOBLAS2_CPPFLAGS=""
        GOTOBLAS2_LDFLAGS=""
    fi

    OLD_CPPFLAGS="$CPPFLAGS"
    OLD_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$GOTOBLAS2_CPPFLAGS $CPPFLAGS"
    LDFLAGS="$GOTOBLAS2_LDFLAGS $LDFLAGS"

    AC_CHECK_LIB([goto2],[main],
    [
        AC_CHECK_HEADER([cblas.h],
        [
            GOTOBLAS2_LIBS="-lgoto2 -lgfortran"
            HAVE_GOTOBLAS2=yes
        ],
        [
            if test "x$with_gotoblas2" != "xcheck"; then
                AC_MSG_ERROR([GotoBLAS2 not found.])
            fi
        ])
    ],
    [
        if test "x$with_gotoblas2" != "xcheck"; then
            AC_MSG_ERROR([GotoBLAS2 not found.])
        fi
    ],
    [$PTHREAD_CFLAGS -lgfortran])

    CPPFLAGS="$OLD_CPPFLAGS"
    LDFLAGS="$OLD_LDFLAGS"

fi
AM_CONDITIONAL([HAVE_GOTOBLAS2], [test x$HAVE_GOTOBLAS2 = xyes])
AC_SUBST(GOTOBLAS2_LIBS)
AC_SUBST(GOTOBLAS2_CPPFLAGS)
AC_SUBST(GOTOBLAS2_LDFLAGS)
AC_SUBST(HAVE_GOTOBLAS2)

# test for ATLAS 
AC_ARG_WITH(atlas,
    [  --with-atlas[=path] specify use of (and optional path to) ATLAS],,
    with_atlas=check)
if test "x$with_atlas" != "xno"; then # if atlas is not disabled

    if test "x$with_atlas" != "xcheck" &&
       test "x$with_atlas" != "xyes";
    then # if a path has been specified
        ATLAS_CPPFLAGS="-I$with_atlas/include"
        ATLAS_LDFLAGS="-L$with_atlas/lib";
    else
        ATLAS_CPPFLAGS=""
        ATLAS_LDFLAGS=""
    fi

    OLD_CPPFLAGS="$CPPFLAGS"
    OLD_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$ATLAS_CPPFLAGS $CPPFLAGS"
    LDFLAGS="$ATLAS_LDFLAGS $LDFLAGS"

    ATLAS_LIBS="-lm"

    # first test for atlas
    AC_CHECK_LIB([atlas],[ATL_xerbla],
    [
        ATLAS_LIBS="-latlas $ATLAS_LIBS"
    ],
    [
        if test "x$with_atlas" != "xcheck"; then
            AC_MSG_ERROR([libatlas not found.])
        fi
    ],
    [$ATLAS_LIBS])

    # then check for ptcblas/cblas
    AC_CHECK_LIB([ptcblas],[cblas_dgemm],
    [
            ATLAS_LIBS="-lptcblas $PTHREAD_CFLAGS $ATLAS_LIBS"
            HAVE_ATLAS=yes
    ],
    [
        AC_CHECK_LIB([cblas],[cblas_dgemm],
        [
            ATLAS_LIBS="-lcblas $ATLAS_LIBS"
                HAVE_ATLAS=yes
        ],
        [
            if test "x$with_atlas" != "xcheck"; then
                AC_MSG_ERROR([libptcblas/libcblas not found.])
            fi
        ],
        [$ATLAS_LIBS])
    ],
    [$ATLAS_LIBS $PTHREAD_CFLAGS])

    CPPFLAGS="$OLD_CPPFLAGS"
    LDFLAGS="$OLD_LDFLAGS"

fi
AM_CONDITIONAL([HAVE_ATLAS], [test x$HAVE_ATLAS = xyes])
AC_SUBST(ATLAS_LIBS)
AC_SUBST(ATLAS_CPPFLAGS)
AC_SUBST(ATLAS_LDFLAGS)
AC_SUBST(HAVE_ATLAS)

# test for LAPACK ... not sure about this ... maybe test for atlas?
AC_ARG_WITH(lapack,
    [  --with-lapack[=path] specify use of (and optional path to) LAPACK],,
    with_lapack=check)
if test "x$with_lapack" != "xno"; then # if lapack is not disabled

    if test "x$with_lapack" != "xcheck" &&
       test "x$with_lapack" != "xyes";
    then # if a path has been specified
        LAPACK_CPPFLAGS="-I$with_lapack/include"
        LAPACK_LDFLAGS="-L$with_lapack/lib";
    else
        LAPACK_CPPFLAGS=""
        LAPACK_LDFLAGS=""
    fi

    OLD_CPPFLAGS="$CPPFLAGS"
    OLD_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$LAPACK_CPPFLAGS $CPPFLAGS"
    LDFLAGS="$LAPACK_LDFLAGS $LDFLAGS"

    LAPACK_LIBS="-latlas -lm"

    AC_CHECK_LIB([cblas],[cblas_dsyrk],
    [

        LAPACK_LIBS="-lcblas $LAPACK_LIBS"

        AC_CHECK_LIB([lapack_atlas],[clapack_dpotrf],
        [
            LAPACK_LIBS="-llapack_atlas $LAPACK_LIBS"
            HAVE_LAPACK=yes
        ],
        [
            if test "x$with_lapack" != "xcheck"; then
                AC_MSG_ERROR([LAPACK not found.])
            fi
        ],
        [$LAPACK_LIBS])
    ],
    [
        if test "x$with_lapack" != "xcheck"; then
            AC_MSG_ERROR([libcblas not found.])
        fi
    ],
    [$LAPACK_LIBS])

    CPPFLAGS="$OLD_CPPFLAGS"
    LDFLAGS="$OLD_LDFLAGS"

fi
AM_CONDITIONAL([HAVE_LAPACK], [test x$HAVE_LAPACK = xyes])
AC_SUBST(LAPACK_LIBS)
AC_SUBST(LAPACK_CPPFLAGS)
AC_SUBST(LAPACK_LDFLAGS)
AC_SUBST(HAVE_LAPACK)



# test for Gnu Scientific Library
AC_ARG_WITH(gsl,
    [  --with-gsl[=path]   specify use of (and optional path to) GSL],,
    with_gsl=check)
if test "x$with_gsl" != "xno"; then #if gsl is not disabled 

    if test "x$with_gsl" != "xcheck" &&
       test "x$with_gsl" != "xyes";
    then #if a path for gsl is specified
        GSL_CPPFLAGS="-I$with_gsl/include"
        GSL_LDFLAGS="-L$with_gsl/lib"
    else
        GSL_CPPFAGS=""
        GSL_LDFLAGS=""
    fi


    OLD_CPPFLAGS="$CPPFLAGS"
    OLD_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$GSL_CPPFLAGS $CPPFLAGS"
    LDFLAGS="$GSL_LDFLAGS $LDFLAGS"

    AC_CHECK_HEADERS([gsl/gsl_blas.h gsl/gsl_statistics_double.h],
    [

        AC_CHECK_LIB([gslcblas],[cblas_dgemm],
        [
            AC_CHECK_LIB([gsl],[gsl_blas_dgemm],
            [
                GSL_LIBS="-lgsl -lgslcblas -lm"
                GSL_SUBDIR=gsl
                HAVE_GSL=yes
            ],
            [
                # if we have explicitly indicated --with-gsl then fail
                if test "x$with_gsl" != "xcheck"; then
                    AC_MSG_ERROR([GSL not found.])
                fi
            ],
            [-lgslcblas -lm])
        ],
        [
            if test "x$with_gsl" != "xcheck"; then
                AC_MSG_ERROR([GSL not found.])
            fi
        ],
        [-lm])
    ],
    [
        # if we have explicitly indicated --with-gsl then fail
        if test "x$with_gsl" != "xcheck"; then
            AC_MSG_ERROR([GSL headers not found])
        fi
    ])


    CPPFLAGS="$OLD_CPPFLAGS"
    LDFLAGS="$OLD_LDFLAGS"

fi
AM_CONDITIONAL([HAVE_GSL], [test x$HAVE_GSL = xyes])
AC_SUBST(GSL_LIBS)
AC_SUBST(GSL_CPPFLAGS)
AC_SUBST(GSL_LDFLAGS)
AC_SUBST(GSL_SUBDIR)
AC_SUBST(HAVE_GSL)


# if we have sched_setaffinity we might use it for mutli-thread/multi-core
AC_CHECK_FUNC([sched_setaffinity],
[
        AC_DEFINE(HAVE_SETAFFINITY,1,[Have sched_setaffinity])
]
,,)


# Checking for libxml2 ....
#
# pkg-config libraries (just libxml2 at the moment)
#PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.6.0)
#
AC_PATH_PROG(XML2_CONFIG, xml2-config)

if test "x$XML2_CONFIG" = "x"; then
    AC_MSG_ERROR([libxml2 not present or configured])
else
    XML_CFLAGS="`$XML2_CONFIG --cflags`"
    XML_LIBS="`$XML2_CONFIG --libs`"
fi
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

OLD_LIBS="$LIBS"
LIBS="$XML_LIBS $LIBS"

AC_CHECK_LIB(xml2, [xmlFreeDoc], , , $XML_CFLAGS)
AC_CHECK_FUNC(xmlRegisterNodeDefault, , AC_MSG_ERROR([libxml2 >= 2.6.0 required]))
LIBS="$OLD_LIBS"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([paths.h stdlib.h string.h sys/param.h sys/time.h])
AC_CHECK_HEADERS([unistd.h utmp.h fcntl.h limits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_FORK
#AC_FUNC_GETLOADAVG TODO - fix this, fails: 'error: ././getloadavg.c is missing'
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_MKTIME
if test "x$ac_cv_func_working_mktime" = "xno"; then
    AC_MSG_ERROR([Working mktime not found])
fi


AC_FUNC_STAT

AC_CHECK_FUNCS([dup2 getpid getrusage gettimeofday localtime_r memset strchr \
               strdup strrchr strstr],,
               AC_MSG_ERROR([Required library function not found]))


#set aux config directory
AC_SUBST(ac_aux_dir)


if test "x$enable_maintainer_mode" = "xyes"; then
    AC_CONFIG_FILES([
        doc/Makefile
        doc/gbbe/Makefile])
fi

AC_CONFIG_FILES([
	etc/pmmd.conf.sh
])

AC_CONFIG_FILES([
	Makefile
	etc/Makefile
	routines/Makefile
	routines/example/Makefile
	routines/gsl/Makefile
	routines/gsl/blas/Makefile
    routines/lapack/Makefile
    routines/lapack/double/Makefile
	routines/cblas/Makefile
    routines/sim/Makefile
    routines/sim/chps/Makefile
    routines/sim/chps/data/Makefile
    routines/sim/ben/Makefile
    routines/sim/ben/data/Makefile
	src/Makefile
	test/Makefile
])

AC_OUTPUT
