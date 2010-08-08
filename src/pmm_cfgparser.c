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
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

//#include "pmm_data.h"
#include "pmm_log.h"

#include "pmm_cfgparser.h"
#include "pmm_muparse.h"

/******************************************************************************
 *
 */

struct pmm_loadhistory* parse_loadconfig(xmlDocPtr, xmlNodePtr node);

int sync_parent_dir(char *file_path);

int
parse_paramdef_set(struct pmm_paramdef_set *pd_set, xmlDocPtr doc,
                   xmlNodePtr node);
int
parse_paramdef(struct pmm_paramdef *pd_array, int n_p, xmlDocPtr doc,
               xmlNodePtr node);
int
parse_param_constraint(struct pmm_paramdef_set *pd_set, xmlDocPtr doc,
                       xmlNodePtr node);

int
parse_bench_list(struct pmm_model *m, xmlDocPtr doc, xmlNodePtr node);
int
parse_routine_construction(struct pmm_routine *r, xmlDocPtr doc,
		                       xmlNodePtr node);


/*
 * TODO Finish this code
 *
 * Convert hours and minutes to a time structure
 *
struct tm hrmm_to_tm(char *key)
	struct tm time;


}*/

/*
 * TODO finish this routine, decide on how/if to support a crontab like
 * period, with week day support and other things
 *
void parse_routine_periodic(node, doc, struct pmm_routine *r) {

	while(node != NULL) {
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		if(!xmlStrcmp(cnode->name (const xmlChar *) "starttime")) {
			r->between_start = hrmm_to_tm(key);
		}
		else if(!xmlStrcmp(cnode->name (const xmlChar *) "endtime")) {
			r->between_end = hrmm_to_tm(key);
		}

		free(key);
        key = NULL;

		node=node->next;
	}
}*/

/*!
 * Parse a routine from an xml document
 *
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the node describing the routine
 *
 * @returns pointer to a routine structure containing the parsed data, or NULL
 *          on failure
 */
struct pmm_routine*
parse_routine(xmlDocPtr doc, xmlNodePtr node)
{

	char *key;
	struct pmm_routine *r;
	xmlNodePtr cnode;

	r = new_routine();
    if(r == NULL) {
        ERRPRINTF("Error creating new routine.\n");
        return NULL;
    }

	// get the children of the routine node
	cnode = node->xmlChildrenNode;

	// iterate through the children of the routine node
	while(cnode != NULL) {

		// get the value associated with the each node
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "name")) {
			if(!set_str(&(r->name), key)) {
				ERRPRINTF("set_str failed setting name\n");
                return NULL; //TODO possibly free allocated routine before
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "exe_path")) {
			if(!set_str(&(r->exe_path), key)) {
				ERRPRINTF("set_str failed setting exe_path\n");
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "exe_args")) {
			if(!set_str(&(r->exe_args), key)) {
				ERRPRINTF("set_str failed setting exe_args\n");
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "model_path")) {
			if(!set_str(&(r->model->model_path), key)) {
				ERRPRINTF("set_str failed failed setting model_path\n");
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "parameters")) {
			if(parse_paramdef_set(r->pd_set, doc, cnode) < 0)
            {
                ERRPRINTF("Error parsing parameter definitions.\n");
                return NULL;
            }
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "condition")) {
			if(strcmp("now", key) == 0) {
				r->condition = PMM_NOW;
			}
			/* TODO change PMM_BEFORE to PMM_UNTIL and add code to support
			else if(strcmp("before", key) == 0) {
				r->condition = PMM_BEFORE;
			} */
			else if(strcmp("idle", key) == 0) {
				r->condition = PMM_IDLE;
			}
			else if(strcmp("nousers", key) == 0) {
				r->condition = PMM_NOUSERS;
			}
			/* TODO  change PMM_BETWEEN to PMM_PERIODIC and add code to support
			else if(strcmp("periodic", key) == 0) {
				r->condition = PMM_PERIODIC;

				parse_routine_periodic(cnode->xmlChildren, doc, &r);

			}
			 */
			else {
				ERRPRINTF("Configuration error, routine:%s, condition:%s\n",
						r->name,
						key);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "priority")) {
			r->priority = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "construction")) {
			if(parse_routine_construction(r, doc, cnode) < 0) {
                ERRPRINTF("Error parsing construction method definition.\n");
                return NULL;
            }
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//ERRPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode=cnode->next;
	}

	if(!check_routine(r)) {
		ERRPRINTF("Routine not parsed correctly.\n");
        return NULL;
	}

	return r;
}

/*!
 * Parse multiple xml parameter definitions into a paramdef array
 *
 * @param   pd_set      pointer to parameter definition set structure
 * @param   doc         pointer to the xml document to parse
 * @param   node        pointer to the node of the xml tree describing the
 *                      parameter definitions
 *
 * @return 0 on success, -1 on failure
 */
int
parse_paramdef_set(struct pmm_paramdef_set *pd_set, xmlDocPtr doc,
                   xmlNodePtr node)
{
	char *key;
	xmlNodePtr cnode;
	int i;
    double d;

	cnode = node->xmlChildrenNode;

	key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

	// if the name of the cnode is ... do something with the key
	if(!xmlStrcmp(cnode->name, (const xmlChar *) "n_p")) {
		pd_set->n_p = atoi((char *)key);

		if(pd_set->n_p <= 0) {
			ERRPRINTF("Number of parameters must be greater than 0.\n");
			return -1;
		}

		pd_set->pd_array = malloc(pd_set->n_p * sizeof *(pd_set->pd_array));

		if(pd_set->pd_array == NULL) {
			ERRPRINTF("Error allocating memory.\n");
			return -1;
		}
	}
	else {
		ERRPRINTF("First element of parameter_array xml must be size (n_p)\n.");
		return -1;
	}

	free(key);
    key = NULL;

	cnode = cnode->next;


	// from this point we can assume that r->paramdef_array has been allocated


	// iterate through the children of the routine node
    i = 0;
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
        if(!xmlStrcmp(cnode->name, (const xmlChar *) "param"))
        {

			if(parse_paramdef(pd_set->pd_array, pd_set->n_p, doc, cnode) < 0) {
                ERRPRINTF("Error parsring parameter defintion.\n");
                return -1;
            }
            i++;

		}
        else if(!xmlStrcmp(cnode->name, (const xmlChar *) "param_constraint"))
        {
            if(parse_param_constraint(pd_set, doc, cnode) < 0) {
                ERRPRINTF("Error parsing parameter contraint.\n");
                return -1;
            }
        }
		else
        {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}


		free(key);
        key = NULL;

		cnode=cnode->next;
	}

    if(i != pd_set->n_p) {
        ERRPRINTF("Parsed unexpected number of param_defs. (%d of %d).\n",
                  i, pd_set->n_p);
        return -1;
    }

#ifdef HAVE_MUPARSER
    // if the pc_formula is set, construct the muParser for it
    if(pd_set->pc_formula != NULL) {
        create_param_constraint_muparser(pd_set);

        if(evaluate_constraint(pd_set->pc_parser, &d) == -1) {
            ERRPRINTF("Error setting up param constraint formula parser.\n");
            return -1;
        }
    }
#endif



	return 0; //success

}

/*!
 * parse an xml parameter constraint defintiion into the paramdef set structure
 *
 * @param   pd_set      pointer to the paramdef set structure
 * @param   doc         pointer to the xml document
 * @param   node        pointer to the node of the xml document describing the
 *                      parameter constraints
 *
 * @return 0 on success, -1 on failure
 */
int
parse_param_constraint(struct pmm_paramdef_set *pd_set, xmlDocPtr doc,
                       xmlNodePtr node)
{
	char *key;
	xmlNodePtr cnode;

	// get the children of the parameters node
	cnode = node->xmlChildrenNode;


	// iterate through the children of the routine node
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "formula")) {
			if(!set_str(&(pd_set->pc_formula), key)) {
				ERRPRINTF("set_str failed setting name\n");
                return -1;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "max")) {
			pd_set->pc_max = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "min")) {
			pd_set->pc_min = atoi((char *)key);
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode=cnode->next;
	}

	return 0; //success
}
/*!
 * parse an xml parameter definition into the paramdef array of a routine
 *
 * @param   pd_array    pointer to the parameter definition array
 * @param   n_p         number of parameter defintions
 * @param   doc         pointer to the xml document
 * @param   node        pointer to node of xml document describing parameter def
 *
 * @return 0 on success, -1 on failure
 */
