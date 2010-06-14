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
		-I$(top_srcdir) -I$(top_srcdir)/src 

if ENABLE_OCTAVE
bin_PROGRAMS = octave_test #executor_test #test_cfgparser timer_test

octave_test_SOURCES = octave_test.c
octave_test_LDADD = -L$(top_builddir)/src $(XML_LIBS) $(LIBS) $(OCTAVE_LIBS)
octave_test_LDFLAGS = -lpmm $(XML_CFLAGS) $(CFLAGS) $(OCTAVE_LIBS)
octave_test_CPPFLAGS = $(XML_CFLAGS)
endif
