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


auxdir		= @ac_aux_dir@

#TODO check this stuff, extra files to inculde in distribution ...
AUX_DIST	= $(auxdir)/config.guess \
		$(auxdir)/config.sub \
		$(auxdir)/ltmain.sh \
		$(auxdir)/install-sh \
		$(auxdir)/missing \
		$(auxdir)/mkinstalldirs

AUX_DIST_EXTRA	= m4/acx_pthread.m4 m4/ax_check_gnu_make.m4

EXTRA_DIST	= bootstrap doc/manual.pdf doc/manual.ps ChangeLog

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = etc src routines test

ChangeLog:
	if test -d ".git"; then \
		git log --date-order --date=short | \
		sed -e '/^commit.*$$/d' | \
		awk '/^Author/ {sub(/\\$$/,""); getline t; print $$0 t; next}; 1'| \
		sed -e 's/^Author: //g' | \
		sed -e 's/>Date:   \([0-9]*-[0-9]*-[0-9]*\)/>\t\1/g' | \
		sed -e 's/^\(.*\) \(\)\t\(.*\)/\3    \1    \2/g' > ChangeLog; \
	else \
		echo "No git repository present."; \
	fi


MAINTAINERCLEANFILES	= Makefile.in aclocal.m4 configure config-h.in \
			stamp-h.in $(AUX_DIST)

#create the 'var' localstate directory for models and loadhistory
localstatepmmdir = $(localstatedir)/pmm
localstatepmm_DATA =

#dist-hook:
#	(cd $(distdir) && mkdir $(auxdir))
#	for file in $(AUX_DIST) $(AUX_DIST_EXTRA); do \
#		cp $$file $(distdir)/$$file; \
#	done
