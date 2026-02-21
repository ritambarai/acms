#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 *  CONFIGURATION
 * ============================================================ */

#define VM_INVALID_ID      0xFFFFFFFFu
#define VM_INVALID_INDEX   0xFFFFu

#define VM_MAX_CLASSES     32
#define VM_MAX_VARS        128

#define CLASS_MAP_SIZE     64
#define VAR_MAP_SIZE       256
#define ADDR_MAP_SIZE      256

/* ============================================================
 *  INIT
 * ============================================================ */

void dm_init(void);

/* ============================================================
 *  REGISTRATION
 * ============================================================ */

bool dm_register_var(const char *class_name,
                     const char *var_name,
                     int32_t    *ext_addr);

/* ============================================================
 *  UPDATE (FAST PATH)
 * ============================================================ */

bool dm_update_by_addr(int32_t *ext_addr, int32_t new_val);

/* ============================================================
 *  SYNC
 * ============================================================ */

void dm_sync(void);

/* ============================================================
 *  STATUS
 * ============================================================ */

bool dm_global_changed(void);
void dm_clear_global_changed(void);

#endif /* DATA_MANAGER_H */