int
parse_paramdef(struct pmm_paramdef *pd_array, int n_p, xmlDocPtr doc,
               xmlNodePtr node)
{
	char *key;
	xmlNodePtr cnode;
	struct pmm_paramdef p;

	// get the children of the parameters node
	cnode = node->xmlChildrenNode;

	p.order = -1; // check this after parsing xml
    p.nonzero_end = 0; // default is that end is zero speed
    p.stride = 1; //default stride is 1
    p.offset = 0; //default offset is 0

	// iterate through the children of the routine node
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "order")) {
			p.order = atoi((char *)key);

	    }
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "name")) {
			if(!set_str(&(p.name), key)) {
				ERRPRINTF("set_str failed setting name\n");
                return -1;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "max") ||
                !xmlStrcmp(cnode->name, (const xmlChar *) "end")) {
			p.end = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "min") ||
                !xmlStrcmp(cnode->name, (const xmlChar *) "start")) {
			p.start = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "stride")) {
			p.stride = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "offset")) {
			p.offset = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "fuzzy_max") ||
                !xmlStrcmp(cnode->name, (const xmlChar *) "nonzero_end")) {
			if(strcmp("yes", key) == 0 ||
               strcmp("true", key) == 0 ||
               strcmp("1", key) == 0)
            {
                p.nonzero_end = 1;
			}
            else if(strcmp("false", key) == 0 ||
                    strcmp("no", key) == 0 ||
                    strcmp("0", key) == 0)
            {
                p.nonzero_end = 0;

            }
            else {
                ERRPRINTF("Error parsing nonzero_end value : %s\n", key);
                return -1;
            }
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode=cnode->next;
	}

	if(p.order < 0 || p.order > n_p) {
		ERRPRINTF("paramdef has an order that is out of bounds order:%d.\n",
		           p.order);
        free(p.name);
        p.name = NULL;

        return -1;
	}

	//copy p into the order-th element of r's paramdef_array
	if(copy_paramdef(&(pd_array[p.order]), &p) < 0) {
        ERRPRINTF("Error copying parsed paramdef into parameter array.\n");
        free(p.name);
        p.name = NULL;

        return -1;
    }

	free(p.name);
    p.name = NULL;

	return 0; //success

}

/*!
 * Parse routine construction information from xml specification.
 *
 * @param   r       pointer to the corresponding routine
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the construction information node in the xml doc
 *
 * @return 0 on success, -1 on failure
 */
int
parse_routine_construction(struct pmm_routine *r, xmlDocPtr doc,
		                       xmlNodePtr node)
{
	char *key;
	xmlNodePtr cnode;

	// get the children of the routine node
	cnode = node->xmlChildrenNode;

	// iterate through the children of the routine node
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "method")) {
			if(!xmlStrcmp((const xmlChar *) "naive", (xmlChar *) key))
            {
				r->construction_method = CM_NAIVE;
			}
            else if(!xmlStrcmp((const xmlChar *)"naive_bisect", (xmlChar *)key))
            {
                r->construction_method = CM_NAIVE_BISECT;
            }
			else if(!xmlStrcmp((const xmlChar *) "gbbp", (xmlChar *) key))
            {
				r->construction_method = CM_GBBP;
			}
			else if(!xmlStrcmp((const xmlChar *) "gbbp_naive", (xmlChar *) key))
            {
				r->construction_method = CM_GBBP_NAIVE;
			}
			else if(!xmlStrcmp((const xmlChar *) "rand", (xmlChar *) key))
            {
				r->construction_method = CM_RAND;
			}
			else
            {
				LOGPRINTF("construction method unrecognised: %s\n", key);
				r->construction_method = CM_NAIVE;
			}
		}
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "min_sample_num")) {
			r->min_sample_num = atoi(key);
		}
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "min_sample_time")) {
			r->min_sample_time = atoi(key);
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode=cnode->next;
	}

	return 0; //success

}

/*!
 * Parse xml config file into config structure.
 *
 * @param cfg   pointer to config structure to store parsed data
 *
 * @return 0 on success, -1 on failure
 *
 * @pre cfg points to an allocated pmm_config structure
 * @pre cfg structure member 'configfile' must be set to the config file
 * path or this function will return an error
 */
int
parse_config(struct pmm_config *cfg) {

	xmlDocPtr doc;
	xmlNodePtr root, cnode;
    char *key;
	struct pmm_routine *r;
	int routine_count = 0;  // number of routines

	// parse the file into a doc tree
	doc = xmlReadFile(cfg->configfile, NULL, XML_PARSE_NOBLANKS);

	// check file was parsed
	if(doc == NULL) {
		ERRPRINTF("Config file: %s not parsed correctly\n", cfg->configfile);
        return -1; //failure
	}

	// get root node of doc tree
	root = xmlDocGetRootElement(doc);

	// check there is a root node (i.e. doc tree is not empty)
	if(root == NULL) {
		ERRPRINTF("Config file: %s empty\n", cfg->configfile);
		xmlFreeDoc(doc);
        return -1;
	}

	// check that the root node is a "config" type
	if(xmlStrcmp(root->name, (const xmlChar *) "config")) {
		ERRPRINTF("Config file is of wrong type. root node: %s\n", root->name);
		xmlFreeDoc(doc);
        return -1;
	}

	// get the child of the root node
	cnode = root->xmlChildrenNode;

	// iterate through the children of the root counting the number of
	// routines
	while(cnode != NULL) {

		if(!xmlStrcmp(cnode->name, (const xmlChar *) "main_sleep_period")) {
		    // get the value associated with the cnode
		    key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);
            double_to_timespec(atof(key), &(cfg->ts_main_sleep_period));
            free(key);
            key = NULL;
		}
		if(!xmlStrcmp(cnode->name,
                      (const xmlChar *) "model_write_time_threshold"))
        {
		    // get the value associated with the cnode
		    key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);
			cfg->time_spend_threshold = atoi(key);
            free(key);
            key = NULL;
		}
		if(!xmlStrcmp(cnode->name,
                      (const xmlChar *) "model_write_execs_threshold"))
        {
		    // get the value associated with the cnode
		    key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);
			cfg->num_execs_threshold = atoi(key);
            free(key);
            key = NULL;
		}
		// if we get a "load_monitor" cnode parse the load monitor config
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "load_monitor")) {
			cfg->loadhistory = parse_loadconfig(doc, cnode);

            if(cfg->loadhistory == NULL) {
                ERRPRINTF("Error parsing load history configuration.\n");
                return -1;
            }
		}
		// if we get a "routine" cnode parse the routine config
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "routine")) {
			routine_count++;

			r = parse_routine(doc, cnode);
            if(r == NULL) {
                ERRPRINTF("Error parsing routine.\n");
                return -1;
            }

			if(add_routine(cfg, r) < 0) {
                ERRPRINTF("Error adding routine to configuration.\n");
                return -1;
            }
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s\n", cnode->name);
		}

		cnode=cnode->next;
	}


	xmlFreeDoc(doc);

	return 0;
}


/*!
 * Parse load history configuration information from an xml document.
 * 
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the node describing the load history config
 *
 * @return pointer to loadhistory structure or NULL on failure
 *
 * TODO FIX naming
 */
struct pmm_loadhistory*
parse_loadconfig(xmlDocPtr doc, xmlNodePtr node)
{

	char *key;
	struct pmm_loadhistory *h;
	xmlNodePtr cnode;

	h = new_loadhistory();
    if(h == NULL) {
        ERRPRINTF("Error creating new loadhistory structure.\n");
        return NULL;
    }

	// get the children of the loadconfig node
	cnode = node->xmlChildrenNode;

	// iterate through the children of the loadconfig node
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "load_path")) {
			if(!set_str(&(h->load_path), key)) {
				ERRPRINTF("set_str failed setting load_path\n");
                return NULL; //TODO free load history before returning
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "write_period")) {
			h->write_period = atoi(key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "history_size")) {
			if(init_loadhistory(h, atoi(key)) < 0) {
                ERRPRINTF("Error initializing load history.\n");
                return NULL;
            }
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode=cnode->next;
	}

	if(!check_loadhistory(h)) {
		ERRPRINTF("load history config not parsed correctly.\n");
        return NULL;
	}

	return h;
}

/*!
 * Parse xml description of a timeval into  a newly allocated timeval structure
 *
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the node describing the timeval
 *
 * @return pointer to a timeval structure or NULL on failure
 */
