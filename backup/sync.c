void sync_class(uint16_t class_idx)
{
    class_t *cls = &class_pool[class_idx];

    /* --------------------------------------------------------
     * CHECK DIRTY FLAG
     * -------------------------------------------------------- 
    if (!cls->dirty) {
        //printf("[sync_class] class '%s' is clean, nothing to do\n",             cls->class_name ? cls->class_name : "(null)");
        return;
    }
    static bool json_started = false;
    /*
    if (!json_started) {
        json_begin_class(cls->class_name);
        json_started = true;
    }*/

    /* --------------------------------------------------------
     * START FROM TAIL (LRU)
     * -------------------------------------------------------- 
    var_t *node = cls->tail;
    /*
    if (node == DM_PTR_NULL) {
        /* No variables 
        //printf("[sync_class] class '%s' has no variables\n",              cls->class_name ? cls->class_name : "(null)");
        cls->dirty = false;
        return;
    }*/

    //printf("[sync_class] syncing class '%s'\n",       cls->class_name ? cls->class_name : "(null)");

    /* --------------------------------------------------------
     * PROCESS ONE NODE
     * -------------------------------------------------------- */
    //printf("  [sync_class] var '%s' value=%f\n",     node->var_name ? node->var_name : "(null)",        node->cached_val);
    
    //foo(node);
    //json_add_var(node->var_idx);

    /* --------------------------------------------------------
     * MOVE TAIL BACKWARD
     * -------------------------------------------------------- */
    cls->tail = node->prev;

    /* --------------------------------------------------------
     * RECURSE OR FINISH
     * -------------------------------------------------------- */
    while (cls->tail != DM_PTR_NULL) {
        //printf("Tail Pointing @ Address   : %u\n", cls->tail->var_idx);
        json_add_var(node->var_idx);
        sync_class(class_idx);
        return;
    }

    /* --------------------------------------------------------
     * DONE: MARK CLASS CLEAN
     * -------------------------------------------------------- */
    cls->dirty = false;
    json_started = false;
    json_send();
    //printf("[sync_class] class '%s' fully synced, marked clean\n",          cls->class_name ? cls->class_name : "(null)");
}
