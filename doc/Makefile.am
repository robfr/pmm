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
SUBDIRS = gbbe
## Include this file into Makefile.am that is used to generate documentation
##
## # $XORP: xorp/docs/mk/Makefile.doc.am,v 1.3 2005/01/19 19:23:19 pavlin Exp $
##

include $(top_srcdir)/doc/mk/Makefile.doc.am

DOCS = manual

manual_LATEX = manual.tex

#all: pdf
html:
	latex2html manual.tex

clean-local:
	-rm -rf manual
