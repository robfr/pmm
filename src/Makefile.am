## Makefile.am -- Process this file with automake to produce Makefile.in
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

##MAINTAINERCLEANFILES = Makefile.in

#INCLUDES	= -I$(top_builddir) -I$(top_builddir)/src \
#		-I$(top_srcdir) -I$(top_srcdir)/src

CC		= $(PTHREAD_CC)

# noinst_HEADERS	= pmm_argparser.h pmm_cfgparser.h pmm_cond.h pmm_data.h \
#		pmm_executor.h pmm_scheduler.h pmm_util.h

bin_PROGRAMS	= pmmd pmm_view

if ENABLE_OCTAVE
if HAVE_GSL
bin_PROGRAMS += pmm_comp
endif
endif

pmmd_DEPEDENCIES = libpmm.la
pmmd_SOURCES	= pmm_main.c pmm_scheduler.c pmm_executor.c \
		pmm_argparser.c pmm_selector.c
pmmd_LDADD	= $(PTHREAD_LIBS)
pmmd_LDFLAGS	= -lpmm $(PTHREAD_CFLAGS)
pmmd_CPPFLAGS = $(XML_CFLAGS) $(PTHREAD_CFLAGS)
pmmd_CXXFLAGS = $(OCTAVE_CXXFLAGS)


pmm_view_DEPENDENCIES = libpmm.la
pmm_view_SOURCES = pmm_view.c gnuplot_i.c pmm_argparser.c
pmm_view_LDFLAGS = -lpmm 
pmm_view_CPPFLAGS = $(XML_CFLAGS)
pmm_view_CXXFLAGS = $(OCTAVE_CXXFLAGS)


pmm_comp_DEPENDENCIES = libpmm.la
pmm_comp_SOURCES = pmm_comp.c
pmm_comp_LDFLAGS = -lpmm $(GSL_LDFLAGS) $(GSL_LIBS)
pmm_comp_CPPFLAGS = $(XML_CFLAGS)
pmm_comp_CXXFLAGS = $(OCTAVE_CXXFLAGS)


lib_LTLIBRARIES = libpmm.la

libpmm_la_SOURCES = pmm_util.c pmm_data.c pmm_cfgparser.c pmm_cond.c \
		pmm_octave.cc pmm_muparse.cc
libpmm_la_LDFLAGS =  $(XML_LIBS) $(OCTAVE_LIBS) $(PAPI_LDFLAGS) $(MUPARSER_LIBS) $(MUPARSER_LDFLAGS)
libpmm_la_CPPFLAGS = $(XML_CFLAGS) $(PAPI_CPPFLAGS) \
					 -DPKGDATADIR=\"$(pkgdatadir)\" \
 					 -DSYSCONFDIR=\"$(sysconfdir)\" \
					 -DLOCALSTATEDIR=\"$(localstatedir)\"
libpmm_la_CXXFLAGS = $(OCTAVE_CXXFLAGS) $(MUPARSER_CPPFLAGS)


pkgdata_DATA = pmm_griddatan.m

EXTRA_DIST	= pmm_argparser.h pmm_cfgparser.h pmm_cond.h pmm_data.h \
		pmm_executor.h pmm_scheduler.h pmm_util.h pmm_selector.h gnuplot_i.h \
		pmm_octave.h pmm_log.h pmm_muparse.h pmm_griddatan.m

##pmm_LDADD	= $(top_builddir)/src/libpmm.a \
##		$(top_builddir)/replace/libreplace.a $(LIBADD_READLINE)

