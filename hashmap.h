#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 *  CONFIG (powers of two)
 * ============================================================ */

/* Compile-time pool capacity — single source of truth for all array sizing */
#define MAX_CLASS_POOL_CAP  64
#define MAX_VAR_POOL_CAP    256

/* Hashmap table sizes: 2× the pool caps (open-addressing load factor) */
#define CLASS_MAP_SIZE  (2 * MAX_CLASS_POOL_CAP)
#define VAR_MAP_SIZE    (2 * MAX_VAR_POOL_CAP)
#define ADDR_MAP_SIZE   (2 * MAX_VAR_POOL_CAP)

/* Runtime-effective pool sizes — wired to settings_schema via data_manager.c */
int32_t dm_max_class(void);
int32_t dm_max_var(void);

/* These now return the runtime-configured values (fall back to caps if unset) */
#define MAX_CLASS   dm_max_class()
#define MAX_VAR     dm_max_var()

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

#ifdef __cplusplus
extern "C" {
#endif

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

uint16_t dm_var_map_find(uint16_t class_idx, const char *var_name, const char *var_type);

void dm_var_map_prepare(uint16_t class_idx,
                        const char *var_name,
                        const char *var_type,
                        uint16_t    var_idx);

void dm_var_map_commit(void);

bool dm_var_map_delete(uint16_t class_idx, const char *var_name, const char *var_type);

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

/* ============================================================
 *  FAULT-CODE MAP : float Fault_Code → const char *Message
 *
 *  Built from metadata_table rows where Class == "Fault_Code".
 *  Call am_fault_map_build() (alert_manager.h) after each
 *  load_metadata_from_spiffs() to keep the LUT current.
 * ============================================================ */

#define FAULT_MAP_SIZE  128   /* power of 2; must be ≥ 2× distinct fault codes */

void        am_fault_map_clear(void);
void        am_fault_map_insert(float fault_code, const char *message);
const char *am_fault_map_find(float fault_code);

#ifdef __cplusplus
}
#endif

#endif /* HASHMAP_H */
