#include <stdio.h>

#include "pmm_model.h"
#include "pmm_cfgparser.h"

int main(void) {

	struct pmm_config *c;

	printf("test_cfgparser\n");

	printf("init config structure\n");
	c = new_config();

	c->configfile="pmm.cfg.xml";

	printf("parse config\n");
	parse_config(c);

	return 1;
}

