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
#ifndef PMM_CFGPARSER_H_
#define PMM_CFGPARSER_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <libxml/xmlwriter.h>
#include "pmm_data.h"

int parse_config(struct pmm_config *cfg);
int parse_history(struct pmm_loadhistory *h);
int parse_model(struct pmm_model *m);
int parse_models(struct pmm_config *c);

int write_loadhistory(struct pmm_loadhistory *h);
int write_loadhistory_xtwp(xmlTextWriterPtr writer, struct pmm_loadhistory *h);
int write_models(struct pmm_config *cfg);
int write_model(struct pmm_model *m);
int write_model_xtwp(xmlTextWriterPtr writer, struct pmm_model *m);
int write_bench_list_xtwp(xmlTextWriterPtr writer,
		                  struct pmm_bench_list *bench_list);
int write_benchmark_xtwp(xmlTextWriterPtr writer, struct pmm_benchmark *b);
int
write_paramdef_array_xtwp(xmlTextWriterPtr writer,
                          struct pmm_paramdef *pd_array,
                          int n);
int
write_paramdef_xtwp(xmlTextWriterPtr writer, struct pmm_paramdef *pd);

int
write_parameter_array_xtwp(xmlTextWriterPtr writer, long long int *p, int n);
int write_interval_xtwp(xmlTextWriterPtr writer, struct pmm_interval *i);
int write_timeval_xtwp(xmlTextWriterPtr writer, struct timeval *t);

void xmlparser_init();
void xmlparser_cleanup();

#endif /*PMM_CFGPARSER_H_*/
