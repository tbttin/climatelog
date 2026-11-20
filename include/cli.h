#ifndef CLI_H_INCLUDED
#define CLI_H_INCLUDED

#include <time.h>
#include "db.h"

struct cli_args {
	int num;
};

db_status_t db_cli_tail(struct db_context *ctx, const struct cli_args *args);
db_status_t db_cli_print_status(struct db_context *pdb, const struct cli_args *unused);

#endif /* CLI_H_INCLUDED */