struct timeval* parse_timeval(xmlDocPtr doc, xmlNodePtr node) {
	char *key;
	struct timeval *t;
	xmlNodePtr cnode;

	t = malloc(sizeof *t);
	if(t == NULL) {
		ERRPRINTF("Error allocating timeval structure");
        return NULL;
	}

	cnode = node->xmlChildrenNode;

	while(cnode != NULL) {
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "secs")) {
			t->tv_sec = atoll((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "usecs")) {
			t->tv_usec = atoll((char *)key);
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;
        
		cnode = cnode->next;
	}


	return t;
}

/*!
 * Parse a timeval from an xml doc.
 *
 * @param   t       pointer to the timeval structure to store to
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the timeval node in the xml doc.
 *
 * @return 0 on success, or -1 on failure
 */
int
parse_timeval_p(struct timeval *t, xmlDocPtr doc, xmlNodePtr node)
{
	char *key;
	xmlNodePtr cnode;

	cnode = node->xmlChildrenNode;

	while(cnode != NULL) {
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// LOGPRINTF("%s : %s\n", cnode->name, key);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "secs")) {
			t->tv_sec = atoll((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "usecs")) {
			t->tv_usec = atoll((char *)key);
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode = cnode->next;
	}

	//check_timeval(t); //TODO

	return 0; //success
}

/*!
 * Parse an xml parameter array into a parameter array and size integer
 *
 * @param   p       pointer to the parameter array pointer
 * @param   n_p     pointer to the integer to store the size in
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the xml node that describes the param array
 *
 * @return 0 on succes, -1 on failure
 */
int parse_parameter_array_p(int **p, int *n_p, xmlDocPtr doc, xmlNodePtr node)
{
	char *key;
	xmlNodePtr cnode;
	int i;

	cnode = node->xmlChildrenNode;

	key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

	// if the name of the cnode is ... do something with the key
	if(!xmlStrcmp(cnode->name, (const xmlChar *) "n_p")) {
		*n_p = atoi((char *)key);

		if(*n_p <= 0) {
			ERRPRINTF("Number of parameters must be greater than 0.\n");
			return -1;
		}

		*p = malloc(*n_p * sizeof *(*p));

		if(*p == NULL) {
			ERRPRINTF("Error allocating memory.\n");
			return -1;
		}
	}
	else {
		ERRPRINTF("First element of parameter_array xml must be size (n_p)"
                  "got: %s\n", cnode->name);
		return -1;
	}

	free(key);
    key = NULL;

	cnode = cnode->next;


	// from this point we can assume that b->p has been allocated

	i=0;
	while(cnode != NULL) {

		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		if(!xmlStrcmp(cnode->name, (const xmlChar *) "parameter")) {
            if(i < *n_p) {
			    (*p)[i++] = atoll((char *)key);
            }
            else {
                ERRPRINTF("Parsed more parameters than expected.\n");
                free(key);
                key = NULL;

                return -1;
            }
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode = cnode->next;
	}

    if(i != *n_p) {
        ERRPRINTF("Parsed unexpected number of parameters. (%d of %d).\n",
                  i, *n_p);
        return -1;
    }


	return 0; //success
}

/*!
 * Parse a benchmark from xml document.
 *
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the node describing the benchmark
 *
 * @return pointer to a newly allocated benchmark representing the parsed
 * information or NULL on error
 */
struct pmm_benchmark* parse_benchmark(xmlDocPtr doc, xmlNodePtr node) {
	char *key;
	struct pmm_benchmark *b;
	xmlNodePtr cnode;

	b = new_benchmark();
    if(b == NULL) {
        ERRPRINTF("Error allocating new benchmark.\n");
        return NULL;
    }

	// get the children of the benchmark node
	cnode = node->xmlChildrenNode;

	// iterate through the children of the routine node
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		//LOGPRINTF("%s : %s\n", cnode->name, key);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "parameter_array")) {
			if(parse_parameter_array_p(&(b->p), &(b->n_p), doc, cnode) < 0) {
				ERRPRINTF("Error parsing parameter_array.\n");
                free_benchmark(&b);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "complexity")) {
			if(sscanf((char *)key, "%lld", &(b->complexity)) != 1) {
				ERRPRINTF("Error parsing complexity.\n");
                free_benchmark(&b);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "flops")) {
			if(sscanf((char *)key, "%lf", &(b->flops)) != 1) {
				ERRPRINTF("Error parsing flops.\n");
                free_benchmark(&b);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "seconds")) {
			if(sscanf((char *)key, "%lf", &(b->seconds)) != 1) {
				ERRPRINTF("Error parsing seconds.\n");
                free_benchmark(&b);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "used_time")) {
			if(parse_timeval_p(&(b->used_t), doc, cnode) < 0) {
				ERRPRINTF("Error parsing used_time timeval.\n");
                free_benchmark(&b);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "wall_time")) {
			if(parse_timeval_p(&(b->wall_t), doc, cnode) < 0) {
				ERRPRINTF("Error parsing wall_time timeval.\n");
                free_benchmark(&b);
                return NULL;
			}
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode=cnode->next;
	}

	/* TODO Check benchmark structure is parsed correctly
		if(!check_benchmark(b)) {
			ERRPRINTF("Benchmark parsed incorrectly.\n");
			free(key);
            key = NULL;

			exit(EXIT_FAILURE);
		}
	 */

	return b;
}

/*!
 * Parse an interval from an xml document.
 *
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the node describing the interval
 *
 * @return newly allocated interval or NULL on failure.
 */
struct pmm_interval* parse_interval(xmlDocPtr doc, xmlNodePtr node) {
	char *key;
	struct pmm_interval *i;
	xmlNodePtr cnode;

	i = new_interval();
    if(i == NULL) {
        ERRPRINTF("Error creating new interval.\n");
        return NULL;
    }

	// get the children of the benchmark node
	cnode = node->xmlChildrenNode;

	// iterate through the children of the routine node
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "plane")) {
			i->plane = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "climb_step")) {
			i->climb_step = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "start")) {
            //TODO fix this to check parameter_array node
            //(passing xmlChildrenNode skips over this node entirely)
			if(parse_parameter_array_p(&(i->start), &(i->n_p), doc,
                                       cnode->xmlChildrenNode) < 0)
            {
				ERRPRINTF("Error parsing parameter_array.\n");
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "end")) {
            //TODO fix this to check parameter_array node
            //(passing xmlChildrenNode skips over this node entirely)
			if(parse_parameter_array_p(&(i->end), &(i->n_p), doc,
                                       cnode->xmlChildrenNode) < 0)
            {
				ERRPRINTF("Error parsing parameter_array.\n");
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "type")) {
			if(!xmlStrcmp((const xmlChar *) "gbbp_empty", (xmlChar *) key)) {
				i->type = IT_GBBP_EMPTY;
			}
			else if(!xmlStrcmp((const xmlChar *) "gbbp_climb", (xmlChar *) key)) {
				i->type = IT_GBBP_CLIMB;
			}
			else if(!xmlStrcmp((const xmlChar *) "gbbp_bisect", (xmlChar *) key)) {
				i->type = IT_GBBP_BISECT;
			}
			else if(!xmlStrcmp((const xmlChar *) "gbbp_inflect", (xmlChar *) key)) {
				i->type = IT_GBBP_INFLECT;
			}
			else if(!xmlStrcmp((const xmlChar *) "point", (xmlChar *) key)) {
				i->type = IT_POINT;
			}
			else if(!xmlStrcmp((const xmlChar *) "boundary_complete", (xmlChar *) key)) {
				i->type = IT_BOUNDARY_COMPLETE;
			}
			else if(!xmlStrcmp((const xmlChar *) "complete", (xmlChar *) key)) {
				i->type = IT_COMPLETE;
			}
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode=cnode->next;
	}

	/* TODO Check structure is parsed correctly
		if(!check_load(r)) {
			ERRPRINTF("Load parsing error.\n");
			free(key);
            key = NULL;

			exit(EXIT_FAILURE);
		}
	 */

	return i;
}


/*!
 * Parse a load record from an xml document.
 *
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the node describing the load
 *
 * @return pointer to a newly allocated load record or NULL on failure
 */
struct pmm_load* parse_load(xmlDocPtr doc, xmlNodePtr node) {

	char *key;
	struct pmm_load *l;
	xmlNodePtr cnode;

