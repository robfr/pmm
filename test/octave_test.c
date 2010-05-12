#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_CONFIG

#include <stdio.h>

#include "pmm_data.h"
#include "pmm_cfgparser.h"


int main(void) {

   struct pmm_config *cfg;
   struct pmm_benchmark *b;

   long long int p[2];

   cfg = new_config();

   parse_config(cfg);

   parse_models(cfg);

   parse_history(cfg->loadhistory);


    p[0] = 124;
    p[1] = 244;

    print_config(cfg);

    print_model(cfg->routines[1]->model);


    b = lookup_model(cfg->routines[1]->model, p);

    printf("Interpolation of %lld, %lld:\n", p[0], p[1]);

    print_benchmark(b);

    printf(":-)\n");

    free_config(cfg);


    return 1;
}

#endif /* ENABLE_OCTAVE */
