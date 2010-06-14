
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

#ifdef HAVE_MUPARSER

#include <iostream>
#include <string>
#include <muParser/muParser.h>

extern "C" {
#include "pmm_data.h"
#include "pmm_log.h"
}
#include "pmm_muparse.h"

extern "C"
int
create_param_constraint_muparser(struct pmm_paramdef_set *pd_set)
{
    int i;
    string_type str;

    pd_set->pc_parser = new PMM_Param_Constraint_Muparser;
    if(pd_set->pc_parser == NULL) {
        ERRPRINTF("Error allocating memory.\n");
        return -1;
    }

    pd_set->pc_parser->n_p = pd_set->n_p;
    pd_set->pc_parser->vars = new double[pd_set->pc_parser->n_p];


    // set up parser variables, by name and pointer to element of 'vars'
    for(i=0; i<pd_set->pc_parser->n_p; i++) {
        pd_set->pc_parser->vars[i] = 1.0;
        str = pd_set->pd_array[i].name;

        pd_set->pc_parser->p.DefineVar(str,
                         &(pd_set->pc_parser->vars[i]));
        DBGPRINTF("setting up variables:\n");
        std::cout << str << " with value " << pd_set->pc_parser->vars[i] << "\n";
    }

    pd_set->pc_parser->p.SetExpr(pd_set->pc_formula);

    DBGPRINTF("setting up formula:\n");
    std::cout << pd_set->pc_formula << "\n";

    return 0;
}

extern "C"
int
evaluate_constraint_with_params(struct pmm_param_constraint_muparser* pc_parser,
                                int* params, double *value)
{

    int i;

    for(i=0; i<pc_parser->n_p; i++)
        pc_parser->vars[i] = (double)params[i];

    return evaluate_constraint(pc_parser, value);

}

extern "C"
int
evaluate_constraint(struct pmm_param_constraint_muparser* pc_parser,
                    double *value)
{
    
    try {
        *value = pc_parser->p.Eval();
        DBGPRINTF("evaluated constraint formula to:%f\n", *value);
    }
    catch (Parser::exception_type &e) {
        ERRPRINTF("Error evaluating parameter constraint formula, message:\n");
        std::cout << e.GetMsg() << "\n";

        return -1;
    }

    return 0;
}

#endif /* HAVE_MUPARSER */
