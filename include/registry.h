#ifndef REGISTRY_H_INCLUDED
#define REGISTRY_H_INCLUDED

#include <string.h>
#include <stdint.h>

#define REG_STR_MAX_LEN 64
#define REG_POOL_MAX_SIZE 1024

struct entity_registry {
	int *device_room;
	int device_cap;
	int device_count;

	uint32_t *room_offsets;
	int room_cap;
	int room_count;

	char *str_pool;
	size_t pool_used;
	size_t pool_cap;
};

int reg_init(struct entity_registry *reg);
int reg_intern(struct entity_registry *reg, char *str, size_t *offset);
const char * reg_get(struct entity_registry *reg, size_t offset);
int reg_clear(struct entity_registry *reg);
int reg_free(struct entity_registry *reg);

#endif /* REGISTRY_H_INCLUDED */

