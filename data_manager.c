#include "data_manager.h"
#include "hashmap.h"
#include "json_telemetry.h"
#include "schema.h"   /* effective_class_pool_size / effective_var_pool_size */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  GLOBAL STATE
 * ============================================================ */

class_t *head = DM_PTR_NULL;
class_t *tail = DM_PTR_NULL;
bool     dirty = false;

class_t  class_pool[MAX_CLASS_POOL_CAP];
bool     used_class[MAX_CLASS_POOL_CAP];

var_t    var_pool[MAX_VAR_POOL_CAP];
bool     used_var[MAX_VAR_POOL_CAP];

/* Runtime pool-size accessors — declared in hashmap.h, defined here */
int32_t dm_max_class(void) { return effective_class_pool_size(); }
int32_t dm_max_var(void)   { return effective_var_pool_size(); }

/* Guard: skip dirty on metaData class when settings_json_includes.Metadata == false */
static void class_mark_dirty(class_t *cls)
{
    if (!settings_json_includes.Metadata && cls->class_name &&
        strcmp(cls->class_name, "metaData") == 0)
        return;
    cls->dirty = true;
}

uint16_t last_class_idx = 0;
uint16_t last_var_idx   = 0;

void dm_system_init(void)
{
    /* initialise hashmaps */
    dm_maps_init();

    /*
    // clear used flags 
    for (uint16_t i = 0; i < MAX_CLASS; i++) {
        used_class[i] = false;
        class_pool[i] = CLASS_INIT(i);
    }

    for (uint16_t i = 0; i < MAX_VAR; i++) {
        used_var[i] = false;
        var_pool[i] = VAR_INIT(i);
    }*/

    head = DM_PTR_NULL;
    tail = DM_PTR_NULL;
    dirty = false;


    /* initialise cursors */
    last_class_idx = 0;   /* classes start at 1 */
    last_var_idx   = 0;   /* vars start at 0 */
}

void insert_list_item(uint16_t class_idx,uint16_t var_idx)
{
    /* --------------------------------------------------------
     * INSERT CLASS (GLOBAL LIST)
     * -------------------------------------------------------- */
    /*if (class_idx == 0) {

        class_t *new_cls = &class_pool[last_class_idx - 1];

        // empty list 
        if (head == DM_PTR_NULL ) {
            tail = new_cls;

            new_cls->next = DM_PTR_NULL;
        }
        else{
        // non-empty list 
        //class_t *old_head = parent->head;
        new_cls->next = head;
        head->prev = new_cls;
        }
        
        new_cls->prev = DM_PTR_NULL;
        
        head = new_cls;
        
        new_cls->head = new_cls;
        new_cls->tail = new_cls;
        dirty = true;
        // parent->prev (tail/LRU) unchanged 

        return;
    }*/

    /* --------------------------------------------------------
     * INSERT VARIABLE (CLASS VAR LIST)
     * -------------------------------------------------------- */
    //else {
        class_t *parent = &class_pool[class_idx];
        var_t   *new_var = &var_pool[var_idx];

        /* empty var list */
        if (parent->head == DM_PTR_NULL) {
            parent->tail = new_var;

            new_var->next = DM_PTR_NULL;
        }else{
            /* non-empty list */
        var_t *old_head = parent->head;
        old_head->prev = new_var;
        new_var->next = old_head;
        }
        if (parent->tail == DM_PTR_NULL) {
            parent->tail = new_var;
        }
        new_var->prev = DM_PTR_NULL;    
        parent->head = new_var;
        
        used_var[var_idx] = true;
        class_mark_dirty(parent);
        dirty = true;
        /* parent->var_tail unchanged */

        return;

}
void remove_from_list(uint16_t class_idx, uint16_t var_idx)
{
    class_t *parent = &class_pool[class_idx];
    var_t   *node   = &var_pool[var_idx];

    var_t *prev = node->prev;
    var_t *next = node->next;

    /* --------------------------------------------------------
     * UPDATE HEAD
     * -------------------------------------------------------- */
    if (parent->head == node) {
        parent->head = next;
    }

    /* --------------------------------------------------------
     * UPDATE TAIL
     * -------------------------------------------------------- */
    if (parent->tail == node) {
        parent->tail = prev;
    }

    /* --------------------------------------------------------
     * STITCH NEIGHBORS
     * -------------------------------------------------------- */
    if (prev != DM_PTR_NULL) {
        prev->next = next;
    }
    if (next != DM_PTR_NULL) {
        next->prev = prev;
    }

    /* --------------------------------------------------------
     * CLEAR NODE LINKS
     * -------------------------------------------------------- */
    node->prev = DM_PTR_NULL;
    node->next = DM_PTR_NULL;

    /* --------------------------------------------------------
     * DIRTY FLAGS
     * -------------------------------------------------------- */
    used_var[var_idx] = false;
    class_mark_dirty(parent);
    dirty = true;
}

