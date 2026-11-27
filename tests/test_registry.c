#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "registry.h"

static void test_lifecycle(void) {
	struct entity_registry *reg = NULL;

	assert(reg_create(NULL, 10, 10) == REG_ERR_INVALID_ARG);
	assert(reg_create(&reg, 0, 10) == REG_ERR_INVALID_ARG);
	assert(reg_create(&reg, 10, 0) == REG_ERR_INVALID_ARG);

	assert(reg_create(&reg, 64, 32) == REG_SUCCESS);
	assert(reg != NULL);

	reg_destroy(reg);
	reg_destroy(NULL);
}

static void test_locations_set_get_and_overwrite(void) {
	struct entity_registry *reg = NULL;
	assert(reg_create(&reg, 64, 100) == REG_SUCCESS);

	char buf[128] = { 0 };
	assert(reg_get_loc(reg, 1, buf, sizeof(buf)) == REG_ERR_NOT_FOUND);

	/* ID 80 exceeds initial 64-byte capacity (32 elements), forcing expansion */
	assert(reg_set_loc(reg, 1, "ROOM_A") == REG_SUCCESS);
	assert(reg_set_loc(reg, 80, "SERVER_ROOM") == REG_SUCCESS);

	assert(reg_get_loc(reg, 1, buf, sizeof(buf)) == REG_SUCCESS);
	assert(strcmp(buf, "ROOM_A") == 0);

	assert(reg_get_loc(reg, 80, buf, sizeof(buf)) == REG_SUCCESS);
	assert(strcmp(buf, "SERVER_ROOM") == 0);

	/* Unassigned index inside expanded range must report REG_ERR_NOT_FOUND */
	assert(reg_get_loc(reg, 42, buf, sizeof(buf)) == REG_ERR_NOT_FOUND);

	/* Overwrite existing location ID */
	assert(reg_set_loc(reg, 1, "LOBBY") == REG_SUCCESS);
	assert(reg_get_loc(reg, 1, buf, sizeof(buf)) == REG_SUCCESS);
	assert(strcmp(buf, "LOBBY") == 0);

	/* Destination buffer too small for string + null terminator */
	char tiny[4] = { 0 };
	assert(reg_get_loc(reg, 1, tiny, sizeof(tiny)) == REG_ERR_INVALID_ARG);

	reg_destroy(reg);
}

static void test_string_deduplication(void) {
	struct entity_registry *reg = NULL;
	assert(reg_create(&reg, 16, 500) == REG_SUCCESS);

	char name1[] = "SHARED_WAREHOUSE";
	char name2[] = "SHARED_WAREHOUSE";

	/* 450 locations sharing 1 string must deduplicate and stay under 4096 bytes */
	for (reg_id_t i = 1; i != 451; i++) {
		assert(reg_set_loc(reg, i, (i % 2 == 0) ? name1 : name2) == REG_SUCCESS);
	}

	char out[128] = { 0 };
	assert(reg_get_loc(reg, 1, out, sizeof(out)) == REG_SUCCESS);
	assert(strcmp(out, "SHARED_WAREHOUSE") == 0);

	assert(reg_get_loc(reg, 450, out, sizeof(out)) == REG_SUCCESS);
	assert(strcmp(out, "SHARED_WAREHOUSE") == 0);

	reg_destroy(reg);
}

static void test_devices_set_get_and_expansion(void) {
	struct entity_registry *reg = NULL;
	assert(reg_create(&reg, 120, 32) == REG_SUCCESS);

	reg_id_t loc_id = 0;
	assert(reg_get_device(reg, 1, &loc_id) == REG_ERR_NOT_FOUND);

	/* Device ID 100 triggers buffer expansion */
	assert(reg_set_device(reg, 1, 5) == REG_SUCCESS);
	assert(reg_set_device(reg, 100, 12) == REG_SUCCESS);

	assert(reg_get_device(reg, 1, &loc_id) == REG_SUCCESS);
	assert(loc_id == 5);

	assert(reg_get_device(reg, 100, &loc_id) == REG_SUCCESS);
	assert(loc_id == 12);

	/* Update existing device location */
	assert(reg_set_device(reg, 100, 19) == REG_SUCCESS);
	assert(reg_get_device(reg, 100, &loc_id) == REG_SUCCESS);
	assert(loc_id == 19);

	/* Unassigned device inside expanded array */
	assert(reg_get_device(reg, 50, &loc_id) == REG_ERR_NOT_FOUND);

	reg_destroy(reg);
}

