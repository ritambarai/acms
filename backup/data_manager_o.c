#include "data_manager.h"
#include <string.h>

/* ============================================================
 *  HASH FUNCTIONS
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

static uint32_t hash_class_var(uint32_t cid, const char *name)
{
    return hash_str(name) ^ (cid * 2654435761u);
}

/* ============================================================
 *  HASHMAP INFRA
 * ============================================================ */

typedef enum {
    MAP_EMPTY = 0,
    MAP_USED,
    MAP_TOMBSTONE
} map_state_t;

/* ---------- CLASS MAP ---------- */

typedef struct {
    const char *key;
    uint32_t    val;
    map_state_t state;
} class_map_entry_t;

static class_map_entry_t class_map[CLASS_MAP_SIZE];

/* ---------- VAR MAP ---------- */

typedef struct {
    uint32_t    class_id;
    const char *var_name;
} var_key_t;

typedef struct {
    var_key_t   key;
    uint32_t    val; /* var_id */
    map_state_t state;
} var_map_entry_t;

static var_map_entry_t var_map[VAR_MAP_SIZE];

/* ---------- ADDR MAP ---------- */

typedef struct {
    const void *key;
    uint32_t    val; /* var_id */
    map_state_t state;
} addr_map_entry_t;

static addr_map_entry_t addr_map[ADDR_MAP_SIZE];

/* ============================================================
 *  OBJECT STORAGE
 * ============================================================ */

typedef struct {
    uint32_t var_id;
    uint32_t class_id;
    int32_t *ext_addr;
    int32_t  cached;
    uint16_t prev, next;
} dm_var_t;

typedef struct {
    uint32_t class_id;
    bool     used;
    bool     dirty;
    uint16_t var_head;
    uint16_t var_tail;
    uint16_t prev, next;
} dm_class_t;

static dm_var_t   vars[VM_MAX_VARS];
static bool       var_used[VM_MAX_VARS];
static dm_class_t classes[VM_MAX_CLASSES];

/* ============================================================
 *  ID → INDEX ACCELERATION
 * ============================================================ */

typedef struct {
    uint32_t id;
    uint16_t index;
    bool     used;
} id_index_t;

static id_index_t var_id_index[VM_MAX_VARS];
static id_index_t class_id_index[VM_MAX_CLASSES];

/* ============================================================
 *  GLOBAL STATE
 * ============================================================ */

static uint16_t class_head = VM_INVALID_INDEX;
static uint16_t class_tail = VM_INVALID_INDEX;
static bool     global_dirty;

/* ============================================================
 *  ID → INDEX HELPERS
 * ============================================================ */

static dm_var_t *var_by_id(uint32_t var_id)
{
    for (uint16_t i = 0; i < VM_MAX_VARS; i++)
        if (var_id_index[i].used && var_id_index[i].id == var_id)
            return &vars[var_id_index[i].index];
    return NULL;
}

static dm_class_t *class_by_id(uint32_t class_id)
{
    for (uint16_t i = 0; i < VM_MAX_CLASSES; i++)
        if (class_id_index[i].used && class_id_index[i].id == class_id)
            return &classes[class_id_index[i].index];
    return NULL;
}

/* ============================================================
 *  HASHMAP LOOKUPS
 * ============================================================ */

static uint32_t class_map_find(const char *name)
{
    uint32_t idx = hash_str(name) & (CLASS_MAP_SIZE - 1);
    for (uint32_t i = 0; i < CLASS_MAP_SIZE; i++) {
        uint32_t p = (idx + i) & (CLASS_MAP_SIZE - 1);
        if (class_map[p].state == MAP_EMPTY)
            return VM_INVALID_ID;
        if (class_map[p].state == MAP_USED &&
            strcmp(class_map[p].key, name) == 0)
            return class_map[p].val;
    }
    return VM_INVALID_ID;
}

static uint32_t var_map_find(uint32_t cid, const char *name)
{
    uint32_t idx = hash_class_var(cid, name) & (VAR_MAP_SIZE - 1);
    for (uint32_t i = 0; i < VAR_MAP_SIZE; i++) {
        uint32_t p = (idx + i) & (VAR_MAP_SIZE - 1);
        if (var_map[p].state == MAP_EMPTY)
            return VM_INVALID_ID;
        if (var_map[p].state == MAP_USED &&
            var_map[p].key.class_id == cid &&
            strcmp(var_map[p].key.var_name, name) == 0)
            return var_map[p].val;
    }
    return VM_INVALID_ID;
}

