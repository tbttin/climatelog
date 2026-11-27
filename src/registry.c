#include <stdlib.h>
#include "registry.h"
#include "console_log.h"

#define REG_TRY(expr) do { \
	reg_status_t _s = (expr); \
	if (_s != REG_SUCCESS) return _s; \
} while (0)

struct dyn_buf {
	uint8_t *data;
	size_t cap_bytes;
	size_t max_bytes;
};

struct entity_registry {
	struct dyn_buf devices;
	int dev_count;

	struct dyn_buf rooms;
	int room_count;

	struct dyn_buf str_pool;
	reg_offset_t pool_used;
};

static const int REG_DYN_BUF_INIT_BYTES = 1024;
static const int REG_STR_MAX_LEN = 64;
static const int REG_POOL_MAX_SIZE = 4096;

/* TIER 1: RAW MEMORY LAYER (Knows only about bytes, offsets, and bounds) */
static reg_status_t dyn_buf_init(struct dyn_buf *b, size_t max_bytes);
static void dyn_buf_clear(struct dyn_buf *b);
static void dyn_buf_free(struct dyn_buf *b);
static reg_status_t dyn_buf_expand(struct dyn_buf *b, size_t bytes_needed);
static reg_status_t dyn_buf_write(struct dyn_buf *b, size_t byte_offset, const void *src, size_t nbytes);
static reg_status_t dyn_buf_read(struct dyn_buf *b, size_t byte_offset, size_t nbytes, void *dest);

static reg_status_t dyn_buf_set_at(struct dyn_buf *b, size_t index, const void *elem, size_t elem_size);
static reg_status_t dyn_buf_get_at(struct dyn_buf *b, size_t index, const void *out_elem, size_t elem_size);

/* TIER 2: TYPED ENTITY ACCESSORS (Translates reg_id_t -> byte offsets) */
static reg_status_t dev_buf_set(struct dyn_buf *b, reg_id_t dev_id, reg_id_t room_id);
static reg_status_t dev_buf_get(struct dyn_buf *b, reg_id_t dev_id, reg_id_t *out_room_id);
static reg_status_t room_buf_set(struct dyn_buf *b, reg_id_t room_id, reg_offset_t str_offset);
static reg_status_t room_buf_get(struct dyn_buf *b, reg_id_t room_id, reg_offset_t *out_str_offset);

static reg_status_t reg_intern(struct entity_registry *reg, const char *str, reg_offset_t *out_offset);

reg_status_t reg_create(struct entity_registry **out_reg, int max_devices, int max_rooms) {
	if (max_devices < 1 || max_rooms < 1) { return REG_ERR_INVALID_ARG; }

	struct entity_registry *reg = calloc(1, sizeof(*reg));
	if (!reg) { return REG_ERR_NOMEM; }

	REG_TRY(dyn_buf_init(&reg->devices, max_devices));
	reg->dev_count = 0;

	REG_TRY(dyn_buf_init(&reg->rooms, max_rooms));
	reg->room_count = 0;

	REG_TRY(dyn_buf_init(&reg->str_pool, REG_POOL_MAX_SIZE));
	str_buf_set(&reg->str_pool, 0, "\0");
	reg->pool_used = 1;

	*out_reg = reg;
	return REG_SUCCESS;
}

void reg_clear(struct entity_registry *reg) {
	if (!reg) return;
	dyn_buf_clear(&reg->devices);
	dyn_buf_clear(&reg->rooms);
	dyn_buf_clear(&reg->str_pool);
	reg->dev_count = reg->room_count = 0;
	reg->pool_used = 1;
}

void reg_destroy(struct entity_registry *reg) {
	if (!reg) return;
	dyn_buf_free(&reg->devices);
	dyn_buf_free(&reg->rooms);
	dyn_buf_free(&reg->str_pool);
	*reg = (struct entity_registry){ 0 };
}

