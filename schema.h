#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "hashmap.h"

/* ═══════ Metadata ═══════ */

typedef enum {
  COL_METADATA_CLASS_STRING,
  COL_METADATA_KEY_FLOAT,
  COL_METADATA_MESSAGE_STRING,
} metadata_col_type_t;

#define METADATA_COL_COUNT 3

static const metadata_col_type_t metadata_column_types[METADATA_COL_COUNT] = {
  COL_METADATA_CLASS_STRING,
  COL_METADATA_KEY_FLOAT,
  COL_METADATA_MESSAGE_STRING,
};

static const char *metadata_column_names[METADATA_COL_COUNT] = {
  "Class", "Key", "Message"
};

typedef struct {
  char* Class;
  float Key;
  char* Message;
} metadata_row_t;

#define MAX_METADATA_ROWS MAX_VAR_POOL_CAP

typedef struct {
  metadata_row_t rows[MAX_METADATA_ROWS];
  int count;
  int version;
} metadata_table_t;

extern metadata_table_t metadata_table;

static inline bool validate_metadata_value(metadata_col_type_t type, const char *v) {
  if (!v || v[0] == '\0') return true;
  switch (type) {
    case COL_METADATA_KEY_FLOAT:
      return isfinite(atof(v));
    default:
      return true;
  }
}


/* ═══════ Variables ═══════ */

/* ─── Variables / description ─── */

typedef enum {
  COL_VARIABLES_DESCRIPTION_CLASS_STRING,
  COL_VARIABLES_DESCRIPTION_NAME_STRING,
  COL_VARIABLES_DESCRIPTION_TYPE_STRING,
  COL_VARIABLES_DESCRIPTION_VALUE_FLOAT,
} variables_description_col_type_t;

#define VARIABLES_DESCRIPTION_COL_COUNT 4

static const variables_description_col_type_t variables_description_column_types[VARIABLES_DESCRIPTION_COL_COUNT] = {
  COL_VARIABLES_DESCRIPTION_CLASS_STRING,
  COL_VARIABLES_DESCRIPTION_NAME_STRING,
  COL_VARIABLES_DESCRIPTION_TYPE_STRING,
  COL_VARIABLES_DESCRIPTION_VALUE_FLOAT,
};

static const char *variables_description_column_names[VARIABLES_DESCRIPTION_COL_COUNT] = {
  "Class", "Name", "Type", "Value"
};

typedef struct {
  char* Class;
  char* Name;
  char* Type;
  float Value;
  int constraint_id;   /* index into constraints table, -1 if no constraints row */
} variables_description_row_t;

#define MAX_VARIABLES_DESCRIPTION_ROWS MAX_VAR_POOL_CAP

typedef struct {
  variables_description_row_t rows[MAX_VARIABLES_DESCRIPTION_ROWS];
  int count;
  int version;
} variables_description_table_t;

extern variables_description_table_t variables_description_table;

static inline bool validate_variables_description_value(variables_description_col_type_t type, const char *v) {
  if (!v || v[0] == '\0') return true;
  switch (type) {
    case COL_VARIABLES_DESCRIPTION_VALUE_FLOAT:
      return isfinite(atof(v));
    default:
      return true;
  }
}


/* ─── Variables / modbus ─── */

typedef enum {
  COL_VARIABLES_MODBUS_SLAVE_ID_FLOAT,
  COL_VARIABLES_MODBUS_FUNCTION_ID_FLOAT,
  COL_VARIABLES_MODBUS_START_ADDRESS_FLOAT,
  COL_VARIABLES_MODBUS_DATA_LENGTH_FLOAT,
} variables_modbus_col_type_t;

#define VARIABLES_MODBUS_COL_COUNT 4

static const variables_modbus_col_type_t variables_modbus_column_types[VARIABLES_MODBUS_COL_COUNT] = {
  COL_VARIABLES_MODBUS_SLAVE_ID_FLOAT,
  COL_VARIABLES_MODBUS_FUNCTION_ID_FLOAT,
  COL_VARIABLES_MODBUS_START_ADDRESS_FLOAT,
  COL_VARIABLES_MODBUS_DATA_LENGTH_FLOAT,
};

static const char *variables_modbus_column_names[VARIABLES_MODBUS_COL_COUNT] = {
  "Slave_ID", "Function_ID", "Start_Address", "Data_Length"
};

typedef struct {
  float Slave_ID;
  float Function_ID;
  float Start_Address;
  float Data_Length;
  float *value_ptr;   /* points to description struct Value field */
} variables_modbus_row_t;

#define MAX_VARIABLES_MODBUS_ROWS MAX_VAR_POOL_CAP

typedef struct {
  variables_modbus_row_t rows[MAX_VARIABLES_MODBUS_ROWS];
  int count;
  int version;
} variables_modbus_table_t;