bool remove_variable(void *ext_addr)
{
    /* --------------------------------------------------------
     * RESOLVE VARIABLE INDEX (O(1))
     * -------------------------------------------------------- */
    uint16_t var_idx = dm_addr_map_find(ext_addr);
    if (var_idx == INVALID_INDEX) {
        //Serial.println("❌ remove_variable_by_addr: addr not found");
        return false;
    }

    if (!used_var[var_idx]) {
        //Serial.println("❌ remove_variable_by_addr: var not active");
        return false;
    }

    var_t   *v   = &var_pool[var_idx];
    class_t *cls = &class_pool[v->class_idx];

    /* --------------------------------------------------------
     * REMOVE FROM CLASS LIST
     * -------------------------------------------------------- */
    remove_from_list(v->class_idx, var_idx);

    /* --------------------------------------------------------
     * REMOVE FROM JSON TELEMETRY
     * -------------------------------------------------------- */
    json_remove_var(var_idx);

    /* --------------------------------------------------------
     * REMOVE FROM HASHMAPS
     * -------------------------------------------------------- */
    dm_var_map_delete(v->class_idx, v->var_name, v->var_type);
    dm_addr_map_delete(ext_addr);

    /* --------------------------------------------------------
     * MARK DIRTY
     * -------------------------------------------------------- */
    class_mark_dirty(cls);
    dirty = true;

    /*Serial.print("🗑️ Removed var: ");
    Serial.print(cls->class_name);
    Serial.print("::");
    Serial.println(v->var_name);*/

    return true;
}


bool update_variable(void *ext_addr)
{

    /* --------------------------------------------------------
     * RESOLVE VARIABLE INDEX
     * -------------------------------------------------------- */
    uint16_t var_idx = dm_addr_map_find(ext_addr);

    if (var_idx == INVALID_INDEX) {
        //printf("[update_variable] ERROR: addr not registered\n");
        return false;
    }

    var_t   *v   = &var_pool[var_idx];
    class_t *cls = &class_pool[v->class_idx];

    /* --------------------------------------------------------
     * READ CURRENT VALUE FROM EXTERNAL MEMORY
     * -------------------------------------------------------- */
    float current_val = *((float *)v->ext_addr);
    


    /* --------------------------------------------------------
     * CHECK FOR ACTUAL CHANGE
     * -------------------------------------------------------- */
    if (v->cached_val == current_val) {
        //printf("[update_variable] no change (cached=%f)\n",             v->cached_val);
        return true;
    }

    /* --------------------------------------------------------
     * UPDATE CACHED VALUE
     * -------------------------------------------------------- */
    //printf("[update_variable] resolved: class='%s' var='%s'\n",         cls->class_name ? cls->class_name : "(null)",      v->var_name     ? v->var_name     : "(null)");

    //printf("[update_variable] value change %f -> %f\n",          v->cached_val, current_val);

    v->cached_val = current_val;

    /* --------------------------------------------------------
     * MOVE TO MRU POSITION
     * -------------------------------------------------------- */
    //printf("[update_variable] moving to MRU\n");

    remove_from_list(v->class_idx, var_idx);
    insert_list_item(v->class_idx, var_idx);
    //printf("Tail Pointing @ Address   : %u\n", cls->tail->var_idx);

    return true;
}