reg_status_t reg_set_room(struct entity_registry *reg, reg_id_t id, const char *name) {
	if (!reg || !id) { return REG_ERR_INVALID_ARG; }

	struct dyn_buf *rooms = &reg->rooms;
	reg_offset_t offset = 0;

	REG_TRY(room_buf_get(rooms, id, &offset));

	if (!offset) { /* New room ID */
		REG_TRY(reg_intern(reg, name, &offset));
		REG_TRY(room_buf_set(rooms, id, offset));
		reg->room_count++;
	}

	return REG_SUCCESS;
}

reg_status_t reg_get_room_name(struct entity_registry *reg, reg_id_t id, char *out_name, size_t len) {
	if (!reg || !id || !out_name) { return REG_ERR_INVALID_ARG; }

	struct dyn_buf *room = &reg->rooms;
	reg_offset_t offset = 0;

	REG_TRY(room_buf_get(room, id, &offset));
	if (!offset) { return REG_ERR_NOT_FOUND; }

	const char * room_name = (const char *)(rooms->str_pool.data + offset);
	size_t needed = strlen(room_name) + 1;
	if (needed > len) { return REG_ERR_INVALID_ARG; }
	memcpy(out_name, room_name, needed);

	return REG_SUCCESS;
}

reg_status_t reg_set_device(struct entity_registry *reg, reg_id_t device_id, reg_id_t room_id) {
	if (!reg || !device_id || !room_id) { return REG_ERR_INVALID_ARG; }

	ZZ

	return REG_SUCCESS;
}

reg_status_t dyn_buf_init(struct dyn_buf *b, size_t max_bytes) {
	if (!b || !max_bytes) { return REG_ERR_INVALID_ARG; }

	size_t init_cap = (max_bytes < REG_DYN_BUF_INIT_BYTES) ? max_bytes : REG_DYN_BUF_INIT_BYTES;
	void *data = calloc(1, init_cap);
	if (!data) { return REG_ERR_NOMEM; }

	*b = (struct dyn_buf){
		.data = data,
		.cap_bytes = init_bytes
	  	.max_bytes = max_bytes,
	};

	return REG_SUCCESS;
}

void dyn_buf_clear(struct dyn_buf *b) {
	if (b && b->cap_bytes) { memset(b->data, 0, b->cap_bytes); }
}

void dyn_buf_free(struct dyn_buf *b) {
	if (!b) return;
	free(b->data);
	b->data = NULL;
	b->cap_bytes = b->max_bytes = 0;
}

reg_status_t dyn_buf_expand(struct dyn_buf *b, size_t bytes_needed) {
	if (!b) return REG_ERR_INVALID_ARG;
	if (bytes_needed > b->max_bytes) return REG_ERR_OUT_OF_BYTES_LIMIT;
	if (bytes_needed <= b->cap_bytes) return REG_SUCCESS;

	size_t new_cap = b->bytes_cap;
	while (new_cap < bytes_needed) { new_cap = (new_cap > SIZE_MAX / 2) ? b->max_bytes : new_cap * 2; }
	if (new_cap > b->max_bytes) { new_cap = b->max_bytes; }

	char *new_data = realloc(b->data, new_cap);
	if (!new_data) { return REG_ERR_NOMEM; }
	
	memset(new_data + b->bytes_cap, 0, new_cap - b-bytes_cap);

	b->data = new_data;
	b->cap_bytes = new_cap;
	return REG_SUCCESS;
}

reg_status_t dyn_buf_write(struct dyn_buf *b, size_t byte_offset, const void *src, size_t nbytes) { 
	if (!b || !src) { return REG_ERR_INVALID_ARG; }
	if (byte_offset + nbytes > b->max_bytes) { return REG_ERR_OUT_OF_BYTES_LIMIT; }

	size_t end_offet = byte_offset + nbytes;
	if (end_offset > b->cap_bytes) { REG_TRY(dyn_buf_expand(b, end_offet)); }

	memcpy(b->data + byte_offset, src, nbytes);

	return REG_SUCCESS;
}

