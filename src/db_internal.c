#include "db_internal.h"
#include "log.h"

bool is_id_out_of_bounds(struct db_context *ctx, int id) {
	return (id >= (ctx->device_max + 1) || id < 1);
}

db_status_t db_get_device_loc(struct db_context *ctx, char **ret_loc, int id) {
	if (is_id_out_of_bounds(ctx, id)) {
		LOG_ERR("There is no such device ID (%d)", id);
		*ret_loc = NULL;
		return DB_ERR_ID_OUT_OF_BOUNDS ;
	}

	*ret_loc = ctx->devices[id].location;
	return DB_SUCCESS;
}

