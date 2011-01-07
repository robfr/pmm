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

INCLUDES	= -I$(top_builddir) -I$(top_builddir)/src \
		-I$(top_srcdir) -I$(top_srcdir)/src \
		$(XML_CFLAGS) $(CFLAGS)

DEPENDS = \
  $(top_builddir)/src/libpmm.la


pkglibexec_PROGRAMS	= example

example_SOURCES = example.c

example_LDADD = -L$(top_builddir)/src $(LIBS) $(XML_LIBS)

example_LDFLAGS = -lpmm $(CFLAGS)

example_DEPENDENCIES = $(DEPENDS)
