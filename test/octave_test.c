#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_OCTAVE

#include <stdio.h>

#include "pmm_model.h"
#include "pmm_cfgparser.h"
#include "pmm_log.h"


int main(void) {

   struct pmm_config *cfg;
   struct pmm_benchmark *b;
   int i;

   int p[2];

   cfg = new_config();

   parse_config(cfg);

   parse_models(cfg);

   parse_history(cfg->loadhistory);


    p[0] = 124;
    p[1] = 244;

    print_config(PMM_LOG, cfg);

    for(i=0;i<cfg->used; i++) {
        if(cfg->routines[i]->pd_set->n_p == 2) {
            break;
        }
    }
    if(i==cfg->used) {
        printf("Could not find a 2-d routine.\n");
        return -1;
    }

    print_model(PMM_LOG, cfg->routines[i]->model);


    b = lookup_model(cfg->routines[i]->model, p);

    printf("Interpolation of %d, %d:\n", p[0], p[1]);

    print_benchmark(PMM_LOG, b);

    printf(":-)\n");

    free_config(&cfg);


    return 1;
}

#endif /* ENABLE_OCTAVE */