bool dm_set_value(const variables_description_row_t *row, void *ext_addr)
{
    const char *class_name     = row->Class;
    const char *var_name       = row->Name;
    const char *type           = row->Type;
    float       value          = row->Value;
    uint16_t    constraint_idx = (row->constraint_id >= 0)
                                 ? (uint16_t)row->constraint_id
                                 : INVALID_INDEX;

    map_create_flags_t flags = MAPF_NONE;

    /* --------------------------------------------------------
     * CLASS LOOKUP / CREATE
     * -------------------------------------------------------- */
    uint16_t class_idx = dm_class_map_find(class_name);

    if (class_idx == INVALID_INDEX) {
        if (last_class_idx >= MAX_CLASS)
            return false;

        class_idx = last_class_idx++;

        class_pool[class_idx] = CLASS_INIT(class_idx);
        class_pool[class_idx].class_name = class_name;
        used_class[class_idx] = true;

        /* insert into global class circular list (MRU at head) */
        class_t *new_cls = &class_pool[last_class_idx - 1];

        /* empty list */
        if (head == DM_PTR_NULL ) {
            tail = new_cls;

            new_cls->next = DM_PTR_NULL;
        }
        else{
        /* non-empty list */
        //class_t *old_head = parent->head;
        new_cls->next = head;
        head->prev = new_cls;
        }
        
        new_cls->prev = DM_PTR_NULL;
        
        head = new_cls;
        
        //new_cls->head = new_cls;
        //new_cls->tail = new_cls;
        dirty = true;
        
        dm_class_map_prepare(class_name, class_idx);
        flags |= MAPF_CLASS;
        //printf("----- CLASS CREATED -----\n");
    }
    else{
        //printf("----- CLASS PRESENT -----\n");
    }
    

    class_t *cls = &class_pool[class_idx];
    //printf("Class Index   : %u\n", cls->class_idx);
    //printf("Class Name    : %s\n",         cls->class_name ? cls->class_name : "(null)");
    /* --------------------------------------------------------
     * VAR LOOKUP / CREATE
     * -------------------------------------------------------- */
    uint16_t var_idx = dm_var_map_find(class_idx, var_name,type);

    if (var_idx == INVALID_INDEX) {
        if (last_var_idx >= MAX_VAR)
            return false;

        var_idx = last_var_idx++;

        var_pool[var_idx] = VAR_INIT(var_idx);
        var_pool[var_idx].var_name       = var_name;
        var_pool[var_idx].class_idx      = class_idx;
        var_pool[var_idx].constraint_idx = constraint_idx;
        var_pool[var_idx].ext_addr       = ext_addr;
        var_pool[var_idx].var_type       = type;
        var_pool[var_idx].cached_val     = value;

        //used_var[var_idx] = true;

        /* insert into class variable circular list (MRU at head) */
        /*
        if (cls->var_head == NULL) {
            cls->var_head = cls->var_tail = &var_pool[var_idx];
            var_pool[var_idx].prev = &var_pool[var_idx];
            var_pool[var_idx].next = &var_pool[var_idx];
        } else {
            var_t *h = cls->var_head;
            var_t *t = cls->var_tail;

            var_pool[var_idx].next = h;
            var_pool[var_idx].prev = t;
            h->prev = &var_pool[var_idx];
            t->next = &var_pool[var_idx];
            cls->var_head = &var_pool[var_idx];
        }
	*/
	insert_list_item(class_idx,var_idx);
        dm_var_map_prepare(class_idx, var_name, type, var_idx);
        dm_addr_map_prepare(ext_addr, var_idx);
        flags |= MAPF_VAR | MAPF_ADDR;
      /* --------------------------------------------------------
     * ATOMIC PUBLISH
     * -------------------------------------------------------- */
        dm_maps_commit_all(flags);	
        //printf("----- VARIABLE CREATED -----\n");
    }
    else{
        //printf("----- VARIABLE PRESENT -----\n");
    }
    var_t *v = &var_pool[var_idx];
    //printf("Var Index     : %u\n", v->var_idx);
    //printf("Var Name      : %s\n",         v->var_name ? v->var_name : "(null)");
    //printf("Ext Address   : %p\n", v->ext_addr);
    //printf("Cached Type   : %s\n",        v->var_type ? v->var_type : "(null)");
    //printf("Tail Pointing @ Address   : %u\n", cls->tail->var_idx);	
    																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																										
    update_variable(ext_addr);
    //sync_class(class_idx);
    /* --------------------------------------------------------
     * UPDATE VALUE (ALWAYS SAFE)
     * -------------------------------------------------------- */
    /*
    var_t *v = &var_pool[var_idx];

    if (v->cached_val != value) {
        v->cached_val = value;
        v->cached_type = type_str;
        cls->dirty = true;
        class_pool[0].dirty = true;
    }

    */

    return true;
}
void get_value(void *ext_addr)
{
    /* --------------------------------------------------------
     * RESOLVE VARIABLE VIA ADDRESS MAP
     * -------------------------------------------------------- */
    uint16_t var_idx = dm_addr_map_find(ext_addr);

    if (var_idx == INVALID_INDEX) {
        //printf("ERROR: variable not registered for addr %p\n", ext_addr);
        return;
    }

    var_t   *v   = &var_pool[var_idx];
    class_t *cls = &class_pool[v->class_idx];

    /* --------------------------------------------------------
     * PRINT VARIABLE DATA
     * -------------------------------------------------------- */
    //printf("----- VARIABLE DATA -----\n");

    //printf("Class Index   : %u\n", cls->class_idx);
    //printf("Class Name    : %s\n",          cls->class_name ? cls->class_name : "(null)");

    //printf("Var Index     : %u\n", v->var_idx);
    //printf("Var Name      : %s\n",          v->var_name ? v->var_name : "(null)");

    //printf("Cached Value  : %f\n", v->cached_val);
    //printf("Cached Type   : %s\n",        v->var_type ? v->var_type : "(null)");

    //printf("Ext Address   : %p\n", v->ext_addr);

    //printf("Prev Pointer  : %p\n", (void *)v->prev);
    //printf("Next Pointer  : %p\n", (void *)v->next);

    //printf("-------------------------\n");
}
void get_class_values(const char *class_name)
{
    /* --------------------------------------------------------
     * RESOLVE CLASS
     * -------------------------------------------------------- */
    uint16_t class_idx = dm_class_map_find(class_name);

    if (class_idx == INVALID_INDEX) {
        //printf("ERROR: class '%s' not found\n", class_name);
        return;
    }

    class_t *cls = &class_pool[class_idx];

    /* --------------------------------------------------------
     * TRAVERSE VARIABLES
     * -------------------------------------------------------- */
    //printf("===== CLASS '%s' VALUES =====\n", class_name);

    var_t *cur = cls->head;

    if (cur == DM_PTR_NULL) {
        //printf("(no variables)\n");
        return;
    }

    while (cur != DM_PTR_NULL) {
        //printf("----- VARIABLE DATA -----\n");

        //printf("Var Index     : %u\n", cur->var_idx);
        //printf("Var Name      : %s\n",            cur->var_name ? cur->var_name : "(null)");

        //printf("Cached Value  : %f\n", cur->cached_val);
        //printf("Cached Type   : %s\n",              cur->var_type ? cur->var_type : "(null)");

        //printf("Ext Address   : %p\n", cur->ext_addr);

        //printf("Prev Pointer  : %p\n", (void *)cur->prev);
        //printf("Next Pointer  : %p\n", (void *)cur->next);

        //printf("-------------------------\n");

        cur = cur->next;
    }
}
void sync_class(uint16_t class_idx)
{
    class_t *cls = &class_pool[class_idx];

    /* --------------------------------------------------------
     * CHECK DIRTY FLAG
     * -------------------------------------------------------- */
    if (!cls->dirty) {
        // printf("[sync_class] class '%s' is clean\n",
        //        cls->class_name ? cls->class_name : "(null)");
        return;
    }

    /* --------------------------------------------------------
     * PROCESS CURRENT TAIL NODE
     * -------------------------------------------------------- */
    var_t *node = cls->tail;

    if (node == DM_PTR_NULL) {
        /* ----------------------------------------------------
         * DONE: ALL VARIABLES PROCESSED
         * ---------------------------------------------------- */
        cls->dirty = false;

        // printf("[sync_class] class '%s' fully synced, sending JSON\n",
        //        cls->class_name ? cls->class_name : "(null)");

        json_send();
        return;
    }

    // printf("[sync_class] var '%s' idx=%u value=%f\n",
    //        node->var_name ? node->var_name : "(null)",
    //        node->var_idx,
    //        node->cached_val);

    /* --------------------------------------------------------
     * ADD VARIABLE TO JSON
     * -------------------------------------------------------- */
    json_add_var(node->var_idx);

    /* --------------------------------------------------------
     * MOVE TAIL BACKWARD (STATEFUL)
     * -------------------------------------------------------- */
    cls->tail = node->prev;

    /* --------------------------------------------------------
     * RECURSE TO PROCESS NEXT VARIABLE
     * -------------------------------------------------------- */
    sync_class(class_idx);
}
void sync_all_classVars(uint16_t class_idx)
{
    class_t *cls = &class_pool[class_idx];

    /* --------------------------------------------------------
     * START FROM HEAD (MRU)
     * -------------------------------------------------------- */
    var_t *node = cls->head;

    if (node == DM_PTR_NULL) {
        // Serial.println("[sync_all_classVars] no variables");
        return;
    }

    // Serial.print("[sync_all_classVars] syncing class: ");
    // Serial.println(cls->class_name);

    /* --------------------------------------------------------
     * WALK HEAD → TAIL (READ-ONLY)
     * -------------------------------------------------------- */
    while (node != DM_PTR_NULL) {

        // Serial.print("  var idx=");
        // Serial.print(node->var_idx);
        // Serial.print(" name=");
        // Serial.print(node->var_name);
        // Serial.print(" val=");
        // Serial.println(node->cached_val);

        json_add_var(node->var_idx);

        /* move forward */
        node = node->next;
    }

    /* --------------------------------------------------------
     * SEND JSON (FULL SNAPSHOT OF CLASS)
     * -------------------------------------------------------- */
    json_send();
}