static void test_clear_and_reuse(void) {
	struct entity_registry *reg = NULL;
	assert(reg_create(&reg, 64, 64) == REG_SUCCESS);

	assert(reg_set_loc(reg, 3, "COLD_STORAGE") == REG_SUCCESS);
	assert(reg_set_device(reg, 7, 3) == REG_SUCCESS);

	reg_clear(reg);

	char out[128] = { 0 };
	reg_id_t loc_id = 0;
	assert(reg_get_loc(reg, 3, out, sizeof(out)) == REG_ERR_NOT_FOUND);
	assert(reg_get_device(reg, 7, &loc_id) == REG_ERR_NOT_FOUND);

	/* Verify registry works cleanly after clearing */
	assert(reg_set_loc(reg, 3, "GREENHOUSE") == REG_SUCCESS);
	assert(reg_get_loc(reg, 3, out, sizeof(out)) == REG_SUCCESS);
	assert(strcmp(out, "GREENHOUSE") == 0);

	reg_destroy(reg);
}

static void test_edge_cases_and_bounds(void) {
	struct entity_registry *reg = NULL;
	assert(reg_create(&reg, 10, 10) == REG_SUCCESS);

	char out[128] = { 0 };
	reg_id_t loc_id = 0;

	/* Invalid arguments and ID 0 */
	assert(reg_set_loc(NULL, 1, "A") == REG_ERR_INVALID_ARG);
	assert(reg_set_loc(reg, 0, "A") == REG_ERR_INVALID_ARG);
	assert(reg_set_loc(reg, 1, NULL) == REG_ERR_INVALID_ARG);
	assert(reg_set_loc(reg, 1, "") == REG_ERR_INVALID_ARG);
	assert(reg_set_loc(reg, 11, "A") == REG_ERR_ID_OUT_OF_RANGE);

	assert(reg_get_loc(NULL, 1, out, sizeof(out)) == REG_ERR_INVALID_ARG);
	assert(reg_get_loc(reg, 0, out, sizeof(out)) == REG_ERR_INVALID_ARG);
	assert(reg_get_loc(reg, 1, NULL, sizeof(out)) == REG_ERR_INVALID_ARG);
	assert(reg_get_loc(reg, 11, out, sizeof(out)) == REG_ERR_NOT_FOUND);

	assert(reg_set_device(NULL, 1, 1) == REG_ERR_INVALID_ARG);
	assert(reg_set_device(reg, 0, 1) == REG_ERR_INVALID_ARG);
	assert(reg_set_device(reg, 1, 0) == REG_ERR_INVALID_ARG);
	assert(reg_set_device(reg, 11, 1) == REG_ERR_ID_OUT_OF_RANGE);

	assert(reg_get_device(NULL, 1, &loc_id) == REG_ERR_INVALID_ARG);
	assert(reg_get_device(reg, 0, &loc_id) == REG_ERR_INVALID_ARG);
	assert(reg_get_device(reg, 1, NULL) == REG_ERR_INVALID_ARG);
	assert(reg_get_device(reg, 11, &loc_id) == REG_ERR_NOT_FOUND);

	/* String length boundary: 64 chars succeeds, 65 chars fails */
	char exact_max[65];
	memset(exact_max, 'x', 64);
	exact_max[64] = '\0';
	assert(reg_set_loc(reg, 1, exact_max) == REG_SUCCESS);
	assert(reg_get_loc(reg, 1, out, sizeof(out)) == REG_SUCCESS);
	assert(strcmp(out, exact_max) == 0);

	char too_long[66];
	memset(too_long, 'y', 65);
	too_long[65] = '\0';
	assert(reg_set_loc(reg, 2, too_long) == REG_ERR_INVALID_ARG);

	reg_destroy(reg);
}

static void test_pool_exhaustion(void) {
	struct entity_registry *reg = NULL;
	assert(reg_create(&reg, 16, 1000) == REG_SUCCESS);

	char unique_str[32];
	reg_id_t id = 1;
	reg_status_t status = REG_SUCCESS;

	while (id != 1001) {
		snprintf(unique_str, sizeof(unique_str), "UNIQUE_LOC_%u", id);
		status = reg_set_loc(reg, id, unique_str);
		if (status != REG_SUCCESS) {
			break;
		}
		id++;
	}

	assert(status == REG_ERR_POOL_FULL);
	assert(id != 1);

	reg_destroy(reg);
}

int main(void) {
	test_lifecycle();
	test_locations_set_get_and_overwrite();
	test_string_deduplication();
	test_devices_set_get_and_expansion();
	test_clear_and_reuse();
	test_edge_cases_and_bounds();
	test_pool_exhaustion();

	printf("All registry unit tests passed!\n");
	return 0;
}

