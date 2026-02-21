#include "hashmap.h"
#include <string.h>
#include <stdint.h>

/* ============================================================
 *  HASH FUNCTIONS (fast, deterministic)
 * ============================================================ */

static uint32_t hash_str(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h;
}

static uint32_t hash_ptr(const void *p)
{
    uintptr_t x = (uintptr_t)p;
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    return (uint32_t)x;
}

/* ============================================================
 *  MAP ENTRY TYPES
 * ============================================================ */

typedef struct {
    const char *key;
    uint16_t    idx;
    map_state_t state;
} class_map_entry_t;

typedef struct {
    uint16_t    class_idx;
    const char *var_name;
    uint16_t    idx;
    map_state_t state;
} var_map_entry_t;

typedef struct {
    const void *addr;
    uint16_t    idx;
    map_state_t state;
} addr_map_entry_t;

/* ============================================================
 *  TABLES
 * ============================================================ */

static class_map_entry_t class_map[CLASS_MAP_SIZE];
static var_map_entry_t   var_map[VAR_MAP_SIZE];
static addr_map_entry_t  addr_map[ADDR_MAP_SIZE];

/* ============================================================
 *  STAGING (single-entry per map, deterministic)
 * ============================================================ */

static class_map_entry_t class_stage;
static bool class_stage_valid;

static var_map_entry_t var_stage;
static bool var_stage_valid;

static addr_map_entry_t addr_stage;
static bool addr_stage_valid;

/* ============================================================
 *  INIT
 * ============================================================ */

void dm_maps_init(void)
{
    memset(class_map, 0, sizeof(class_map));
    memset(var_map,   0, sizeof(var_map));
    memset(addr_map,  0, sizeof(addr_map));

    class_stage_valid = false;
    var_stage_valid   = false;
    addr_stage_valid  = false;
}

/* ============================================================
 *  CLASS MAP
 * ============================================================ */

uint16_t dm_class_map_find(const char *class_name)
{
    uint32_t b = hash_str(class_name) & (CLASS_MAP_SIZE - 1);

    for (uint32_t i = 0; i < CLASS_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (CLASS_MAP_SIZE - 1);
        if (class_map[p].state == MAP_EMPTY)
            return INVALID_INDEX;
        if (class_map[p].state == MAP_USED &&
            strcmp(class_map[p].key, class_name) == 0)
            return class_map[p].idx;
    }
    return INVALID_INDEX;
}

void dm_class_map_prepare(const char *class_name, uint16_t class_idx)
{
    class_stage.key   = class_name;
    class_stage.idx   = class_idx;
    class_stage.state = MAP_USED;
    class_stage_valid = true;
}

void dm_class_map_commit(void)
{
    if (!class_stage_valid)
        return;

    uint32_t b = hash_str(class_stage.key) & (CLASS_MAP_SIZE - 1);

    for (uint32_t i = 0; i < CLASS_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (CLASS_MAP_SIZE - 1);
        if (class_map[p].state != MAP_USED) {
            class_map[p] = class_stage;
            break;
        }
    }
    class_stage_valid = false;
}

bool dm_class_map_delete(const char *class_name)
{
    uint32_t b = hash_str(class_name) & (CLASS_MAP_SIZE - 1);

    for (uint32_t i = 0; i < CLASS_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (CLASS_MAP_SIZE - 1);
        if (class_map[p].state == MAP_EMPTY)
            return false;
        if (class_map[p].state == MAP_USED &&
            strcmp(class_map[p].key, class_name) == 0) {
            class_map[p].state = MAP_TOMBSTONE;
            return true;
        }
    }
    return false;
}

/* ============================================================
 *  VAR MAP
 * ============================================================ */

uint16_t dm_var_map_find(uint16_t class_idx,
                         const char *var_name,
                         const char *var_type)
{
    char key[64];

    size_t n1 = strlen(var_name);
    size_t n2 = strlen(var_type);

    if (n1 + 1 + n2 + 1 > sizeof(key))
        return INVALID_INDEX;

    memcpy(key, var_name, n1);
    key[n1] = '_';
    memcpy(key + n1 + 1, var_type, n2);
    key[n1 + 1 + n2] = '\0';

    uint32_t h = hash_str(key) ^ class_idx;
    uint32_t b = h & (VAR_MAP_SIZE - 1);

    for (uint32_t i = 0; i < VAR_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (VAR_MAP_SIZE - 1);

        if (var_map[p].state == MAP_EMPTY)
            return INVALID_INDEX;

        if (var_map[p].state == MAP_USED &&
            var_map[p].class_idx == class_idx &&
            strcmp(var_map[p].var_name, key) == 0)
            return var_map[p].idx;
    }

    return INVALID_INDEX;
}


