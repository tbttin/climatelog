#include <stdio.h>
#include <assert.h>
#include "registry.h"

static void test_basic_insert_and_get() {
	struct entity_registry reg;

	reg_init(&reg);

	size_t os1;
	assert(reg_intern(&reg, "alpha", &os1) == 0);
	size_t os2;
	assert(reg_intern(&reg, "beta", &os2) == 0);

	assert(os1 == 1);
	assert(os2 == 1 + strlen("alpha") + 1);
	assert(os1 != os2);

	assert(strcmp(reg_get(&reg, os1), "alpha") == 0);
	assert(strcmp(reg_get(&reg, os2), "beta") == 0);

	reg_free(&reg);
}

static void test_deduplication() {
	struct entity_registry reg;

	reg_init(&reg);

	char buf1[] = "ROOM_A";
	char buf2[] = "ROOM_A";

	size_t os1, os2;

	assert(reg_intern(&reg, buf1, &os1) == 0);
	size_t bytes_after_fist = reg.pool_used;
	assert(reg_intern(&reg, buf2, &os2) == 0);

	assert(os1 == os2);
	assert(bytes_after_fist == reg.pool_used);

	reg_free(&reg);
}

static void test_edge_cases() {
	struct entity_registry reg;

	reg_init(&reg);

	size_t os;
	int res = reg_intern(&reg, "", &os);
	assert(res != 0);

	assert(reg_intern(NULL, "test", &os) != 0);
	assert(reg_intern(&reg, NULL, &os) != 0);

	reg_free(&reg);
}

static void test_capacity_limits() {
	struct entity_registry reg;

	reg_init(&reg);

	char exact_max[REG_STR_MAX_LEN + 1];
	memset(exact_max, 'a', REG_STR_MAX_LEN);
	exact_max[REG_STR_MAX_LEN] = '\0';
	
	size_t os;
	assert(reg_intern(&reg, exact_max, &os) == 0); /* Success */

	char one_too_log[REG_STR_MAX_LEN + 2];
	memset(one_too_log, 'b', REG_STR_MAX_LEN + 1);
	one_too_log[REG_STR_MAX_LEN + 1] = '\0';

	assert(reg_intern(&reg, one_too_log, &os) != 0); /* Fail */

	char unique_str[32];
	int counter = 0;
	while (1) {
		snprintf(unique_str, sizeof(unique_str), "item_%d", counter++);
		int rs = reg_intern(&reg, unique_str, &os);
		if (rs != 0) break;
	}
	
	assert(reg.pool_used <= REG_POOL_MAX_SIZE);

	reg_free(&reg);
}

int main(void) {

	test_basic_insert_and_get();
	test_deduplication();
	test_edge_cases();
	test_capacity_limits();

	return 0;
}
