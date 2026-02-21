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
variables_constraints_table_t variables_constraints_table = { .count = 0, .version = 0 };
variables_modbus_table_t variables_modbus_table = { .count = 0, .version = 0 };
settings_wifi_t settings_wifi = { NULL, NULL };
settings_mqtt_t settings_mqtt = { NULL, 0, NULL, NULL };
settings_schema_t settings_schema = { 0, 0 };
settings_json_t settings_json = { false, false, false };

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


/* ═══════ Variables: parse XML → variables_description, variables_constraints, variables_modbus tables ═══════ */
static int parse_variables_xml(const char *xml, variables_description_table_t *description_tbl, variables_constraints_table_t *constraints_tbl, variables_modbus_table_t *modbus_tbl) {
  int description_count = 0;
  int constraints_count = 0;
  int modbus_count = 0;
  const char *pos = xml;
  char buf[256];

  while (description_count < MAX_VARIABLES_DESCRIPTION_ROWS) {
    const char *row_start = strstr(pos, "<row>");
    if (!row_start) break;
    const char *row_end = strstr(row_start, "</row>");
    if (!row_end) break;

    /* ── description ── */
    extract_tag(row_start, "Class", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_CLASS_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[description_count].Class = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_start, "Name", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_NAME_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[description_count].Name = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_start, "Type", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_TYPE_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[description_count].Type = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_start, "Value", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_VALUE_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[description_count].Value = buf[0] ? (float)atof(buf) : -9999.0f;

    /* ── constraints: parse into temporaries, commit only if any field is non-default ── */
    float _t_Operation_ID, _t_Threshold, _t_Fault_Code, _t_Increment;

    extract_tag(row_start, "Operation_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_constraints_value(COL_VARIABLES_CONSTRAINTS_OPERATION_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Operation_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Threshold", buf, sizeof(buf));
    if (buf[0] && !validate_variables_constraints_value(COL_VARIABLES_CONSTRAINTS_THRESHOLD_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Threshold = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Fault_Code", buf, sizeof(buf));
    if (buf[0] && !validate_variables_constraints_value(COL_VARIABLES_CONSTRAINTS_FAULT_CODE_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Fault_Code = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Increment", buf, sizeof(buf));
    if (buf[0] && !validate_variables_constraints_value(COL_VARIABLES_CONSTRAINTS_INCREMENT_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Increment = buf[0] ? (float)atof(buf) : -9999.0f;

    if (_t_Operation_ID != -9999.0f || _t_Threshold != -9999.0f || _t_Fault_Code != -9999.0f || _t_Increment != -9999.0f) {
      /* at least one constraints field is real — commit this row */
      constraints_tbl->rows[constraints_count].Operation_ID = _t_Operation_ID;
      constraints_tbl->rows[constraints_count].Threshold = _t_Threshold;
      constraints_tbl->rows[constraints_count].Fault_Code = _t_Fault_Code;
      constraints_tbl->rows[constraints_count].Increment = _t_Increment;
      constraints_tbl->rows[constraints_count].value_ptr = &description_tbl->rows[description_count].Value;
      description_tbl->rows[description_count].constraint_id = constraints_count;
      constraints_count++;
    } else {
      description_tbl->rows[description_count].constraint_id = -1;
    }

    /* ── modbus: parse into temporaries, commit only if any field is non-default ── */
    float _t_Slave_ID, _t_Function_ID, _t_Start_Address, _t_Data_Length;

    extract_tag(row_start, "Slave_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_SLAVE_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Slave_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Function_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_FUNCTION_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Function_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Start_Address", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_START_ADDRESS_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Start_Address = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_start, "Data_Length", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_DATA_LENGTH_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Data_Length = buf[0] ? (float)atof(buf) : -9999.0f;

    if (_t_Slave_ID != -9999.0f || _t_Function_ID != -9999.0f || _t_Start_Address != -9999.0f || _t_Data_Length != -9999.0f) {
      /* at least one modbus field is real — commit this row */
      modbus_tbl->rows[modbus_count].Slave_ID = _t_Slave_ID;
      modbus_tbl->rows[modbus_count].Function_ID = _t_Function_ID;
      modbus_tbl->rows[modbus_count].Start_Address = _t_Start_Address;
      modbus_tbl->rows[modbus_count].Data_Length = _t_Data_Length;
      modbus_tbl->rows[modbus_count].value_ptr = &description_tbl->rows[description_count].Value;
      modbus_count++;
    } else {
    }

    description_count++;
    pos = row_end + 6;
  }

  description_tbl->count = description_count;
  constraints_tbl->count = constraints_count;
  modbus_tbl->count = modbus_count;
  return description_count;
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
  int n = parse_variables_xml(content.c_str(), &variables_description_table, &variables_constraints_table, &variables_modbus_table);
  variables_description_table.version++;
  variables_constraints_table.version++;
  variables_modbus_table.version++;
  return n;
}


/* ── parse Settings XML — one subcategory per row ── */
static void parse_settings_xml(const char *xml) {
  const char *pos = xml;
  char buf[256];
  while (1) {
    const char *row_start = strstr(pos, "<row>");
    if (!row_start) break;
    const char *row_end = strstr(row_start, "</row>");
    if (!row_end) break;

    /* ── wifi ── */
    if (strstr(row_start, "<wifi>")) {
      extract_tag(row_start, "SSID", buf, sizeof(buf));
      if (settings_wifi.SSID) { free(settings_wifi.SSID); settings_wifi.SSID = NULL; }
      if (buf[0]) settings_wifi.SSID = strdup(buf);
      extract_tag(row_start, "Password", buf, sizeof(buf));
      if (settings_wifi.Password) { free(settings_wifi.Password); settings_wifi.Password = NULL; }
      if (buf[0]) settings_wifi.Password = strdup(buf);
    }

    /* ── mqtt ── */
    else if (strstr(row_start, "<mqtt>")) {
      extract_tag(row_start, "Host", buf, sizeof(buf));
      if (settings_mqtt.Host) { free(settings_mqtt.Host); settings_mqtt.Host = NULL; }
      if (buf[0]) settings_mqtt.Host = strdup(buf);
      extract_tag(row_start, "Port", buf, sizeof(buf));
      settings_mqtt.Port = buf[0] ? (int32_t)strtol(buf, NULL, 10) : 0;
      extract_tag(row_start, "Data_Topic", buf, sizeof(buf));
      if (settings_mqtt.Data_Topic) { free(settings_mqtt.Data_Topic); settings_mqtt.Data_Topic = NULL; }
      if (buf[0]) settings_mqtt.Data_Topic = strdup(buf);
      extract_tag(row_start, "Alert_Topic", buf, sizeof(buf));
      if (settings_mqtt.Alert_Topic) { free(settings_mqtt.Alert_Topic); settings_mqtt.Alert_Topic = NULL; }
      if (buf[0]) settings_mqtt.Alert_Topic = strdup(buf);
    }

    /* ── schema ── */
    else if (strstr(row_start, "<schema>")) {
      extract_tag(row_start, "Class_Pool_Size", buf, sizeof(buf));
      settings_schema.Class_Pool_Size = buf[0] ? (int32_t)strtol(buf, NULL, 10) : 0;
      extract_tag(row_start, "Var_Pool_Size", buf, sizeof(buf));
      settings_schema.Var_Pool_Size = buf[0] ? (int32_t)strtol(buf, NULL, 10) : 0;
    }

    /* ── json ── */
    else if (strstr(row_start, "<json>")) {
      extract_tag(row_start, "Metadata", buf, sizeof(buf));
      settings_json.Metadata = (strcmp(buf, "true") == 0);
      extract_tag(row_start, "Constraints", buf, sizeof(buf));
      settings_json.Constraints = (strcmp(buf, "true") == 0);
      extract_tag(row_start, "Modbus", buf, sizeof(buf));
      settings_json.Modbus = (strcmp(buf, "true") == 0);
    }

    pos = row_end + 6;
  }
}

/* ── load Settings from SPIFFS — silently skips if file absent ── */
int load_settings_from_spiffs(void) {
  File f = SPIFFS.open("/Settings.xml", "r");
  if (!f) return 0;
  String content = f.readString();
  f.close();
  parse_settings_xml(content.c_str());
  return 1;
}


/* ═══════ SPIFFS provisioning — writes embedded XML defaults ═══════ */
/* Call once in setup() after SPIFFS.begin() / acms_web_init(). */
void provision_spiffs_xml(void) {
  /* always overwrite — developer-managed tables */
  const struct { const char *path; const char *data; } always[2] = {
    { "/Metadata.xml", METADATA_XML_DEFAULT },
    { "/Settings.xml", SETTINGS_XML_DEFAULT },
  };
  for (int i = 0; i < 2; i++) {
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
