/*
 * xml_parser.c — generated from XSD
 * Parses XML from SPIFFS and populates row structs.
 */

#include "schema.h"
#include <SPIFFS.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "xml_defaults.h"

metadata_table_t metadata_table = { .count = 0, .version = 0 };
variables_description_table_t variables_description_table = { .count = 0, .version = 0 };
variables_values_table_t variables_values_table = { .count = 0, .version = 0 };
variables_modbus_table_t variables_modbus_table = { .count = 0, .version = 0 };

/* ── extract value between <tag>…</tag> or detect <tag/> ── */
static bool extract_tag(const char *xml, const char *tag, char *buf, int buflen) {
  char open[64], self_close[64], close_tag[64];
  snprintf(open, sizeof(open), "<%s>", tag);
  snprintf(self_close, sizeof(self_close), "<%s/>", tag);
  snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

  /* self-closing → empty value */
  if (strstr(xml, self_close)) { buf[0] = '\0'; return true; }

  const char *s = strstr(xml, open);
  if (!s) { buf[0] = '\0'; return false; }
  s += strlen(open);
  const char *e = strstr(s, close_tag);
  if (!e) { buf[0] = '\0'; return false; }
  int len = e - s;
  if (len >= buflen) len = buflen - 1;
  memcpy(buf, s, len);
  buf[len] = '\0';
  return true;
}