void sync_all_internal(bool noChange)
{
    /* --------------------------------------------------------
     * START FROM GLOBAL LRU (READ-ONLY)
     * -------------------------------------------------------- */
    class_t *node = tail;

    if (node == DM_PTR_NULL) {
        // Serial.println("[sync_all] no classes");
        return;
    }

    // Serial.println("[sync_all] starting global sync");

    /* --------------------------------------------------------
     * WALK LRU → MRU (TAIL → HEAD)
     * -------------------------------------------------------- */
    while (node != DM_PTR_NULL) {

        // Serial.print("[sync_all] class=");
        // Serial.print(node->class_name);
        // Serial.print(" dirty=");
        // Serial.println(node->dirty);

        if (node->dirty) {
            if (noChange) {
                /* snapshot sync (no list mutation) */
                sync_all_classVars(node->class_idx);
            } else {
                /* delta sync (moves var tail internally) */
                sync_class(node->class_idx);
            }
        }

        /* move backward in global class list */
        node = node->prev;
    }

    // Serial.println("[sync_all] global sync done");
}
/* --------------------------------------------------------
 * PUBLIC WRAPPERS
 * -------------------------------------------------------- */
void sync_all(void)
{
    sync_all_internal(false);
}

void sync_all_nochange(void)
{
    sync_all_internal(true);
}
