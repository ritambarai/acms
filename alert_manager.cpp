/*
 * alert_manager.cpp
 *
 * Alert ring buffer with NTP-stamped entries and SPIFFS JSONL persistence.
 *
 * Design
 * ──────
 *  alert_table  : circular ring buffer
 *                   head  = oldest unprocessed slot (next to publish)
 *                   tail  = most recently written slot
 *                   count = pending entries (0 = empty, MAX = full)
 *                 On overflow: both head and tail advance; oldest is overwritten.
 *  alert_mutex  : FreeRTOS mutex guarding head/tail/count access
 *  NTP          : started in alert_system_init() (UTC, no DST)
 *  alert_log    : /alert_log.jsonl — one JSON object per line, written on enqueue
 *  alert_mqtt_task : FreeRTOS task that drains the ring buffer to MQTT;
 *                    advances head only after a successful publish
 */

#include <Arduino.h>
#include <SPIFFS.h>
#include <time.h>
#include <string.h>

#include "alert_manager.h"
#include "data_manager.h"       /* var_pool, class_pool, used_var              */
#include "schema.h"             /* metadata_table, variables_constraints_table  */
#include "network_manager.h"    /* mqtt_manager_publish_alert, connected_alert  */

/* ============================================================
 *  GLOBAL STATE
 * ============================================================ */

alert_table_t alert_table;

static SemaphoreHandle_t alert_mutex = NULL;

/* ============================================================
 *  UTILITY — current UTC time string
 * ============================================================ */

void am_now(char *buf, size_t len)
{
    struct tm t;
    if (getLocalTime(&t, 0)) {         /* 0 ms — non-blocking, false if unsynced */
        strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &t);
    } else {
        snprintf(buf, len, "1970-01-01T00:00:00Z");
    }
}

/* ============================================================
 *  INITIALISATION
 * ============================================================ */

void alert_system_init(void)
{
    /* ── 1. Initialise every slot in the ring buffer ── */
    for (uint16_t i = 0; i < MAX_ALERT_QUEUE; i++) {
        alert_table.alerts[i] = ALERT_INIT(i);
    }
    /* tail = MAX-1 so that the first enqueue lands in slot 0 */
    alert_table.head  = 0;
    alert_table.tail  = MAX_ALERT_QUEUE - 1;
    alert_table.count = 0;

    /* ── 2. Create mutex ── */
    alert_mutex = xSemaphoreCreateMutex();
    if (!alert_mutex) {
        Serial.println("[Alert] ERROR: could not create alert_mutex");
    }

    /* ── 3. Start NTP (UTC) ── */
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[NTP] Sync started (UTC)");

    /* ── 4. Ensure /alert_log.jsonl exists in SPIFFS ── */
    if (!SPIFFS.exists("/alert_log.jsonl")) {
        File f = SPIFFS.open("/alert_log.jsonl", "w");
        if (f) {
            f.close();
            Serial.println("[Alert] Created /alert_log.jsonl");
        } else {
            Serial.println("[Alert] ERROR: could not create /alert_log.jsonl");
        }
    } else {
        Serial.println("[Alert] /alert_log.jsonl found");
    }
}

/* ============================================================
 *  PERSISTENCE  — shared internal helper
 * ============================================================ */

static void log_alert_ptr(const alert_t *a)
{
    char line[512];
    snprintf(line, sizeof(line),
        "{"
        "\"alert_idx\":%u,"
        "\"var_idx\":%u,"
        "\"class_name\":\"%s\","
        "\"var_name\":\"%s\","
        "\"var_type\":\"%s\","
        "\"constraint_id\":%u,"
        "\"value\":%.2f,"
        "\"threshold\":%.2f,"
        "\"fault_code\":%.0f,"
        "\"fault_message\":\"%s\","
        "\"datetime\":\"%s\""
        "}\n",
        a->alert_idx,
        a->var_idx,
        a->class_name    ? a->class_name    : "",
        a->var_name      ? a->var_name      : "",
        a->var_type      ? a->var_type      : "",
        a->constraint_id,
        a->value,
        a->threshold,
        a->fault_code,
        a->fault_message ? a->fault_message : "",
        a->datetime);

    File f = SPIFFS.open("/alert_log.jsonl", "a");
    if (!f) {
        Serial.println("[Alert] ERROR: could not open /alert_log.jsonl for append");
        return;
    }
    f.print(line);
    f.close();

    Serial.printf("[Alert] Logged idx=%u  fault=%.0f  %s/%s  val=%.2f  %s\n",
                  a->alert_idx, a->fault_code,
                  a->class_name ? a->class_name : "?",
                  a->var_name   ? a->var_name   : "?",
                  a->value, a->datetime);
}

void am_log_alert(uint16_t alert_idx)
{
    if (alert_idx >= MAX_ALERT_QUEUE) return;
    log_alert_ptr(&alert_table.alerts[alert_idx]);
}

