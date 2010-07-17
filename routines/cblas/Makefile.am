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


LIBS += -lpmm
LDFLAGS += -L$(top_builddir)/src

pkglib_PROGRAMS = 

if HAVE_GOTOBLAS2
pkglib_PROGRAMS += gotoblas2_dgemm gotoblas2_dgemm_stripe
endif

if HAVE_ATLAS
pkglib_PROGRAMS += atlas_dgemm atlas_dgemm_stripe
endif

gotoblas2_dgemm_SOURCES = cblas_dgemm.c
gotoblas2_dgemm_LDADD = $(GOTOBLAS2_LIBS)
gotoblas2_dgemm_CPPFLAGS = $(GOTOBLAS2_CPPFLAGS)
gotoblas2_dgemm_LDFLAGS = $(GOTOBLAS2_LDFLAGS)

gotoblas2_dgemm_stripe_SOURCES = cblas_dgemm_stripe.c
gotoblas2_dgemm_stripe_LDADD = $(GOTOBLAS2_LIBS)
gotoblas2_dgemm_stripe_CPPFLAGS = $(GOTOBLAS2_CPPFLAGS)
gotoblas2_dgemm_stripe_LDFLAGS = $(GOTOBLAS2_LDFLAGS)

atlas_dgemm_SOURCES = cblas_dgemm.c
atlas_dgemm_LDADD = $(ATLAS_LIBS)
atlas_dgemm_LDFLAGS = $(ATLAS_LDFLAGS)
atlas_dgemm_CPPFLAGS = $(ATLAS_CPPFLAGS)

atlas_dgemm_stripe_SOURCES = cblas_dgemm_stripe.c
atlas_dgemm_stripe_LDADD = $(ATLAS_LIBS)
atlas_dgemm_stripe_LDFLAGS = $(ATLAS_LDFLAGS)
atlas_dgemm_stripe_CPPFLAGS = $(ATLAS_CPPFLAGS)
