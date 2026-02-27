#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "hashmap.h"   /* INVALID_INDEX */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  CAPACITY AND DEDUPLICATION
 * ============================================================ */

#define MAX_ALERT_QUEUE  64   /* ring-buffer capacity */


/* ============================================================
 *  ALERT ENTRY STRUCTURE
 * ============================================================ */

typedef struct {
    uint16_t    alert_idx;      /* slot index in alert_table.alerts[]           */
    uint16_t    var_idx;        /* var_pool index that triggered this alert      */

    /* identity — owned copies so pool reloads cannot produce dangling pointers */
    char        class_name[32];
    char        var_name[32];
    char        var_type[16];

    uint16_t    constraint_id;  /* index into variables_constraints_table        */
    float       value;          /* var value at the moment the constraint fired  */
    float       threshold;      /* threshold value that was violated             */
    float       fault_code;     /* fault code from constraints row               */
    float       operation_id;   /* operation id from constraints row             */
    char        fault_message[128]; /* looked up from metadata Fault_Code class  */

    time_t      timestamp;      /* Unix epoch at enqueue time (for dedup/diff)   */
    char        datetime[32];   /* UTC timestamp "YYYY-MM-DDTHH:MM:SSZ"          */
} alert_t;

/* ============================================================
 *  INITIALISER MACRO  (mirrors CLASS_INIT / VAR_INIT)
 * ============================================================ */

/* char[] fields are zero-initialised by the C99 compound-literal default;
 * no explicit initialiser needed for them. */
#define ALERT_INIT(idx) ((alert_t){       \
    .alert_idx     = (idx),               \
    .var_idx       = INVALID_INDEX,       \
    .constraint_id = INVALID_INDEX,       \
    .value         = 0.0f,               \
    .threshold     = 0.0f,               \
    .fault_code    = 0.0f,               \
    .operation_id  = 0.0f,               \
    .timestamp     = 0,                   \
})

/* ============================================================
 *  ALERT TABLE  (circular ring buffer)
 *
 *  head  : index of the oldest unprocessed alert (next to publish)
 *  tail  : index of the most recently written alert (last valid slot)
 *  count : number of pending alerts (0 = empty, MAX_ALERT_QUEUE = full)
 *
 *  Initial state: head=0, tail=MAX_ALERT_QUEUE-1, count=0
 *   → first enqueue lands in slot 0
 *
 *  Empty : count == 0
 *  Full  : count == MAX_ALERT_QUEUE
 *  Overflow: on full, both head and tail advance — oldest is overwritten
 * ============================================================ */

typedef struct {
    alert_t  alerts[MAX_ALERT_QUEUE];
    uint16_t head;    /* oldest unprocessed slot (next to publish) */
    uint16_t tail;    /* most recently written slot                */
    uint16_t count;   /* number of pending entries                 */
} alert_table_t;

extern alert_table_t alert_table;
extern uint32_t      alert_log_count;  /* total entries ever appended to /alert_log.jsonl */

/* ============================================================
 *  INITIALISATION
 *
 *  After alert_system_init():
 *    - Every alerts[i] = ALERT_INIT(i)
 *    - head=0, tail=MAX_ALERT_QUEUE-1, count=0
 *    - NTP started (UTC, pool.ntp.org)
 *    - /alert_log.jsonl present in SPIFFS (created if absent)
 *    - alert_mutex created
 * ============================================================ */

void alert_system_init(void);

/* ============================================================
 *  RING-BUFFER OPERATIONS (low-level)
 * ============================================================ */

/* Enqueue a pre-built alert (all fields caller-filled except datetime).
 * Returns slot index on success, INVALID_INDEX if mutex unavailable. */
uint16_t am_enqueue_alert(uint16_t    var_idx,
                          uint16_t    constraint_id,
                          float       value,
                          float       threshold,
                          float       fault_code,
                          float       operation_id,
                          const char *class_name,
                          const char *var_name,
                          const char *var_type,
                          const char *fault_message);

/* Dequeue the oldest alert (advances head, decrements count).
 * Returns slot index on success, INVALID_INDEX if empty. */
uint16_t am_dequeue_alert(void);

/* ============================================================
 *  PERSISTENCE
 * ============================================================ */

/* Append alert_table.alerts[alert_idx] as one JSONL line to
 * /alert_log.jsonl in SPIFFS.  Keys match alert_t field names. */
void am_log_alert(uint16_t alert_idx);

/* ============================================================
 *  UTILITY
 * ============================================================ */

/* Fill buf with the current UTC time as "YYYY-MM-DDTHH:MM:SSZ".
 * Falls back to "1970-01-01T00:00:00Z" if NTP is not yet synced. */
void am_now(char *buf, size_t len);

/* ============================================================
 *  FAULT-CODE LUT
 * ============================================================ */

/* Rebuild the fault-code → message LUT from metadata_table rows
 * where Class == "Fault_Code".  Call after each load_metadata_from_spiffs(). */
void am_fault_map_build(void);

/* ============================================================
 *  HIGH-LEVEL ALERT CREATION
 * ============================================================ */

/* Build an alert from var_pool[var_id] and
 * variables_constraints_table.rows[constraint_id]:
 *   1. Snapshot var value + constraint fields
 *   2. Look up fault message via am_fault_map_find()
 *   3. Stamp datetime from NTP
 *   4. Append to /alert_log.jsonl  (always — even on overflow)
 *   5. Enqueue in alert_table; on overflow both head and tail advance */
void add_alert_queue(uint16_t var_id, uint16_t constraint_id);

/* ============================================================
 *  MQTT PUBLISH TASK
 * ============================================================ */

/* Start the FreeRTOS task that drains alert_table → MQTT.
 * Pinned to Core 1, priority 2.  Call once from acms_system_init(). */
void alert_mqtt_task_start(void);

#ifdef __cplusplus
}
#endif

#endif /* ALERT_MANAGER_H */
