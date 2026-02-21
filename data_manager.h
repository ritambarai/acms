#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "hashmap.h"

/* ============================================================
 *  POINTER SENTINEL
 * ============================================================ */

#define DM_PTR_NULL      ((void *)0)

/* ============================================================
 *  FORWARD DECLARATIONS
 * ============================================================ */

struct class_s;
struct var_s;

/* ============================================================
 *  CLASS STRUCTURE
 * ============================================================ */

/*
 * NOTE:
 *  - class_pool[0] is RESERVED as GLOBAL STATE
 *  - class_pool[0].dirty == GLOBAL DIRTY FLAG
 *  - prev/next/var_head/var_tail unused for idx 0
 */
typedef struct class_s {
    /* identity */
    const char *class_name;
    uint16_t    class_idx;

    /* state */
    bool        dirty;        /* idx 0 → global dirty */

    /* global class circular list */
    struct class_s *prev;
    struct class_s *next;

    /* per-class variable circular list */
    struct var_s   *head;
    struct var_s   *tail;
} class_t;

/* ============================================================
 *  VARIABLE STRUCTURE
 * ============================================================ */

typedef struct var_s {
    /* identity */
    const char *var_name;
    const char *var_type;
    uint16_t    var_idx;

    /* ownership */
    uint16_t    class_idx;

    /* constraint table row (INVALID_INDEX if none) */
    uint16_t    constraint_idx;

    /* data */
    int32_t    *ext_addr;
    float     cached_val;

    /* per-class variable circular list */
    struct var_s *prev;
    struct var_s *next;
} var_t;

/* ============================================================
 *  INITIALISER MACROS (MANDATORY)
 * ============================================================ */

/*
 * These MUST be used to initialise pool entries.
 * Pointer members are explicitly initialised to NULL.
 */

#define CLASS_INIT(idx)  ((class_t){      \
    .class_name = NULL,                   \
    .class_idx  = (idx),                  \
    .dirty      = false,                  \
    .prev       = DM_PTR_NULL,            \
    .next       = DM_PTR_NULL,            \
    .head   = DM_PTR_NULL,                \
    .tail   = DM_PTR_NULL                 \
})

#define VAR_INIT(idx)    ((var_t){        \
    .var_name       = NULL,               \
    .var_idx        = (idx),              \
    .class_idx      = 0,                  \
    .constraint_idx = INVALID_INDEX,      \
    .ext_addr       = NULL,               \
    .cached_val     = 0,                  \
    .var_type       = NULL,               \
    .prev           = DM_PTR_NULL,        \
    .next           = DM_PTR_NULL         \
})

/* ============================================================
 *  GLOBAL POOLS (DEFINED IN data_manager.c)
 * ============================================================ */

extern class_t class_pool[MAX_CLASS_CAP];
extern bool    used_class[MAX_CLASS_CAP];

extern var_t   var_pool[MAX_VAR_CAP];
extern bool    used_var[MAX_VAR_CAP];

/* ============================================================
 *  GLOBAL CURSORS (MONOTONIC, NEVER REUSED)
 * ============================================================ */

extern uint16_t last_class_idx;
extern uint16_t last_var_idx;
extern class_t *head;
extern class_t *tail;
extern bool dirty;

/* ============================================================
 *  INITIALISATION CONTRACT
 * ============================================================ */

/*
 * After system init:
 *
 *  - class_pool[0] = CLASS_INIT(0)
 *  - used_class[0] = true
 *  - last_class_idx = 1
 *  - last_var_idx   = 0
 *
 *  - All other pool entries are initialised using
 *    CLASS_INIT(i) / VAR_INIT(i) before use.
 *
 *  - prev/next/var_head/var_tail are pointer-based links.
 *  - Indices are NEVER reused.
 */
 void dm_system_init(void);

bool dm_set_value(const char *class_name,
                  const char *type,
                  const char *var_name,
                  void       *ext_addr,
                  float       value,
                  uint16_t    constraint_idx);

void get_value(void *ext_addr);
void get_class_values(const char *class_name);
bool update_variable(void *ext_addr);
void sync_class(uint16_t class_idx);
void sync_all_classVars(uint16_t class_idx);
void sync_all(void);
void sync_all_nochange(void);
bool remove_variable(void *ext_addr);
#endif /* DATA_MANAGER_H */