/* ============================================================
 *  LOW-LEVEL RING-BUFFER OPERATIONS
 * ============================================================ */

uint16_t am_enqueue_alert(uint16_t    var_idx,
                          uint16_t    constraint_id,
                          float       value,
                          float       threshold,
                          float       fault_code,
                          float       operation_id,
                          const char *class_name,
                          const char *var_name,
                          const char *var_type,
                          const char *fault_message)
{
    if (!alert_mutex) return INVALID_INDEX;
    if (xSemaphoreTake(alert_mutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return INVALID_INDEX;

    uint16_t slot = (alert_table.tail + 1) % MAX_ALERT_QUEUE;

    if (alert_table.count == MAX_ALERT_QUEUE) {
        /* Full — advance head to drop oldest */
        alert_table.head = (alert_table.head + 1) % MAX_ALERT_QUEUE;
    } else {
        alert_table.count++;
    }
    alert_table.tail = slot;

    alert_table.alerts[slot] = ALERT_INIT(slot);
    alert_table.alerts[slot].var_idx        = var_idx;
    alert_table.alerts[slot].constraint_id  = constraint_id;
    alert_table.alerts[slot].value          = value;
    alert_table.alerts[slot].threshold      = threshold;
    alert_table.alerts[slot].fault_code     = fault_code;
    alert_table.alerts[slot].operation_id   = operation_id;
    alert_table.alerts[slot].class_name     = class_name;
    alert_table.alerts[slot].var_name       = var_name;
    alert_table.alerts[slot].var_type       = var_type;
    alert_table.alerts[slot].fault_message  = fault_message;
    alert_table.alerts[slot].timestamp      = time(NULL);
    am_now(alert_table.alerts[slot].datetime, sizeof(alert_table.alerts[slot].datetime));

    xSemaphoreGive(alert_mutex);
    return slot;
}

uint16_t am_dequeue_alert(void)
{
    if (!alert_mutex) return INVALID_INDEX;
    if (xSemaphoreTake(alert_mutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return INVALID_INDEX;

    uint16_t slot = INVALID_INDEX;
    if (alert_table.count > 0) {
        slot = alert_table.head;
        alert_table.head = (alert_table.head + 1) % MAX_ALERT_QUEUE;
        alert_table.count--;
    }

    xSemaphoreGive(alert_mutex);
    return slot;
}

/* ============================================================
 *  FAULT-CODE LUT — rebuild from metadata_table
 * ============================================================ */

void am_fault_map_build(void)
{
    am_fault_map_clear();
    int built = 0;
    for (int i = 0; i < metadata_table.count; i++) {
        const metadata_row_t *r = &metadata_table.rows[i];
        if (r->Class && strcmp(r->Class, "Fault_Code") == 0 && r->Message) {
            am_fault_map_insert(r->Key, r->Message);
            built++;
        }
    }
    Serial.printf("[Alert] Fault-code LUT: %d entries built\n", built);
}

/* ============================================================
 *  HIGH-LEVEL ALERT CREATION
 * ============================================================ */

void add_alert_queue(uint16_t var_id, uint16_t constraint_id)
{
    /* ── 1. Validate inputs ── */
    if (var_id >= MAX_VAR_POOL_CAP || !used_var[var_id]) {
        Serial.printf("[Alert] add_alert_queue: invalid var_id=%u\n", var_id);
        return;
    }
    if ((int)constraint_id >= variables_constraints_table.count) {
        Serial.printf("[Alert] add_alert_queue: invalid constraint_id=%u\n", constraint_id);
        return;
    }

    const var_t *v   = &var_pool[var_id];
    const variables_constraints_row_t *cr =
        &variables_constraints_table.rows[constraint_id];
    const char *cls_name =
        (v->class_idx < MAX_CLASS_POOL_CAP) ? class_pool[v->class_idx].class_name : NULL;

    /* ── 2. Build temp alert_t ── */
    alert_t tmp = ALERT_INIT(0);            /* placeholder slot index */
    tmp.var_idx       = var_id;
    tmp.class_name    = cls_name;
    tmp.var_name      = v->var_name;
    tmp.var_type      = v->var_type;
    tmp.constraint_id = constraint_id;
    tmp.value         = v->cached_val;      /* snapshot at the moment of violation */
    tmp.threshold     = cr->Threshold;
    tmp.fault_code    = cr->Fault_Code;
    tmp.operation_id  = cr->Operation_ID;
    tmp.fault_message = am_fault_map_find(cr->Fault_Code);
    tmp.timestamp     = time(NULL);
    am_now(tmp.datetime, sizeof(tmp.datetime));

    /* ── 3. Log to JSONL (always — before touching the ring buffer) ── */
    log_alert_ptr(&tmp);

    /* ── 4. Enqueue with dedup check + circular overwrite on overflow ── */
    if (!alert_mutex) return;
    if (xSemaphoreTake(alert_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        Serial.println("[Alert] add_alert_queue: mutex timeout");
        return;
    }

    /* ── 4a. Deduplication: compare against last written alert ── */
    if (alert_table.count > 0) {
        const alert_t *last = &alert_table.alerts[alert_table.tail];
        if (last->var_idx == var_id &&
            (int32_t)last->fault_code == (int32_t)tmp.fault_code) {

            const float dedup_m = effective_alert_cooldown();
            bool suppress;

            if (dedup_m < 0.0f) {
                /* Negative → always suppress duplicates (infinite cooldown) */
                suppress = true;
            } else if (dedup_m == 0.0f) {
                /* Zero → no deduplication, every trigger enqueued */
                suppress = false;
            } else {
                float elapsed_s = (float)(tmp.timestamp - last->timestamp);
                suppress = (elapsed_s < dedup_m * 60.0f);
            }

            if (suppress) {
                xSemaphoreGive(alert_mutex);
                Serial.printf("[Alert] Dedup skipped  fault=%.0f  %s/%s\n",
                              tmp.fault_code,
                              tmp.class_name ? tmp.class_name : "?",
                              tmp.var_name   ? tmp.var_name   : "?");
                return;
            }
        }
    }

    uint16_t slot = (alert_table.tail + 1) % MAX_ALERT_QUEUE;

    if (alert_table.count == MAX_ALERT_QUEUE) {
        /* Full — both head and tail advance; oldest slot overwritten */
        alert_table.head = (alert_table.head + 1) % MAX_ALERT_QUEUE;
        Serial.printf("[Alert] Overflow — slot %u overwritten\n", slot);
    } else {
        alert_table.count++;
    }

    alert_table.tail      = slot;
    tmp.alert_idx         = slot;
    alert_table.alerts[slot] = tmp;

    xSemaphoreGive(alert_mutex);
}

/* ============================================================
 *  MQTT PUBLISH TASK
 * ============================================================ */

static void alert_publish_task(void *pvParameters)
{
    for (;;) {
        /* ── Wait until there is something to publish ── */
        if (!alert_mutex || !alert_table.count) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        /* ── Check MQTT connectivity ── */
        if (!mqtt_manager_connected_alert()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        /* ── Copy oldest alert out (minimise mutex hold time) ── */
        alert_t local;
        if (xSemaphoreTake(alert_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (alert_table.count == 0) {
            xSemaphoreGive(alert_mutex);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        local = alert_table.alerts[alert_table.head];   /* struct copy */
        xSemaphoreGive(alert_mutex);

        /* ── Build JSON payload ── */
        char payload[384];
        snprintf(payload, sizeof(payload),
            "{"
            "\"class_name\":\"%s\","
            "\"var_name\":\"%s\","
            "\"var_type\":\"%s\","
            "\"var_idx\":%u,"
            "\"constraint_id\":%u,"
            "\"value\":%.2f,"
            "\"threshold\":%.2f,"
            "\"fault_code\":%.0f,"
            "\"fault_message\":\"%s\","
            "\"datetime\":\"%s\""
            "}",
            local.class_name    ? local.class_name    : "",
            local.var_name      ? local.var_name      : "",
            local.var_type      ? local.var_type      : "",
            local.var_idx,
            local.constraint_id,
            local.value,
            local.threshold,
            local.fault_code,
            local.fault_message ? local.fault_message : "",
            local.datetime);

        /* ── Publish — retain=false (live alert, not a persistent state) ── */
        bool ok = mqtt_manager_publish_alert(payload, false);

        /* ── Advance head only on successful publish ── */
        if (ok) {
            if (xSemaphoreTake(alert_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (alert_table.count > 0) {
                    alert_table.head = (alert_table.head + 1) % MAX_ALERT_QUEUE;
                    alert_table.count--;
                }
                xSemaphoreGive(alert_mutex);
            }
            Serial.printf("[Alert] Published fault=%.0f  %s/%s\n",
                          local.fault_code,
                          local.class_name ? local.class_name : "?",
                          local.var_name   ? local.var_name   : "?");
        } else {
            /* Retry after a short back-off */
            Serial.println("[Alert] MQTT publish failed — will retry");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void alert_mqtt_task_start(void)
{
    BaseType_t rc = xTaskCreatePinnedToCore(
        alert_publish_task,
        "alert_mqtt",
        4096,           /* stack words */
        NULL,
        2,              /* priority — above idle, below increment_task */
        NULL,
        1);             /* Core 1 — same as http_task; no shared-data race with Core 0 */

    if (rc != pdPASS) {
        Serial.println("[Alert] ERROR: failed to create alert_mqtt task");
    } else {
        Serial.println("[Alert] alert_mqtt task started (Core 1)");
    }
}
