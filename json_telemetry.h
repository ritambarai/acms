#ifndef JSON_TELEMETRY_H
#define JSON_TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include "hashmap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_REMOTE_VARS MAX_VAR_POOL_CAP

/* --------------------------------------------------------
 * JSON BUILDING (USED BY DATA MANAGER SYNC)
 * -------------------------------------------------------- */

/* Add a variable (by var_idx) to JSON */
void json_add_var(uint16_t var_idx);

/* Remove a variable (by var_idx) from JSON */
void json_remove_var(uint16_t var_idx);

/* Send current JSON to data topic */
void json_send(void);

/* Send a constraint-fired alert to the alert topic.
 * dual mode  → Alert_Topic on dedicated alert client
 * single mode→ Data_Topic on shared client             */
void json_send_alert(const char *class_name,
                     const char *var_name,
                     float       value,
                     float       fault_code,
                     float       threshold,
                     float       operation_id);

void json_receive(void);

/* --------------------------------------------------------
 * TELEMETRY COMMANDS (SERVER / UI CONTROL)
 * -------------------------------------------------------- */

/* Remove variable (validated + JSON + server response) */
bool delete_variable(uint16_t var_idx,
                     const char *var_name,
                     const char *class_name);

/* Update variable (validated + data_manager + sync + response) */
bool update_variable_telemetry(uint16_t var_idx,
                               const char *var_name,
                               const char *class_name,
                               float       new_value);

#ifdef __cplusplus
}
#endif

#endif /* JSON_TELEMETRY_H */