	l = new_load();
    if(l == NULL) {
        ERRPRINTF("Error creating new load structure.\n");
        return NULL;
    }

	// get the children of the routine node
	cnode = node->xmlChildrenNode;

	// iterate through the children of the routine node
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		//LOGPRINTF("%s : %s\n", cnode->name, key);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "time")) {
			if((l->time = parseISO8601Date((char *)key)) == (time_t)-1) {
				ERRPRINTF("Error parsign time string.\n");
                free(key);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "one_min")) {
			if(sscanf((char *)key, "%lf", &(l->load[0])) != 1) {
				ERRPRINTF("Error parsing 1 minute load.\n");
                free(key);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "five_min")) {
			if(sscanf((char *)key, "%lf", &(l->load[1])) != 1) {
				ERRPRINTF("Error parsing 5 minute load.\n");
                free(key);
                return NULL;
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "fifteen_min")) {
			if(sscanf((char *)key, "%lf", &(l->load[2])) != 1) {
				ERRPRINTF("Error parsing 15 minute load.\n");
                free(key);
                return NULL;
			}

		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;
		cnode=cnode->next;
	}

	/* TODO Check Load structure is parsed correctly FREE if it is not!
	if(!check_load(r)) {
		ERRPRINTF("Load parsing error.\n");
		free(key);
        key = NULL;

		exit(EXIT_FAILURE);
	}
	 */

	return l;
}

/*!
 * Parse the load history file.
 *
 * @param   h   pointer to a load history structure, already populated with
 *              config data (i.e. the load history file path)
 *
 * @return 0 on success, -1 on failure.
 */
int
parse_history(struct pmm_loadhistory *h) {

	xmlDocPtr doc;
	xmlNodePtr root, cnode;
	struct pmm_load *l;

    int fd;
    struct flock fl;

    //open file descriptor on load history file
    fd = open(h->load_path, O_RDWR);
    if(fd == -1) {
        ERRPRINTF("Error opening load history file:%s\n", h->load_path);
        perror("open");

        if(errno == ENOENT) { //path does not exist
            return -1; //return code to init new history
        }
        else { //some other error
            return -2;
        }
    }
    
    //set up a read lock
    fl.l_type = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = 0;

    //get read lock on file
    if(fcntl(fd, F_SETLKW, &fl) == -1) {
        ERRPRINTF("Error getting read lock on file:%s\n", h->load_path);
        perror("fnctl");
        return -2; //failure
    }

	// parse the load in file to a doc tree
	doc = xmlReadFd(fd, h->load_path, NULL, XML_PARSE_NOBLANKS);
	if(doc == NULL) {
		ERRPRINTF("Load history file: %s not parsed correctly\n", h->load_path);

		xmlFreeDoc(doc);
        if(fd != -1) {
            close(fd);
        }

		return -2; // failure
	}

    //close file and free lock
    if(close(fd) < 0) {
        ERRPRINTF("Error closing load history file:%s.\n", h->load_path);
        perror("close");

        return -2; // failure
    }

	// get root node of doc tree
	root = xmlDocGetRootElement(doc);

	// check there is a root node (i.e. doc tree is not empty)
	if(root == NULL) {
		ERRPRINTF("Load history file: %s empty\n", h->load_path);
        
		xmlFreeDoc(doc);
        return -1; //return code to init new history
	}

	// check that the root node is a "loadhistory" type
	if(xmlStrcmp(root->name, (const xmlChar *) "loadhistory")) {
		ERRPRINTF("Load history file is of wrong type. root node: %s\n",
                  root->name);

		xmlFreeDoc(doc);
        return -2; //failure
	}

	// get the child of the root node
	cnode = root->xmlChildrenNode;

	// iterate through the children of the root adding loads to the load history
	// structure
	while(cnode != NULL) {

		// LOGPRINTF("%s\n", cnode->name);

		// if we get a load cnode parse it and add to the load history struct
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "load")) {

			l = parse_load(doc, cnode);
            if(l == NULL) {
                ERRPRINTF("Error parsing load.\n");

                xmlFreeDoc(doc);
                return -1; //failure
            }

			add_load(h, l);

			// add load actually copys the load object into the history so we
			// may free it at this point
			free(l);
            l = NULL;
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s\n", cnode->name);
		}

		cnode=cnode->next;
	}

	xmlFreeDoc(doc);
	return 0; //success
}

/*!
 * Parse a bench list from an xml document into a model.
 *
 * @param   m       pointer to the model
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the node describing the bench list
 *
 * @return 0 on success, -1 on failure
 */
int
parse_bench_list(struct pmm_model *m, xmlDocPtr doc, xmlNodePtr node)
{
    int ret;
	char *key;
	struct pmm_benchmark *b;
	xmlNodePtr cnode;

    int size;


	cnode = node->xmlChildrenNode;

	key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

	// if the name of the cnode is ... do something with the key
	// first node must be the size fail if otherwise
	// TODO parsing "size" from the xml first is not really required
	if(!xmlStrcmp(cnode->name, (const xmlChar *) "size")) {

		size = atoi((char *)key);

		m->bench_list = new_bench_list(m, m->n_p);

	}
	else {
		ERRPRINTF("First element of bench_list xml must be size "
		          "got: %s\n.", cnode->name);
		return -1;
	}

	free(key);
    key = NULL;

	cnode = cnode->next;

	// iterate through the rest of the children of the bench_list node
	while(cnode != NULL) {

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "benchmark")) {

			b = parse_benchmark(doc, cnode);

            if(b == NULL) {
                ERRPRINTF("Error parsing benchmark.\n");
                return -1;
            }

			ret = insert_bench_into_list(m->bench_list, b);
            if(ret < 0) {
                free_benchmark(&b);
                ERRPRINTF("Error inserting bench into bench list.\n");
                return -1;
            }
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s\n", cnode->name);
		}

		cnode=cnode->next;
	}

    if(m->bench_list->size != size) {
		ERRPRINTF("%d benchmarks parsed, expected %d\n", m->bench_list->size,
                                                         size);
        return -1;
	}

	return 0; //success
}

/*!
 * Parse an interval list from an xml document into a model.
 *
 * @param   m       pointer to the model
 * @param   doc     pointer to the xml document
 * @param   node    pointer to the node beginning the description of the
 *                  interval list
 *
 * @return 0 on success, -1 on failure
 */
int parse_interval_list(struct pmm_model *m, xmlDocPtr doc, xmlNodePtr node)
{

	struct pmm_interval *i;
	xmlNodePtr cnode;

	// get the children of the routine node
	cnode = node->xmlChildrenNode;

	// iterate through the children of the routine node
	while(cnode != NULL) {

		// if the name of the cnode is ... do something
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "interval")) {
			i = parse_interval(doc, cnode);
            if(i == NULL) {
                ERRPRINTF("Error parsing interval.\n");
                return -1;
            }

            //to preserve the order of the list as it is read from disk
            //new intervals are added to the end of the interval_list structure
			add_bottom_interval(m->interval_list, i);
		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s\n", cnode->name);
		}

		cnode=cnode->next;
	}

	return 0; //success
}

/*!
 * Parses a model from an xml file. The model structure is expected to have
 * the path of the file already stored.
 *
 * @param   m   pointer to the model
 *
 * @return 0 on success, -1 if no model file exists, -2 on other error
 */
