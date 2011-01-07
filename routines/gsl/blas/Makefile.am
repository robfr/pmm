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


INCLUDES	= -I$(top_builddir) -I$(top_builddir)/src \
		-I$(top_srcdir) -I$(top_srcdir)/src \
		$(CFLAGS)

LIBS += $(GSL_LIBS) $(XML_LIBS) -lpmm
LDFLAGS += -L$(top_builddir)/src

pkglibexec_PROGRAMS = dgemm dgemm2 dgemm3 dsyrk

dgemm_SOURCES = dgemm.c

dgemm2_SOURCES = dgemm2.c

dgemm3_SOURCES = dgemm3.c

dsyrk_SOURCES = dsyrk.c