static uint32_t addr_map_find(const void *addr)
{
    uint32_t idx = hash_ptr(addr) & (ADDR_MAP_SIZE - 1);
    for (uint32_t i = 0; i < ADDR_MAP_SIZE; i++) {
        uint32_t p = (idx + i) & (ADDR_MAP_SIZE - 1);
        if (addr_map[p].state == MAP_EMPTY)
            return VM_INVALID_ID;
        if (addr_map[p].state == MAP_USED &&
            addr_map[p].key == addr)
            return addr_map[p].val;
    }
    return VM_INVALID_ID;
}

/* ============================================================
 *  CLASS CREATION
 * ============================================================ */

static dm_class_t *get_or_create_class(const char *name)
{
    uint32_t cid = class_map_find(name);
    if (cid != VM_INVALID_ID)
        return class_by_id(cid);

    for (uint16_t i = 0; i < VM_MAX_CLASSES; i++) {
        if (!classes[i].used) {
            uint32_t new_id = hash_str(name);
            classes[i] = (dm_class_t){
                .class_id = new_id,
                .used = true,
                .dirty = false,
                .var_head = VM_INVALID_INDEX,
                .var_tail = VM_INVALID_INDEX,
                .prev = i,
                .next = i
            };

            class_id_index[i] = (id_index_t){
                .id = new_id,
                .index = i,
                .used = true
            };

            class_map_insert(name, new_id);

            if (class_head == VM_INVALID_INDEX)
                class_head = class_tail = i;

            return &classes[i];
        }
    }
    return NULL;
}

/* ============================================================
 *  PUBLIC API
 * ============================================================ */

void dm_init(void)
{
    memset(class_map, 0, sizeof(class_map));
    memset(var_map,   0, sizeof(var_map));
    memset(addr_map,  0, sizeof(addr_map));
    memset(classes,   0, sizeof(classes));
    memset(vars,      0, sizeof(vars));
    memset(var_used,  0, sizeof(var_used));
    memset(var_id_index,   0, sizeof(var_id_index));
    memset(class_id_index, 0, sizeof(class_id_index));

    class_head = class_tail = VM_INVALID_INDEX;
    global_dirty = false;
}

bool dm_register_var(const char *class_name,
                     const char *var_name,
                     int32_t    *ext_addr)
{
    dm_class_t *cls = get_or_create_class(class_name);
    if (!cls)
        return false;

    if (addr_map_find(ext_addr) != VM_INVALID_ID)
        return true;

    for (uint16_t i = 0; i < VM_MAX_VARS; i++) {
        if (!var_used[i]) {
            uint32_t vid = hash_class_var(cls->class_id, var_name);

            var_used[i] = true;
            vars[i] = (dm_var_t){
                .var_id = vid,
                .class_id = cls->class_id,
                .ext_addr = ext_addr,
                .cached = *ext_addr
            };

            var_id_index[i] = (id_index_t){
                .id = vid,
                .index = i,
                .used = true
            };

            if (cls->var_head == VM_INVALID_INDEX) {
                cls->var_head = cls->var_tail = i;
                vars[i].prev = vars[i].next = i;
            } else {
                vars[i].prev = cls->var_tail;
                vars[i].next = cls->var_head;
                vars[cls->var_tail].next = i;
                vars[cls->var_head].prev = i;
                cls->var_head = i;
            }

            return true;
        }
    }
    return false;
}

bool dm_update_by_addr(int32_t *ext_addr, int32_t new_val)
{
    uint32_t vid = addr_map_find(ext_addr);
    if (vid == VM_INVALID_ID)
        return false;

    dm_var_t *v = var_by_id(vid);
    if (!v || v->cached == new_val)
        return true;

    v->cached = new_val;
    *v->ext_addr = new_val;

    dm_class_t *cls = class_by_id(v->class_id);
    if (cls)
        cls->dirty = true;

    global_dirty = true;
    return true;
}

/* ============================================================
 *  CURSOR-BASED SYNC
 * ============================================================ */

static void sync_vars(dm_class_t *cls)
{
    if (!cls->dirty)
        return;

    while (1) {
        /* emit vars[cls->var_tail].cached here */

        if (cls->var_tail == cls->var_head) {
            cls->dirty = false;
            return;
        }
        cls->var_tail = vars[cls->var_tail].prev;
    }
}

void dm_sync(void)
{
    if (!global_dirty)
        return;

    while (1) {
        dm_class_t *cls = &classes[class_tail];
        if (cls->dirty)
            sync_vars(cls);

        if (class_tail == class_head) {
            global_dirty = false;
            return;
        }
        class_tail = classes[class_tail].prev;
    }
}

bool dm_global_changed(void)
{
    return global_dirty;
}

void dm_clear_global_changed(void)
{
    global_dirty = false;
}