/* ═══════ Metadata: parse XML string → metadata_table ═══════ */
static int parse_metadata_xml(const char *xml, metadata_table_t *tbl) {
  int count = 0;
  const char *pos = xml;
  char buf[256];

  while (count < MAX_METADATA_ROWS) {
    const char *row_start = strstr(pos, "<row>");
    if (!row_start) break;
    const char *row_end = strstr(row_start, "</row>");
    if (!row_end) break;

    extract_tag(row_start, "Key", buf, sizeof(buf));
    if (buf[0] && !validate_metadata_value(COL_METADATA_KEY_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    tbl->rows[count].Key = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Message", buf, sizeof(buf));
    if (buf[0] && !validate_metadata_value(COL_METADATA_MESSAGE_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    tbl->rows[count].Message = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_start, "Class", buf, sizeof(buf));
    if (buf[0] && !validate_metadata_value(COL_METADATA_CLASS_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    tbl->rows[count].Class = buf[0] ? strdup(buf) : NULL;

    count++;
    pos = row_end + 6;
  }
  tbl->count = count;
  return count;
}

/* ── free dynamically allocated strings ── */
void free_metadata_table(metadata_table_t *tbl) {
  for (int i = 0; i < tbl->count; i++) {
    if (tbl->rows[i].Message) { free(tbl->rows[i].Message); tbl->rows[i].Message = NULL; }
    if (tbl->rows[i].Class) { free(tbl->rows[i].Class); tbl->rows[i].Class = NULL; }
  }
  tbl->count = 0;
}

/* ── load Metadata from SPIFFS into metadata_table ── */
int load_metadata_from_spiffs(void) {
  free_metadata_table(&metadata_table);
  File f = SPIFFS.open("/Metadata.xml", "r");
  if (!f) return 0;
  String content = f.readString();
  f.close();
  int n = parse_metadata_xml(content.c_str(), &metadata_table);
  metadata_table.version++;
  return n;
}


/* ═══════ Variables: parse XML → variables_description, variables_values, variables_modbus tables ═══════ */
static int parse_variables_xml(const char *xml, variables_description_table_t *description_tbl, variables_values_table_t *values_tbl, variables_modbus_table_t *modbus_tbl) {
  int count = 0;
  const char *pos = xml;
  char buf[256];

  while (count < MAX_VARIABLES_DESCRIPTION_ROWS) {
    const char *row_start = strstr(pos, "<row>");
    if (!row_start) break;
    const char *row_end = strstr(row_start, "</row>");
    if (!row_end) break;

    /* ── description ── */
    extract_tag(row_start, "Class", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_CLASS_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[count].Class = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_start, "Name", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_NAME_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[count].Name = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_start, "Type", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_TYPE_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[count].Type = buf[0] ? strdup(buf) : NULL;

    /* ── values ── */
    extract_tag(row_start, "Value", buf, sizeof(buf));
    if (buf[0] && !validate_variables_values_value(COL_VARIABLES_VALUES_VALUE_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    values_tbl->rows[count].Value = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Operation_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_values_value(COL_VARIABLES_VALUES_OPERATION_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    values_tbl->rows[count].Operation_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Threshold", buf, sizeof(buf));
    if (buf[0] && !validate_variables_values_value(COL_VARIABLES_VALUES_THRESHOLD_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    values_tbl->rows[count].Threshold = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Fault_Code", buf, sizeof(buf));
    if (buf[0] && !validate_variables_values_value(COL_VARIABLES_VALUES_FAULT_CODE_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    values_tbl->rows[count].Fault_Code = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Increment", buf, sizeof(buf));
    if (buf[0] && !validate_variables_values_value(COL_VARIABLES_VALUES_INCREMENT_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    values_tbl->rows[count].Increment = buf[0] ? (float)atof(buf) : -9999.0f;

    /* ── modbus ── */
    extract_tag(row_start, "Slave_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_SLAVE_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    modbus_tbl->rows[count].Slave_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Function_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_FUNCTION_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    modbus_tbl->rows[count].Function_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Start_Address", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_START_ADDRESS_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    modbus_tbl->rows[count].Start_Address = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Data_Length", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_DATA_LENGTH_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    modbus_tbl->rows[count].Data_Length = buf[0] ? (float)atof(buf) : -9999.0f;

    /* link modbus value_ptr → values.Value */
    modbus_tbl->rows[count].value_ptr = &values_tbl->rows[count].Value;

    count++;
    pos = row_end + 6;
  }
  description_tbl->count = count;
  values_tbl->count = count;
  modbus_tbl->count = count;
  return count;
}

/* ── free description strings ── */
void free_variables_description_table(variables_description_table_t *tbl) {
  for (int i = 0; i < tbl->count; i++) {
    if (tbl->rows[i].Class) { free(tbl->rows[i].Class); tbl->rows[i].Class = NULL; }
    if (tbl->rows[i].Name) { free(tbl->rows[i].Name); tbl->rows[i].Name = NULL; }
    if (tbl->rows[i].Type) { free(tbl->rows[i].Type); tbl->rows[i].Type = NULL; }
  }
  tbl->count = 0;
}

/* ── load Variables from SPIFFS into all subcat tables ── */
int load_variables_from_spiffs(void) {
  free_variables_description_table(&variables_description_table);
  File f = SPIFFS.open("/Variables.xml", "r");
  if (!f) return 0;
  String content = f.readString();
  f.close();
  int n = parse_variables_xml(content.c_str(), &variables_description_table, &variables_values_table, &variables_modbus_table);
  variables_description_table.version++;
  variables_values_table.version++;
  variables_modbus_table.version++;
  return n;
}


/* ═══════ SPIFFS provisioning — writes embedded XML defaults ═══════ */
/* Call once in setup() after SPIFFS.begin() / acms_web_init(). */
void provision_spiffs_xml(void) {
  /* always overwrite — developer-managed tables */
  const struct { const char *path; const char *data; } always[1] = {
    { "/Metadata.xml", METADATA_XML_DEFAULT },
  };
  for (int i = 0; i < 1; i++) {
    File f = SPIFFS.open(always[i].path, "w");
    if (f) { f.print(always[i].data); f.close(); }
    yield();
  }
  /* only write if absent — user-managed tables */
  const struct { const char *path; const char *data; } once[1] = {
    { "/Variables.xml", VARIABLES_XML_DEFAULT },
  };
  for (int i = 0; i < 1; i++) {
    if (!SPIFFS.exists(once[i].path)) {
      File f = SPIFFS.open(once[i].path, "w");
      if (f) { f.print(once[i].data); f.close(); }
    }
    yield();
  }
}