int parse_model(struct pmm_model *m)
{

	xmlDocPtr doc;
	xmlNodePtr root, cnode;
	char *key;

    int fd;
    struct flock fl;

	int completion;

    struct pmm_paramdef_set *pd_set;

    pd_set = new_paramdef_set();

    if(m->bench_list != NULL) {
        ERRPRINTF("Error, attempting to parse model file into non empty model:"
                  " %s completion:%d\n", m->model_path, m->completion);
        return -2;
    }

    //open file descriptor on model file
    fd = open(m->model_path, O_RDWR);
    if(fd == -1) {
        ERRPRINTF("Error opening model file:%s\n", m->model_path);
        perror("open");

        if(errno == ENOENT) { //file does not exist
            return -1; //return code to init new model
        }
        else { //some other error
            return -2;
        }
    }

    //set up a read lock
    fl.l_type = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = 0;

    //get read lock on file
    if(fcntl(fd, F_SETLKW, &fl) == -1) {
        ERRPRINTF("Error getting read lock on file:%s\n", m->model_path);
        perror("fnctl");
        return -2;
    }

	// parse the model in file to a doc tree
	doc = xmlReadFd(fd, m->model_path, NULL, XML_PARSE_NOBLANKS);
	if(doc == NULL) {
		ERRPRINTF("Model file: %s not parsed correctly continuing with new "
		          "model\n", m->model_path);

		xmlFreeDoc(doc);

        if(fd == -1) {
            close(fd);
        }

		return -1; // file not read, return code to init new model
	}

    //close file and free lock
    if(close(fd) < 0) {
        ERRPRINTF("Error closing model file:%s.\n", m->model_path);
        perror("close");

        return -2; // failure
    }

	// get root node of doc tree
	root = xmlDocGetRootElement(doc);

	// check there is a root node (i.e. doc tree is not empty)
	if(root == NULL) {
		ERRPRINTF("Model file: %s empty\n", m->model_path);
		xmlFreeDoc(doc);
		return -1; // file empty, return code to init new model
	}

	// check that the root node is a "model" type
	if(xmlStrcmp(root->name, (const xmlChar *) "model")) {
		ERRPRINTF("Model xml has wrong type. root node: %s\n", root->name);
		xmlFreeDoc(doc);
        return -2; // model file corrupt, return failure;
		
	}

	// get the child of the root node
	cnode = root->xmlChildrenNode;

	// iterate through the children of the root adding data to the model
	while(cnode != NULL) {

		// get the value associated with the each cnode
		key = (char *)xmlNodeListGetString(doc, cnode->xmlChildrenNode, 1);

		// if the name of the cnode is ... do something with the key
		if(!xmlStrcmp(cnode->name, (const xmlChar *) "n_p")) {
			m->n_p = atoi((char *)key);

			if(m->parent_routine != NULL &&
               m->n_p != m->parent_routine->pd_set->n_p)
            {
				ERRPRINTF("model / routine parameter mismatch m:%d r:%d.\n",
				          m->n_p, m->parent_routine->pd_set->n_p);
		        xmlFreeDoc(doc);
                return -2; //failure
			}
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "completion")) {
			// store the completion variable so we can check later
			// that the number of parsed benchmarks matches
			completion = atoi((char *)key);
		}
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "complete")) {
			m->complete = atoi((char *)key);
		}
        else if(!xmlStrcmp(cnode->name, (const xmlChar *) "parameters")) {


            if(parse_paramdef_set(pd_set, doc, cnode) < 0)
            {
                ERRPRINTF("Error parsing parameter definitions.\n");
		        xmlFreeDoc(doc);
                return -2; //failure
            }


            if(pd_set->n_p != m->n_p) {
                ERRPRINTF("Parameter definition and model mismatch.\n");
            }

            // if we have a parent routine, check that the param definitions
            // in the model match the routine
            if(m->parent_routine != NULL) {
                if(isequal_paramdef_set(m->parent_routine->pd_set, pd_set) != 0)
                {
                    ERRPRINTF("Current parameter definitions do not match "
                              "those initially used to build model.\n");

                    ERRPRINTF("model:\n");
                    print_paramdef_set(PMM_ERR, pd_set);

                    ERRPRINTF("routine:\n");
                    print_paramdef_set(PMM_ERR, m->parent_routine->pd_set);

                    free_paramdef_set(&pd_set);

                    return -2; // failure
                }
            }

            // finished with the parsed model parameter definitions now
            free_paramdef_set(&pd_set);

        }
		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "bench_list")) {
			if(parse_bench_list(m, doc, cnode) < 0) {
                ERRPRINTF("Error parsing bench list.\n");
		        xmlFreeDoc(doc);
                return -2; //failure
            }
		}

		else if(!xmlStrcmp(cnode->name, (const xmlChar *) "interval_list")) {

			if(parse_interval_list(m, doc, cnode) < 0) {
                ERRPRINTF("Error parsing interval list.\n");
		        xmlFreeDoc(doc);
                return -2; //failure
            }

		}
		else {
			// probably a text : null tag
			// TODO suppress these and check everywhere else
			//LOGPRINTF("unexpected tag: %s / %s\n", cnode->name, (char *)key);
		}

		free(key);
        key = NULL;

		cnode=cnode->next;
	}

	xmlFreeDoc(doc);


	if(m->completion != completion) {
		ERRPRINTF("Model completion mismatch, model: %s, %d benchmarks parsed, %d expected\n",
				m->model_path, m->completion, completion);
		return -2;
	}

	return 0; //success
}

/*!
 * Parse models of routines listed in the config structure.
 *
 * @param   c   pointer to the config with all routine details
 *
 * @return 0 on success, -1 on failure
 */
int parse_models(struct pmm_config *c)
{
	int i, rc;
	struct pmm_routine *r;
	struct pmm_model *m;

	for(i=0; i<c->used; i++) {
		r = c->routines[i];
		m = r->model;

		LOGPRINTF("Loading model: %s for routine: %s\n",
		          r->name, m->model_path);

        rc = parse_model(m);
        if(rc < 0) {
		    if(rc == -1) {
			    //model parsing failed, so initialize model with definitions
			    //from routine

			    m->n_p = r->pd_set->n_p;
			    if(init_bench_list(m, r->pd_set) < 0){
                    ERRPRINTF("Error initialising bench list.\n");
                    return -1; //failure
                }

		    }
            else {
                ERRPRINTF("Error parsing model.\n");
                return -1; //failure
            }
        }
	}

	return 0; //success
}

/*
 * writes history structure to disk in xml returning +ve on sucess and -ve on
 * failure
 */
int write_interval_xtwp(xmlTextWriterPtr writer, struct pmm_interval *i)
{
	int rc;


	// Start an element named interval
	rc = xmlTextWriterStartElement(writer, BAD_CAST "interval");
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartElement (interval)\n");
		return rc;
	}

    switch (i->type) {
        case IT_GBBP_EMPTY :
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "type", "%s",
                                                 "gbbp_empty");
            break;
        case IT_GBBP_CLIMB :
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "type", "%s",
                                                 "gbbp_climb");
            break;

        case IT_GBBP_BISECT :
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "type", "%s",
                                                 "gbbp_bisect");
            break;

        case IT_GBBP_INFLECT :
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "type", "%s",
                                                 "gbbp_inflect");
            break;

        case IT_BOUNDARY_COMPLETE :
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "type", "%s",
                                                 "boundary_complete");
            break;

        case IT_POINT :
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "type", "%s",
                                                 "point");
            break;

        case IT_COMPLETE :
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "type", "%s",
                                                 "complete");
            break;

        default:
            ERRPRINTF("Invalid interval type: %s (%d)\n",
                      interval_type_to_string(i->type), i->type);
            print_interval(PMM_ERR, i);
            return -1; // fail
    }

    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (type)\n");
        return rc;
    }


    switch (i->type) {
        case IT_GBBP_EMPTY :
        case IT_GBBP_CLIMB :
            // Add an element with name "climb_step" and value to interval.
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "climb_step",
                    "%d", i->climb_step);
            if (rc < 0) {
                ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (plane)\n");
                return rc;
            }
        case IT_GBBP_BISECT :
        case IT_GBBP_INFLECT :
            // Add an element with name "plane" and value to interval.
            rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "plane",
                    "%d", i->plane);
            if (rc < 0) {
                ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (plane)\n");
                return rc;
            }


            // Add an element with name "start" 
            rc = xmlTextWriterStartElement(writer, BAD_CAST "start");
            if (rc < 0) {
                ERRPRINTF("Error @ xmlTextWriterStartElement (start)\n");
                return rc;
            }
            // write the start parameter array
            rc = write_parameter_array_xtwp(writer, i->start, i->n_p);
            if (rc < 0) {
                ERRPRINTF("Error writing parameter array (start)\n");
                return rc;
            }
            // Close the start element
            rc = xmlTextWriterEndElement(writer);
            if (rc < 0) {
                ERRPRINTF("Error @ xmlTextWriterEndElement\n");
                return rc;
            }


            // Add an element with name "end" 
            rc = xmlTextWriterStartElement(writer, BAD_CAST "end");
            if (rc < 0) {
                ERRPRINTF("Error @ xmlTextWriterStartElement (end)\n");
                return rc;
            }
            // write the end parameter array
            rc = write_parameter_array_xtwp(writer, i->end, i->n_p);
            if (rc < 0) {
                ERRPRINTF("Error writing parameter array (end)\n");
                return rc;
            }
            // Close the end element
            rc = xmlTextWriterEndElement(writer);
            if (rc < 0) {
                ERRPRINTF("Error @ xmlTextWriterEndElement\n");
                return rc;
            }

            break;

        case IT_POINT :
            // Add an element with name "start" 
            rc = xmlTextWriterStartElement(writer, BAD_CAST "start");
            if (rc < 0) {
                ERRPRINTF("Error @ xmlTextWriterStartElement (start)\n");
                return rc;
            }
            // write the start parameter array
            rc = write_parameter_array_xtwp(writer, i->start, i->n_p);
            if (rc < 0) {
                ERRPRINTF("Error writing parameter array (start)\n");
                return rc;
            }
            // Close the start element
            rc = xmlTextWriterEndElement(writer);
            if (rc < 0) {
                ERRPRINTF("Error @ xmlTextWriterEndElement\n");
                return rc;
            }

            break;

        case IT_COMPLETE :
        case IT_BOUNDARY_COMPLETE :
            //nothing
            break;

        default :
            ERRPRINTF("Invalid interval type: %s (%d)\n",
                      interval_type_to_string(i->type), i->type);
            print_interval(PMM_ERR, i);
            return -1; //failure
    }




	// Close the interval element.
	rc = xmlTextWriterEndElement(writer);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterEndElement\n");
		return rc;
	}

	return 0; //success
}

