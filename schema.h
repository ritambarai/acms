#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ═══════ Metadata ═══════ */

typedef enum {
  COL_METADATA_KEY_FLOAT,
  COL_METADATA_MESSAGE_STRING,
  COL_METADATA_CLASS_STRING,
} metadata_col_type_t;

#define METADATA_COL_COUNT 3

static const metadata_col_type_t metadata_column_types[METADATA_COL_COUNT] = {
  COL_METADATA_KEY_FLOAT,
  COL_METADATA_MESSAGE_STRING,
  COL_METADATA_CLASS_STRING,
};

static const char *metadata_column_names[METADATA_COL_COUNT] = {
  "Key", "Message", "Class"
};

typedef struct {
  float Key;
  char* Message;
  char* Class;
} metadata_row_t;

#define MAX_METADATA_ROWS 128

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
} variables_description_col_type_t;

#define VARIABLES_DESCRIPTION_COL_COUNT 3

static const variables_description_col_type_t variables_description_column_types[VARIABLES_DESCRIPTION_COL_COUNT] = {
  COL_VARIABLES_DESCRIPTION_CLASS_STRING,
  COL_VARIABLES_DESCRIPTION_NAME_STRING,
  COL_VARIABLES_DESCRIPTION_TYPE_STRING,
};

static const char *variables_description_column_names[VARIABLES_DESCRIPTION_COL_COUNT] = {
  "Class", "Name", "Type"
};

typedef struct {
  char* Class;
  char* Name;
  char* Type;
} variables_description_row_t;

#define MAX_VARIABLES_DESCRIPTION_ROWS 128

typedef struct {
  variables_description_row_t rows[MAX_VARIABLES_DESCRIPTION_ROWS];
  int count;
  int version;
} variables_description_table_t;

extern variables_description_table_t variables_description_table;

static inline bool validate_variables_description_value(variables_description_col_type_t type, const char *v) {
  if (!v || v[0] == '\0') return true;
  return true;
}


/* ─── Variables / values ─── */

typedef enum {
  COL_VARIABLES_VALUES_VALUE_FLOAT,
  COL_VARIABLES_VALUES_OPERATION_ID_FLOAT,
  COL_VARIABLES_VALUES_THRESHOLD_FLOAT,
  COL_VARIABLES_VALUES_FAULT_CODE_FLOAT,
  COL_VARIABLES_VALUES_INCREMENT_FLOAT,
} variables_values_col_type_t;

#define VARIABLES_VALUES_COL_COUNT 5

static const variables_values_col_type_t variables_values_column_types[VARIABLES_VALUES_COL_COUNT] = {
  COL_VARIABLES_VALUES_VALUE_FLOAT,
  COL_VARIABLES_VALUES_OPERATION_ID_FLOAT,
  COL_VARIABLES_VALUES_THRESHOLD_FLOAT,
  COL_VARIABLES_VALUES_FAULT_CODE_FLOAT,
  COL_VARIABLES_VALUES_INCREMENT_FLOAT,
};

static const char *variables_values_column_names[VARIABLES_VALUES_COL_COUNT] = {
  "Value", "Operation_ID", "Threshold", "Fault_Code", "Increment"
};

typedef struct {
  float Value;
  float Operation_ID;
  float Threshold;
  float Fault_Code;
  float Increment;
} variables_values_row_t;

#define MAX_VARIABLES_VALUES_ROWS 128

typedef struct {
  variables_values_row_t rows[MAX_VARIABLES_VALUES_ROWS];
  int count;
  int version;
} variables_values_table_t;

extern variables_values_table_t variables_values_table;

static inline bool validate_variables_values_value(variables_values_col_type_t type, const char *v) {
  if (!v || v[0] == '\0') return true;
  switch (type) {
    case COL_VARIABLES_VALUES_VALUE_FLOAT:
    case COL_VARIABLES_VALUES_OPERATION_ID_FLOAT:
    case COL_VARIABLES_VALUES_THRESHOLD_FLOAT:
    case COL_VARIABLES_VALUES_FAULT_CODE_FLOAT:
    case COL_VARIABLES_VALUES_INCREMENT_FLOAT:
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
  float *value_ptr;   /* points to values struct Value field */
} variables_modbus_row_t;

#define MAX_VARIABLES_MODBUS_ROWS 128

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