reg_status_t dyn_buf_read(struct dyn_buf *b, size_t byte_offset, size_t nbytes, void *dest) {
	if (!b || !nbytes) { return REG_ERR_INVALID_ARG; }
	if (byte_offset + nbytes > b->cap_bytes) { return REG_ERR_OUT_OF_BYTES_LIMIT; }

	memcpy(dest, b->data + byte_offset, nbytes);

	return REG_SUCCESS;
}

reg_status_t dyn_buf_set_at(struct dyn_buf *b, size_t index, const void *elem, size_t elem_size) {
	if (!b || !elem || !elem_size) { return REG_ERR_INVALID_ARG; }
	if (index > b->cap_bytes / elem_size) { return REG_ERR_ID_OUT_OF_RANGE; }
	return dyn_buf_write(b, index * elem_size, elem, elem_size);
}

reg_status_t dyn_buf_get_at(struct dyn_buf *b, size_t index, const void *out_elem, size_t elem_size) {
	if (!b || !out_elem || !elem_size) { return REG_ERR_INVALID_ARG; }
	if (index > b->cap_bytes / elem_size) { return REG_ERR_ID_OUT_OF_RANGE; }
	return dyn_buf_read(b, index * elem_size, out_elem, elem_size);
}

reg_status_t dev_buf_set(struct dyn_buf *b, reg_id_t dev_id, reg_id_t room_id) {
	if (!b || dev_id < 1 || room_id < 1) { return REG_ERR_INVALID_ARG; }
	return dyn_buf_set_at(b, (size_t)dev_id, &room_id, sizeof(room_id));
}

reg_status_t dev_buf_get(struct dyn_buf *b, reg_id_t dev_id, reg_id_t *out_room_id) {
	if (!b || dev_id < 1) { return REG_ERR_INVALID_ARG; }
	return dyn_buf_get_at(b, (size_t)dev_id, out_room_id, sizeof(*out_room_id));
}

reg_status_t room_buf_set(struct dyn_buf *b, reg_id_t room_id, reg_offset_t str_offset) {
	if (!b || room_id < 1) { return REG_ERR_INVALID_ARG; }
	return dyn_buf_set_at(b, (size_t)room_id, &str_offset, sizeof(str_offset));
}

reg_status_t room_buf_get(struct dyn_buf *b, reg_id_t room_id, reg_offset_t *out_str_offset) {
	if (!b || dev_id < 1) { return REG_ERR_INVALID_ARG; }
	return dyn_buf_get_at(b, (size_t)room_id, out_str_offset, sizeof(*out_str_offset));
}

reg_status_t reg_intern(struct entity_registry *reg, const char *str, reg_offset_t *out_offset) {
	if (!reg || !reg->str_pool.data || !str || !out_offset) return REG_ERR_INVALID_ARG;

	size_t len = strnlen(str, REG_STR_MAX_LEN + 1);
	if (!len || len > REG_STR_MAX_LEN) return REG_ERR_INVALID_ARG;

	struct dyn_buf *str_pool = &reg->str_pool;
	size_t *pool_used = &reg->pool_used;

	size_t currsor = 1;
	while (currsor < *pool_used) {
		const char *existing = (const char *)(str_pool->data + currsor);
		size_t existing_len = strlen(existing);

		if (len == existing_len && memcmp(existing, str, len) == 0) {
			*out_offset = (reg_offset_t) currsor;
			return REG_SUCCESS;
		}

		currsor += existing_len + 1;
	}

	size_t write_len = len + 1;
	if (write_len > REG_OFFSET_MAX - reg->pool_used) { return REG_ERR_POOL_FULL; }

	reg_offset_t new_offset = reg->pool_used;
	REG_TRY(dyn_buf_write(str_pool, new_offset, str, write_len));

	*pool_used += (reg_offset_t)write_len;
	*out_offset = new_offset;

	return REG_SUCCESS;
}