int
write_loadhistory(struct pmm_loadhistory *h)
{
    char *temp_file;

	int rc;
	xmlTextWriterPtr writer;
    xmlOutputBufferPtr output_buffer;

    struct flock fl = {F_WRLCK, SEEK_SET, 0, 0, 0};
    int temp_fd, hist_fd;

    DBGPRINTF("writing file: %s\n", h->load_path);

    // create file name for mkstemp
    if(asprintf(&temp_file, "%s.XXXXXX", h->load_path)
       > PATH_MAX)
    {
        ERRPRINTF("filename too long: %s\n", temp_file);

        free(temp_file);
        temp_file = NULL;

        return -1; //fail
    }


    //open temp file
    temp_fd = mkstemp(temp_file);
    if(temp_fd == -1) {
        ERRPRINTF("Error opening temp file for writing: %s\n", temp_file);
        perror("mkstemp");
        return -1; //fail
    }


    //create output buffer
    output_buffer = xmlOutputBufferCreateFd(temp_fd, NULL);
    if(output_buffer == NULL) {
        ERRPRINTF("Error creating xml output buffer.\n");

        unlink(temp_file);
        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

        return -1; //fail
    }



    //create xml writer
	writer = xmlNewTextWriter(output_buffer);
	if (writer == NULL) {
		ERRPRINTF("Error creating the xml writer\n");

        unlink(temp_file);
        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

		return -1; //fail
	}

	rc = xmlTextWriterSetIndent(writer, 1);
	if(rc < 0) {
		ERRPRINTF("Error setting indent\n");

		xmlFreeTextWriter(writer);
        unlink(temp_file);
        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

		return -1;
	}

    //write history
	rc = write_loadhistory_xtwp(writer, h);
	if(rc < 0) {
		ERRPRINTF("Error writing history, partial history saved in: %s\n",
                   temp_file);

		xmlFreeTextWriter(writer);
        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

		return -1;
	}

	xmlFreeTextWriter(writer);

    if(fsync(temp_fd) < 0) {
        ERRPRINTF("Error syncing data for file, remove:%s manually.\n",
                  temp_file);
        perror("fsync");

        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

        return -1;

    }

    //close temp file
    if(close(temp_fd) < 0) {
        ERRPRINTF("Error closing file, remove:%s manually\n", temp_file);

        free(temp_file);
        temp_file = NULL;
        return -1;
    }

    //open historyl file
    hist_fd = open(h->load_path, O_WRONLY|O_CREAT, S_IRWXU);
    if(hist_fd < 0) {
        ERRPRINTF("Error opening load file: %s.\n", h->load_path);

        free(temp_file);
        temp_file = NULL;

        return -1;
    }

    //set up a write lock
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = 0;

    //get write lock on load history file so reading processes will block
    if(fcntl(hist_fd, F_SETLKW, &fl) == -1) {
        ERRPRINTF("Error aquiring lock on load history file: %s\n",
                  h->load_path);
        perror("fcntl");

        close(hist_fd);
        free(temp_file);
        temp_file = NULL;

        return -1;
    }

    //rename temp file to history file
    //TODO if we port to windows, this will not work
    if(rename(temp_file, h->load_path) < 0) {
        ERRPRINTF("Error renaming %s to %s, leaving %s on filesystem\n",
                  temp_file, h->load_path, temp_file);

        close(hist_fd);
        free(temp_file);
        temp_file = NULL;

        return -1;
    }

    free(temp_file);
    temp_file = NULL;

    //close and free lock
    if(close(hist_fd) < 0) {
        ERRPRINTF("Error closing file: %s\n.", h->load_path);

        return -1;
    }

    //sync dir so we are sure everything is committed to the filesystem
    if(sync_parent_dir(h->load_path) < 0) {
        ERRPRINTF("Error syncing parent dir of: %s.\n", h->load_path);

        return -1;
    }

	return 0; //success
}


#define TIME_STR_SIZE 256
/*
 * this is the 'internal' function that writes the various elements and
 * returns error codes to the main write_loadhistory function so it may close
 * the xmlTextWriterPtr gracefully
 */
int write_loadhistory_xtwp(xmlTextWriterPtr writer, struct pmm_loadhistory *h)
{
	int rc;
	int i;
	char time_str[TIME_STR_SIZE+1] = "";
	struct tm time_tm;

	// start document with standard version/enconding
	rc = xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartDocument\n");
		return rc;
	}

	// start root element loadhistory
	rc = xmlTextWriterStartElement(writer, BAD_CAST "loadhistory");
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartElement (loadhistory)\n");
		return rc;
	}

	i = h->start_i;
	while(i != h->end_i) {

		// Start an element named load as child of loadhistory.
		rc = xmlTextWriterStartElement(writer, BAD_CAST "load");
		if (rc < 0) {
			ERRPRINTF("Error @ xmlTextWriterStartElement (load)\n");
			return rc;
		}

		// convert time_t value to ISO 8601
		localtime_r(&(h->history[i].time), &time_tm);
		if(0 == strftime(time_str, TIME_STR_SIZE,
		                 "%Y-%m-%dT%H:%M:%S%z",
		                 &time_tm))
		{
			ERRPRINTF("Error converting time_t to ISO 8601 string.\n");
			return -1;
		}

		// Add an element with name "time" and value to load.
		rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "time",
				"%s", time_str);
		if (rc < 0) {
			ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (time)\n");
			return rc;
		}

		// Add a comment with the human readable version of the time value
		//rc = xmlTextWriterWriteFormatComment(writer, "%s",
		//		ctime((time_t*)&(h->history[i].time)));
		//if(rc < 0) {
		//	ERRPRINTF("Error @ xmlTextWriterFormatComment (time)\n");
		//	return rc;
		//}


		// Add an element with name one_min and value to load
		rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "one_min",
				"%f", h->history[i].load[0]);
		if (rc < 0) {
			ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (one_min)\n");
			return rc;
		}
		// Add an element with name five_min and value to load
		rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "five_min",
				"%f", h->history[i].load[1]);
		if (rc < 0) {
			ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (five_min)\n");
			return rc;
		}
		// Add an element with name fifteen_min and value to load
		rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "fifteen_min",
				"%f", h->history[i].load[2]);
		if (rc < 0) {
			ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (fifteen_min)\n");
			return rc;
		}

		// Close the load element.
		rc = xmlTextWriterEndElement(writer);
		if (rc < 0) {
			ERRPRINTF("Error @ xmlTextWriterEndElement\n");
			return rc;
		}

		i = (i + 1) % h->size_mod;

	}

	// Close the root element named loadhistory (and all other open tags).
	rc = xmlTextWriterEndDocument(writer);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterEndDocument\n");
		return rc;
	}

	return 0; //success
}

/*!
 * Check if a file descriptor is locked
 *
 * @param   fd  the file descriptor to check
 * @param   type    the type of lock to test FD_WRLCK, FD_RDLCK, FD_RWLCK
 *
 * @return -1 on error, 0 if file is not locked, pid of owning process if file
 * is locked
 */
