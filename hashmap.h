#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 *  CONFIG (powers of two)
 * ============================================================ */

#define MAX_CLASS   32
#define MAX_VAR     2*128

#define CLASS_MAP_SIZE  2*(MAX_CLASS - 1)
#define VAR_MAP_SIZE    2*MAX_VAR
#define ADDR_MAP_SIZE   2*MAX_VAR

#define INVALID_INDEX   0xFFFFu

/* ============================================================
 *  MAP STATE
 * ============================================================ */

typedef enum {
    MAP_EMPTY = 0,
    MAP_USED,
    MAP_TOMBSTONE
} map_state_t;

/* ============================================================
 *  CREATE FLAGS (caller-controlled)
 * ============================================================ */

typedef enum {
    MAPF_NONE  = 0,
    MAPF_CLASS = 1u << 0,
    MAPF_VAR   = 1u << 1,
    MAPF_ADDR  = 1u << 2
} map_create_flags_t;

/* ============================================================
 *  INIT
 * ============================================================ */

void dm_maps_init(void);

/* ============================================================
 *  CLASS MAP : class_name → class_idx
 * ============================================================ */

uint16_t dm_class_map_find(const char *class_name);

/* prepare (stage only; not visible) */
void dm_class_map_prepare(const char *class_name, uint16_t class_idx);

/* commit staged entry */
void dm_class_map_commit(void);

/* tombstone */
bool dm_class_map_delete(const char *class_name);

/* ============================================================
 *  VAR MAP : (class_idx, var_name) → var_idx
 * ============================================================ */

uint16_t dm_var_map_find(uint16_t class_idx, const char *var_name,const char *var_type);

void dm_var_map_prepare(uint16_t class_idx,
                        const char *var_name,
                        uint16_t    var_idx);

void dm_var_map_commit(void);

bool dm_var_map_delete(uint16_t class_idx, const char *var_name);

/* ============================================================
 *  ADDR MAP : ext_addr → var_idx
 * ============================================================ */

uint16_t dm_addr_map_find(const void *addr);

void dm_addr_map_prepare(const void *addr, uint16_t var_idx);

void dm_addr_map_commit(void);

bool dm_addr_map_delete(const void *addr);

/* ============================================================
 *  GLOBAL COMMIT
 * ============================================================ */

/* Commits only the maps indicated by flags */
void dm_maps_commit_all(map_create_flags_t flags);

#endif /* HASHMAP_H */