extern variables_modbus_table_t variables_modbus_table;

static inline bool validate_variables_modbus_value(variables_modbus_col_type_t type, const char *v) {
  if (!v || v[0] == '\0') return true;
  switch (type) {
    case COL_VARIABLES_MODBUS_SLAVE_ID_FLOAT:
    case COL_VARIABLES_MODBUS_FUNCTION_ID_FLOAT:
    case COL_VARIABLES_MODBUS_START_ADDRESS_FLOAT:
    case COL_VARIABLES_MODBUS_DATA_LENGTH_FLOAT:
      return isfinite(atof(v));
    default:
      return true;
  }
}


/* ─── Variables / constraints ─── */

typedef enum {
  COL_VARIABLES_CONSTRAINTS_OPERATION_ID_FLOAT,
  COL_VARIABLES_CONSTRAINTS_THRESHOLD_FLOAT,
  COL_VARIABLES_CONSTRAINTS_FAULT_CODE_FLOAT,
  COL_VARIABLES_CONSTRAINTS_INCREMENT_FLOAT,
} variables_constraints_col_type_t;

#define VARIABLES_CONSTRAINTS_COL_COUNT 4

static const variables_constraints_col_type_t variables_constraints_column_types[VARIABLES_CONSTRAINTS_COL_COUNT] = {
  COL_VARIABLES_CONSTRAINTS_OPERATION_ID_FLOAT,
  COL_VARIABLES_CONSTRAINTS_THRESHOLD_FLOAT,
  COL_VARIABLES_CONSTRAINTS_FAULT_CODE_FLOAT,
  COL_VARIABLES_CONSTRAINTS_INCREMENT_FLOAT,
};

static const char *variables_constraints_column_names[VARIABLES_CONSTRAINTS_COL_COUNT] = {
  "Operation_ID", "Threshold", "Fault_Code", "Increment"
};

typedef struct {
  float Operation_ID;
  float Threshold;
  float Fault_Code;
  float Increment;
  float *value_ptr;   /* points to description struct Value field */
  int constraints_id;  /* next constraints row for this variable, -1 = none */
} variables_constraints_row_t;

#define MAX_VARIABLES_CONSTRAINTS_ROWS MAX_VAR_POOL_CAP

typedef struct {
  variables_constraints_row_t rows[MAX_VARIABLES_CONSTRAINTS_ROWS];
  int count;
  int version;
} variables_constraints_table_t;

extern variables_constraints_table_t variables_constraints_table;

static inline bool validate_variables_constraints_value(variables_constraints_col_type_t type, const char *v) {
  if (!v || v[0] == '\0') return true;
  switch (type) {
    case COL_VARIABLES_CONSTRAINTS_OPERATION_ID_FLOAT:
    case COL_VARIABLES_CONSTRAINTS_THRESHOLD_FLOAT:
    case COL_VARIABLES_CONSTRAINTS_FAULT_CODE_FLOAT:
    case COL_VARIABLES_CONSTRAINTS_INCREMENT_FLOAT:
      return isfinite(atof(v));
    default:
      return true;
  }
}


/* ═══════ Settings ═══════ */

/* ─── Settings / general ─── */

typedef struct {
  char* SSID;
  char* Password;
  int32_t Class_Pool_Size;
  int32_t Var_Pool_Size;
  float Alert_cooldown;
} settings_general_t;

extern settings_general_t settings_general;


/* ─── Settings / mqtt ─── */

typedef struct {
  char* Host;
  int32_t Port;
  char* Data_Topic;
  char* Alert_Topic;
  char* Mqtt_Username;
  char* Mqtt_Password;
} settings_mqtt_t;

extern settings_mqtt_t settings_mqtt;


/* ─── Settings / json includes ─── */

typedef struct {
  bool Metadata;
  bool Constraints;
  bool Type_Unit;
} settings_json_includes_t;

extern settings_json_includes_t settings_json_includes;


/* ── Runtime pool-size accessors (defaults when struct value is 0) ── */

static inline int32_t effective_var_pool_size(void) {
  int32_t v = settings_general.Var_Pool_Size;
  if (v <= 0)              v = MAX_VAR_POOL_CAP;
  if (v > MAX_VAR_POOL_CAP) v = MAX_VAR_POOL_CAP;
  return v;
}

static inline int32_t effective_class_pool_size(void) {
  int32_t v = settings_general.Class_Pool_Size;
  if (v <= 0)                v = MAX_CLASS_POOL_CAP;
  if (v > MAX_CLASS_POOL_CAP) v = MAX_CLASS_POOL_CAP;
  return v;
}

static inline float effective_alert_cooldown(void) {
  /* 0.0f = no dedup  |  > 0.0f = minutes  |  < 0.0f = always suppress */
  return settings_general.Alert_cooldown;
}