int check_lock(int fd, int type)
{
    struct flock fl;

    fl.l_type = type;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = 0;

    if(fcntl(fd, F_GETLK, &fl) == -1) {
        ERRPRINTF("Error checking lock status on fd: %d\n", fd);
        perror("fcntl\n");
        return -1;
    }


    if(fl.l_type == F_UNLCK) {
        return 0;
    }
    else {
        return fl.l_pid;
    }
}

int
write_models(struct pmm_config *cfg)
{
    int i;
    int ret = 0;

    LOGPRINTF("Writing models to disk.\n");


    for(i=0; i<cfg->used; i++) {
        if(write_model(cfg->routines[i]->model) < 0) {
            ERRPRINTF("Error writing model for routine: %s.\n",
                      cfg->routines[i]->name);
            ret = -1;
        }
    }

    return ret;
}

int
write_model(struct pmm_model *m)
{
    char *temp_file;

	int rc;
	xmlTextWriterPtr writer;
    xmlOutputBufferPtr output_buffer;

    struct flock fl;
    int temp_fd, model_fd;

    DBGPRINTF("writing model file: %s\n", m->model_path);

    // create file name for mkstemp
    if(asprintf(&temp_file, "%s.XXXXXX", m->model_path)
       > PATH_MAX)
    {
        ERRPRINTF("filename too long: %s\n", temp_file);

        free(temp_file);
        temp_file = NULL;
        return -1; //fail
    }

    //open temp file
    temp_fd = mkstemp(temp_file);
    if(temp_fd == -1) {
        ERRPRINTF("Error opening temp file for writing: %s\n", temp_file);
        perror("mkstemp");
        return -1; //fail
    }


    //create output buffer
    output_buffer = xmlOutputBufferCreateFd(temp_fd, NULL);
    if(output_buffer == NULL) {
        ERRPRINTF("Error creating xml output buffer.\n");

        unlink(temp_file);
        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

        return -1; //fail
    }

    //create xml writer
	writer = xmlNewTextWriter(output_buffer);
	if (writer == NULL) {
		ERRPRINTF("Error creating the xml writer\n");

        unlink(temp_file);
        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

		return -1; //fail
	}

	rc = xmlTextWriterSetIndent(writer, 1);
	if(rc < 0) {
		ERRPRINTF("Error setting indent\n");

		xmlFreeTextWriter(writer);
        unlink(temp_file);
        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

		return -1;
	}

    //write model
	rc = write_model_xtwp(writer, m);
	if(rc < 0) {
		ERRPRINTF("Error writing model, partial model saved in: %s\n",
                   temp_file);

		xmlFreeTextWriter(writer);
        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

		return -1;
	}

	xmlFreeTextWriter(writer);

    if(fsync(temp_fd) < 0) {
        ERRPRINTF("Error syncing data for file, remove:%s manually.\n",
                  temp_file);
        perror("fsync");

        close(temp_fd);
        free(temp_file);
        temp_file = NULL;

        return -1;

    }

    //close temp file
    if(close(temp_fd) < 0) {
        ERRPRINTF("Error closing file, remove:%s manually\n", temp_file);

        free(temp_file);
        temp_file = NULL;
        return -1;
    }

    //open model file
    model_fd = open(m->model_path, O_WRONLY|O_CREAT, S_IRWXU);
    if(model_fd < 0) {
        ERRPRINTF("Error opening model file: %s.\n", m->model_path);

        free(temp_file);
        temp_file = NULL;

        return -1;
    }

    //set up a write lock
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_pid = 0;

    //get write lock on model file so reading processes will block
    if(fcntl(model_fd, F_SETLKW, &fl) == -1) {
        ERRPRINTF("Error aquiring lock on model file: %s\n", m->model_path);
        perror("fcntl");

        close(model_fd);
        free(temp_file);
        temp_file = NULL;

        return -1;
    }

    //rename temp file to model file
    //TODO if we port to windows, this will not work
    if(rename(temp_file, m->model_path) < 0) {
        ERRPRINTF("Error renaming %s to %s, leaving %s on filesystem\n",
                  temp_file, m->model_path, temp_file);

        close(model_fd);
        free(temp_file);
        temp_file = NULL;

        return -1;
    }

    free(temp_file);
    temp_file = NULL;

    //close and free lock
    if(close(model_fd) < 0) {
        ERRPRINTF("Error closing file: %s\n.", m->model_path);

        return -1;
    }


    //sync dir so we are sure everything is committed to the filesystem
    if(sync_parent_dir(m->model_path) < 0) {
        ERRPRINTF("Error syncing parent dir of: %s.\n", m->model_path);

        return -1;
    }

    m->unwritten_num_execs = 0;
    m->unwritten_time_spend = 0;

	return 0; //success
}

/*!
 * attempt to sync and close an open file descriptor.
 *
 */
int
sync_close_file(int fd)
{
    int ret = 0;

    //sync file
    if(fsync(fd) < 0) {
        ERRPRINTF("Error syncing data for file descriptor: %d.\n", fd);
        perror("fsync");

        ret = -1;
    }

    //close file
    if(close(fd) < 0) {
        ERRPRINTF("Error closing file descriptor: %d.\n", fd);
        perror("close");

        ret = -1;
    }

    return ret;
}

int
sync_parent_dir(char *file_path)
{
    char *dir_c;
    char *dir_name;
    int dir_fd;

    int ret = 0;

    dir_c = strdup(file_path);
    if(dir_c == NULL) {
        ERRPRINTF("Could not allocate memory to copy file_path.\n");
        perror("strdup");

        free(dir_c);
        dir_c = NULL;

        return -1;
    }

    dir_name = dirname(dir_c);

    dir_fd = open(dir_name, O_RDONLY);
    if(dir_fd < 0) {
        ERRPRINTF("Error opening directory: %s.\n", dir_name);
        perror("open");

        free(dir_c);
        dir_c = NULL;

        return -1;
    }

    if(fsync(dir_fd) < 0) {
        ERRPRINTF("Error fsycing directory: %s.\n", dir_name);
        perror("fsync");

        ret = -1;
    }

    if(close(dir_fd) < 0) {
        ERRPRINTF("Error closing directory: %s.\n", dir_name);
        perror("close");

        ret = -1;
    }

    free(dir_c);
    dir_c = NULL;

    return ret;
}

/*
 * writes model to xmlTextWriterPtr
 */
int write_model_xtwp(xmlTextWriterPtr writer, struct pmm_model *m)
{
	int rc;
	struct pmm_interval *i;

	// start document with standard version/enconding
	rc = xmlTextWriterStartDocument(writer, NULL, "ISO-8859-1", NULL);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartDocument\n");
		return rc;
	}

	// start root element model
	rc = xmlTextWriterStartElement(writer, BAD_CAST "model");
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartElement (model)\n");
		return rc;
	}

	// Add an element with name "completion" and value to the model
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "completion",
			"%d", m->completion);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (completion)\n");
		return rc;
	}

	// Add an element with name "n_p" and value to the model
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "n_p",
			"%d", m->n_p);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (n_p)\n");
		return rc;
	}

	// Add an element with name "complete" and value to the model
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "complete",
			"%d", m->complete);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (complete)\n");
		return rc;
	}

    // write the parameter definitions from the parent routine
    rc = write_paramdef_set_xtwp(writer, m->parent_routine->pd_set);
    if(rc < 0) {
        ERRPRINTF("Error in write_paramdef_set_xtwp.\n");
        return rc;
    }

	// start writing bench list
	rc = write_bench_list_xtwp(writer, m->bench_list);
	if(rc < 0) {
		ERRPRINTF("Error in write_bench_list_xtwp.\n");
		return rc;
	}

	// start interval_list element
	rc = xmlTextWriterStartElement(writer, BAD_CAST "interval_list");
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartElement (interval_list)\n");
		return rc;
	}

	i = m->interval_list->top;
	while(i != NULL) {

		rc = write_interval_xtwp(writer, i);
        if (rc < 0) {
            ERRPRINTF("Error writing interval.\n");
            print_interval(PMM_ERR, i);
            return rc;
        }

		i = i->previous;
	}

	// Close the interval_list element
	rc = xmlTextWriterEndElement(writer);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterEndElement\n");
		return rc;
	}

	// Close the root element named model (and all other open tags).
	rc = xmlTextWriterEndDocument(writer);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterEndDocument\n");
		return rc;
	}

	return 0; //success
}