void dm_var_map_prepare(uint16_t class_idx,
                        const char *var_name,
                        uint16_t    var_idx)
{
    var_stage.class_idx = class_idx;
    var_stage.var_name  = var_name;
    var_stage.idx       = var_idx;
    var_stage.state     = MAP_USED;
    var_stage_valid     = true;
}

void dm_var_map_commit(void)
{
    if (!var_stage_valid)
        return;

    uint32_t h = hash_str(var_stage.var_name) ^ var_stage.class_idx;
    uint32_t b = h & (VAR_MAP_SIZE - 1);

    for (uint32_t i = 0; i < VAR_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (VAR_MAP_SIZE - 1);
        if (var_map[p].state != MAP_USED) {
            var_map[p] = var_stage;
            break;
        }
    }
    var_stage_valid = false;
}

bool dm_var_map_delete(uint16_t class_idx, const char *var_name)
{
    uint32_t h = hash_str(var_name) ^ class_idx;
    uint32_t b = h & (VAR_MAP_SIZE - 1);

    for (uint32_t i = 0; i < VAR_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (VAR_MAP_SIZE - 1);
        if (var_map[p].state == MAP_EMPTY)
            return false;
        if (var_map[p].state == MAP_USED &&
            var_map[p].class_idx == class_idx &&
            strcmp(var_map[p].var_name, var_name) == 0) {
            var_map[p].state = MAP_TOMBSTONE;
            return true;
        }
    }
    return false;
}

/* ============================================================
 *  ADDR MAP
 * ============================================================ */

uint16_t dm_addr_map_find(const void *addr)
{
    uint32_t b = hash_ptr(addr) & (ADDR_MAP_SIZE - 1);

    for (uint32_t i = 0; i < ADDR_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (ADDR_MAP_SIZE - 1);
        if (addr_map[p].state == MAP_EMPTY)
            return INVALID_INDEX;
        if (addr_map[p].state == MAP_USED &&
            addr_map[p].addr == addr)
            return addr_map[p].idx;
    }
    return INVALID_INDEX;
}

void dm_addr_map_prepare(const void *addr, uint16_t var_idx)
{
    addr_stage.addr  = addr;
    addr_stage.idx   = var_idx;
    addr_stage.state = MAP_USED;
    addr_stage_valid = true;
}

void dm_addr_map_commit(void)
{
    if (!addr_stage_valid)
        return;

    uint32_t b = hash_ptr(addr_stage.addr) & (ADDR_MAP_SIZE - 1);

    for (uint32_t i = 0; i < ADDR_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (ADDR_MAP_SIZE - 1);
        if (addr_map[p].state != MAP_USED) {
            addr_map[p] = addr_stage;
            break;
        }
    }
    addr_stage_valid = false;
}

bool dm_addr_map_delete(const void *addr)
{
    uint32_t b = hash_ptr(addr) & (ADDR_MAP_SIZE - 1);

    for (uint32_t i = 0; i < ADDR_MAP_SIZE; i++) {
        uint32_t p = (b + i) & (ADDR_MAP_SIZE - 1);
        if (addr_map[p].state == MAP_EMPTY)
            return false;
        if (addr_map[p].state == MAP_USED &&
            addr_map[p].addr == addr) {
            addr_map[p].state = MAP_TOMBSTONE;
            return true;
        }
    }
    return false;
}

/* ============================================================
 *  GLOBAL COMMIT
 * ============================================================ */

void dm_maps_commit_all(map_create_flags_t flags)
{
    if (flags & MAPF_CLASS)
        dm_class_map_commit();
    if (flags & MAPF_VAR)
        dm_var_map_commit();
    if (flags & MAPF_ADDR)
        dm_addr_map_commit();
}
