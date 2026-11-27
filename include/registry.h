#ifndef REGISTRY_H_INCLUDED
#define REGISTRY_H_INCLUDED

#include <string.h>
#include <stdint.h>
#include "db.h"

typedef enum reg_status {
	REG_SUCCESS = 0,
	REG_ERR,
	REG_ERR_NOMEM,
	REG_ERR_OUT_OF_BYTES_LIMIT,
	REG_ERR_INVALID_ARG,
	REG_ERR_ID_OUT_OF_RANGE,
	REG_ERR_POOL_FULL,
	REG_ERR_NOT_FOUND,
} reg_status_t;

typedef uint16_t reg_id_t;
#define REG_ID_MAX UINT16_MAX
typedef uint16_t reg_offset_t;
#define REG_OFFSET_MAX UINT16_MAX

struct entity_registry;

reg_status_t reg_create(struct entity_registry **out_reg, reg_id_t max_devices, reg_id_t max_locs);
void reg_clear(struct entity_registry *reg);
void reg_destroy(struct entity_registry *reg);

reg_status_t reg_set_loc(struct entity_registry *reg, reg_id_t id, const char *name);
reg_status_t reg_get_loc(struct entity_registry *reg, reg_id_t loc_id, char *out_name, size_t len);

reg_status_t reg_set_device(struct entity_registry *reg, reg_id_t device_id, reg_id_t loc_id);
reg_status_t reg_get_device(struct entity_registry *reg, reg_id_t device_id, reg_id_t *out_loc);

#endif /* REGISTRY_H_INCLUDED */

