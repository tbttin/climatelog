#include <stdlib.h>
#include "registry.h"
#include "log.h"

int reg_init(struct entity_registry *reg) {
	reg->str_pool = calloc(32, sizeof(char));
	reg->pool_cap = 32;
	if (!reg->str_pool) { return 1; }
	reg->str_pool[0] = '\0';
	reg->pool_used = 1;

	return 0;
}

int reg_intern(struct entity_registry *reg, char *str, size_t *oo) {
	if (!reg || !str) return -1;

	size_t len = strnlen(str, REG_STR_MAX_LEN + 1);
	if (!len || len > REG_STR_MAX_LEN) return -1;

	size_t offset = 1;
	while (offset < reg->pool_used) {
		const char *existing = &reg->str_pool[offset];
		size_t existing_len = strlen(existing);

		if (len == existing_len && memcmp(existing, str, len) == 0) {
			*oo = offset;
			return 0;
		}

		offset += existing_len + 1;
	}

	size_t bytes_needed = len + 1;
	size_t new_used = reg->pool_used + bytes_needed;

	if (new_used > REG_POOL_MAX_SIZE) return -1;

	if (new_used > reg->pool_cap) {
		size_t new_cap = (reg->pool_cap == 0) ? 64 : 2 * reg->pool_cap;
		while (new_cap < new_used) { new_cap *= 2; }

		if (new_cap > REG_POOL_MAX_SIZE) new_cap = REG_POOL_MAX_SIZE;

		char *new_pool = realloc(reg->str_pool, new_cap);
		if (!new_pool) return -1; /* Handle and free old pool */

		reg->str_pool = new_pool;
		reg->pool_cap = new_cap;
	}

	offset = reg->pool_used;
	memcpy(&reg->str_pool[offset], str, bytes_needed);
	reg->pool_used += bytes_needed;

	*oo = offset;
	return 0;
}

const char * reg_get(struct entity_registry *reg, size_t offset) {
	if (!reg || offset > reg->pool_used) return NULL;

	return &reg->str_pool[offset];
}

int reg_clear(struct entity_registry *reg) {
	if (!reg) return 0;

	reg->pool_cap = reg->pool_used = 0;

	return 0;
}

int reg_free(struct entity_registry *reg) {
	if (!reg) return 0;

	reg_clear(reg);
	free(reg->str_pool);
	reg->str_pool = NULL;
	/* free(reg); */

	return 0;
}