int write_bench_list_xtwp(xmlTextWriterPtr writer,
		                  struct pmm_bench_list *bench_list)
{

	int rc;
	struct pmm_benchmark *b;

	// start bench_list element
	rc = xmlTextWriterStartElement(writer, BAD_CAST "bench_list");
	if(rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartElement (bench_list)\n");
		return rc;
	}

	// Add an element with the name "size" and value: size of bench list
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "size", "%d",
			bench_list->size);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (size)\n");
		return rc;
	}


	b = bench_list->first;
	while(b != NULL) {
		rc = write_benchmark_xtwp(writer, b);

		if(rc < 0) {
			ERRPRINTF("Error in write_benchmark_xtwp\n");
			return rc;
		}
		b = b->next;
	}

	// Close the bench_list element
	rc = xmlTextWriterEndElement(writer);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterEndElement (bench_list)\n");
		return rc;
	}

	return 0; //success
}

/*
 * TODO change all this so it can be reused for communication via socket
 */

/*!
 * Write the parameter definition set to an xmlTextWriterPtr object
 *
 * @param   writer      xmlTextWriter pointer
 * @param   pd_set      pointer to the parameter defintion set
 *
 * @returns 0 on success, -1 on failure
 */
int
write_paramdef_set_xtwp(xmlTextWriterPtr writer,
                        struct pmm_paramdef_set *pd_set)
{
    int rc;
    int i;

    // start and element named parameters
    rc = xmlTextWriterStartElement(writer, BAD_CAST "parameters");
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterStartElement (parameters)\n");
        return rc;
    }

    // add an element with name "n_p" and value number of parameters
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "n_p",
            "%d", pd_set->n_p);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (n_p)\n");
        return rc;
    }

    //write each parameter definition
    for(i=0; i<pd_set->n_p; i++) {
        rc = write_paramdef_xtwp(writer, &(pd_set->pd_array[i]));
        if (rc < 0) {
            ERRPRINTF("Error writing parameter definition\n");
            return rc;
        }
    }
    
    if(pd_set->pc_formula != NULL) {
        // start and element named param_constraint
        rc = xmlTextWriterStartElement(writer, BAD_CAST "param_constraint");
        if (rc < 0) {
            ERRPRINTF("Error @ xmlTextWriterStartElement (param_constraint)\n");
            return rc;
        }

        // add an element with name "pc_formula" and value of the formula string
        rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "formula",
                "%s", pd_set->pc_formula);
        if (rc < 0) {
            ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (pc_formula)\n");
            return rc;
        }
        // add an element with name "pc_max" and value of max parameter product
        rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "pc_max",
                "%d", pd_set->pc_max);
        if (rc < 0) {
            ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (pc_max)\n");
            return rc;
        }
        // add an element with name "pc_min" and value of min parameter product
        rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "pc_min",
                "%d", pd_set->pc_min);
        if (rc < 0) {
            ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (pc_min)\n");
            return rc;
        }

        // Close the param_constraint element.
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            ERRPRINTF("Error @ xmlTextWriterEndElement\n");
            return rc;
        }
    }

    // Close the parameters element.
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterEndElement\n");
        return rc;
    }

    return 0;
}

/*!
 * Write a parameter definition to an xmlTextWriterPtr object
 *
 * @param   writer  xmlTextWriter pointer
 * @param   pd      pointer to the parameter definition to write
 *
 * @return 0 on success -1 on error
 */
int
write_paramdef_xtwp(xmlTextWriterPtr writer, struct pmm_paramdef *pd)
{
    int rc;

    // start an element named param
    rc = xmlTextWriterStartElement(writer, BAD_CAST "param");
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterStartElement (param)\n");
        return rc;
    }

  
    //add an element with name "order" and value of the order
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "order", "%d",
                                         pd->order);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (order)\n");
        return rc;
    }

    //add an element with name "name" and value of the name 
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "name", "%s",
                                         pd->name);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (name)\n");
        return rc;
    }

    //add an element with name "start" and value of the start
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "start", "%d",
                                         pd->start);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (start)\n");
        return rc;
    }

    //add an element with name "end" and value of the end
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "end", "%d",
                                         pd->end);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (end)\n");
        return rc;
    }

    //add an element with name "stride" and value of the stride
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "stride", "%d",
                                         pd->stride);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (stride)\n");
        return rc;
    }

    //add an element with name "stride" and value of the stride
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "offset", "%d",
                                         pd->offset);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (offset)\n");
        return rc;
    }

    //add an element with name "nonzero_end" and value of the nonzero_end 
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "nonzero_end", "%d",
                                         pd->nonzero_end);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (nonzero_end)\n");
        return rc;
    }

    // Close the param element.
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterEndElement\n");
        return rc;
    }

    return 0;
}


/*!
 * Write a parameter array to an xmlTextWriterPtr object
 *
 * In the form:
 *
 *   \<parameter_array>
 *     \<n_p>n\</n_p>
 *     \<parameter>p[0]\</parameter>
 *     \<parameter> ....
 *   \</parameter_array>
 *
 * @param   writer  writer to use
 * @param   p       parameter array
 * @param   n       number of parameters in array
 *
 * @return 0 on succes, -1 on failure
 */
int
write_parameter_array_xtwp(xmlTextWriterPtr writer, int *p, int n)
{
    int rc;
    int i;


    // start and element named parameter_array
    rc = xmlTextWriterStartElement(writer, BAD_CAST "parameter_array");
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterStartElement (parameter_array)\n");
        return rc;
    }

    // add an element with name "n_p" and value number of parameters
    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "n_p",
            "%d", n);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (n_p)\n");
        return rc;
    }

    for(i=0; i<n; i++) {
        //add an element with name "parameter" and value of the param
        rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "parameter",
                "%d", p[i]);
        if (rc < 0) {
            ERRPRINTF("Error @ xmlTextWriterWriteFormatElement "
                    "(parameter)\n");
            return rc;
        }
    }

    // Close the parameter_array element.
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        ERRPRINTF("Error @ xmlTextWriterEndElement\n");
        return rc;
    }

    return 0;
}


int write_benchmark_xtwp(xmlTextWriterPtr writer, struct pmm_benchmark *b)
{
	int rc;

	// Start an element named benchmark
	rc = xmlTextWriterStartElement(writer, BAD_CAST "benchmark");
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartElement (bench)\n");
		return rc;
	}

    //write parameters
    rc = write_parameter_array_xtwp(writer, b->p, b->n_p);
    if(rc < 0) {
        ERRPRINTF("Error writing parameter array.\n");
        return rc;
    }

	// Add an element with name "complexity" and value to bench.
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "complexity",
			"%lld", b->complexity);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (complexity)\n");
		return rc;
	}

	// Add an element with name "flops" and value to benchmark
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "flops",
			"%f", b->flops);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (flops)\n");
		return rc;
	}
	// Add an element with name seconds and value to benchmark
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "seconds",
			"%f", b->seconds);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (seconds)\n");
		return rc;
	}

	// Start an element named used_time
	rc = xmlTextWriterStartElement(writer, BAD_CAST "used_time");
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartElement (used_time)\n");
		return rc;
	}

	write_timeval_xtwp(writer, &(b->used_t));

	// Close the used_time element.
	rc = xmlTextWriterEndElement(writer);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterEndElement\n");
		return rc;
	}

	// Start an element named wall_time
	rc = xmlTextWriterStartElement(writer, BAD_CAST "wall_time");
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterStartElement (wall_time)\n");
		return rc;
	}

	write_timeval_xtwp(writer, &(b->wall_t));

	// Close the wall_time element.
	rc = xmlTextWriterEndElement(writer);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterEndElement\n");
		return rc;
	}

	// Close the benchmark element.
	rc = xmlTextWriterEndElement(writer);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterEndElement\n");
		return rc;
	}

	return 0; //success
}


int write_timeval_xtwp(xmlTextWriterPtr writer, struct timeval *t)
{
	int rc;

	// Add an element with name "secs" and value
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "secs",
			"%ld", t->tv_sec);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (secs)\n");
		return rc;
	}

	// Add an element with name "usecs" and value.
	rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "usecs",
			"%ld", t->tv_usec);
	if (rc < 0) {
		ERRPRINTF("Error @ xmlTextWriterWriteFormatElement (usecs)\n");
		return rc;
	}

	return 0; // success

}

void xmlparser_init()
{
    xmlInitParser();
}

void xmlparser_cleanup()
{
	xmlCleanupParser();
}
