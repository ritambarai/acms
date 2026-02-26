/*
 * xml_parser.c — generated from XSD
 * Parses XML from SPIFFS and populates row structs.
 */

extern "C" {
#include "schema.h"
#include "data_manager.h"
}
#include <SPIFFS.h>
#include <Preferences.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "xml_defaults.h"
#include "acms_web.h"

metadata_table_t metadata_table = { .count = 0, .version = 0 };
variables_description_table_t variables_description_table = { .count = 0, .version = 0 };
variables_constraints_table_t variables_constraints_table = { .count = 0, .version = 0 };
variables_modbus_table_t variables_modbus_table = { .count = 0, .version = 0 };
settings_general_t settings_general = { NULL, NULL, 0, 0 };
settings_mqtt_t settings_mqtt = { NULL, 0, NULL, NULL, NULL, NULL };
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

  variables_description_row_t row;
  row.Class         = (char *)"metaData";
  row.constraint_id = -1;

  while (count < MAX_METADATA_ROWS) {
    const char *row_start = strstr(pos, "<row>");
    if (!row_start) break;
    const char *row_end = strstr(row_start, "</row>");
    if (!row_end) break;
    int row_len = row_end - row_start;
    char row_buf[512];
    if (row_len >= (int)sizeof(row_buf)) { pos = row_end + 6; continue; }
    memcpy(row_buf, row_start, row_len);
    row_buf[row_len] = '\0';

    extract_tag(row_buf, "Class", buf, sizeof(buf));
    if (buf[0] && !validate_metadata_value(COL_METADATA_CLASS_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    tbl->rows[count].Class = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_buf, "Key", buf, sizeof(buf));
    if (buf[0] && !validate_metadata_value(COL_METADATA_KEY_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    tbl->rows[count].Key = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_buf, "Message", buf, sizeof(buf));
    if (buf[0] && !validate_metadata_value(COL_METADATA_MESSAGE_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    tbl->rows[count].Message = buf[0] ? strdup(buf) : NULL;

    row.Name  = tbl->rows[count].Class;
    row.Type  = tbl->rows[count].Message;
    row.Value = tbl->rows[count].Key;
    dm_set_value(&row, &tbl->rows[count].Key);

    Serial.printf("[%d] Class=%s  Key=%.2f  Message=%s\n", count, tbl->rows[count].Class ? tbl->rows[count].Class : "(null)", tbl->rows[count].Key, tbl->rows[count].Message ? tbl->rows[count].Message : "(null)");

    count++;
    pos = row_end + 6;
  }
  tbl->count = count;
  Serial.printf("[Metadata] total rows: %d\n", count);
  return count;
}

/* ── free dynamically allocated strings ── */
void free_metadata_table(metadata_table_t *tbl) {
  for (int i = 0; i < tbl->count; i++) {
    if (tbl->rows[i].Class) { free(tbl->rows[i].Class); tbl->rows[i].Class = NULL; }
    if (tbl->rows[i].Message) { free(tbl->rows[i].Message); tbl->rows[i].Message = NULL; }
  }
  tbl->count = 0;
}

/* ── load Metadata from SPIFFS; falls back to embedded default if absent or empty ── */
int load_metadata_from_spiffs(void) {
  free_metadata_table(&metadata_table);
  String _content;
  File f = SPIFFS.open("/Metadata.xml", "r");
  if (f) { _content = f.readString(); f.close(); }
  int n = (_content.length() > 0)
          ? parse_metadata_xml(_content.c_str(), &metadata_table)
          : 0;
  if (n == 0) {
    Serial.println("[XML] Metadata.xml empty/missing -- using default");
    n = parse_metadata_xml(METADATA_XML_DEFAULT, &metadata_table);
  }
  metadata_table.version++;
  uint16_t meta_idx = dm_class_map_find("metaData");
  if (meta_idx != INVALID_INDEX) sync_class(meta_idx);
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
    /* Scope searches to the current row — prevents self-closing tags in
     * later rows from shadowing real values in the current row. */
    int row_len = row_end - row_start;
    char row_buf[1024];
    if (row_len >= (int)sizeof(row_buf)) { pos = row_end + 6; continue; }
    memcpy(row_buf, row_start, row_len);
    row_buf[row_len] = '\0';

    /* ── description ── */
    extract_tag(row_buf, "Class", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_CLASS_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[description_count].Class = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_buf, "Name", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_NAME_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[description_count].Name = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_buf, "Type", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_TYPE_STRING, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[description_count].Type = buf[0] ? strdup(buf) : NULL;

    extract_tag(row_buf, "Value", buf, sizeof(buf));
    if (buf[0] && !validate_variables_description_value(COL_VARIABLES_DESCRIPTION_VALUE_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    description_tbl->rows[description_count].Value = buf[0] ? (float)atof(buf) : -9999.0f;

    /* ── constraints: parse into temporaries, commit only if any field is non-default ── */
    float _t_Operation_ID, _t_Threshold, _t_Fault_Code, _t_Increment;

    extract_tag(row_buf, "Operation_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_constraints_value(COL_VARIABLES_CONSTRAINTS_OPERATION_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Operation_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_buf, "Threshold", buf, sizeof(buf));
    if (buf[0] && !validate_variables_constraints_value(COL_VARIABLES_CONSTRAINTS_THRESHOLD_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Threshold = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_buf, "Fault_Code", buf, sizeof(buf));
    if (buf[0] && !validate_variables_constraints_value(COL_VARIABLES_CONSTRAINTS_FAULT_CODE_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Fault_Code = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_buf, "Increment", buf, sizeof(buf));
    if (buf[0] && !validate_variables_constraints_value(COL_VARIABLES_CONSTRAINTS_INCREMENT_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Increment = buf[0] ? (float)atof(buf) : -9999.0f;

    /* ── modbus: parse into temporaries, commit only if any field is non-default ── */
    float _t_Slave_ID, _t_Function_ID, _t_Start_Address, _t_Data_Length;

    extract_tag(row_buf, "Slave_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_SLAVE_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Slave_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_buf, "Function_ID", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_FUNCTION_ID_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Function_ID = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_buf, "Start_Address", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_START_ADDRESS_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Start_Address = buf[0] ? (float)atof(buf) : -9999.0f;

    extract_tag(row_buf, "Data_Length", buf, sizeof(buf));
    if (buf[0] && !validate_variables_modbus_value(COL_VARIABLES_MODBUS_DATA_LENGTH_FLOAT, buf)) {
      pos = row_end + 6; continue;
    }
    _t_Data_Length = buf[0] ? (float)atof(buf) : -9999.0f;

    /* ── hashmap lookup: check if var already registered ── */
    uint16_t _cls_idx = INVALID_INDEX;
    uint16_t _var_pool_id = INVALID_INDEX;
    if (description_tbl->rows[description_count].Class && description_tbl->rows[description_count].Name && description_tbl->rows[description_count].Type) {
      _cls_idx = dm_class_map_find(description_tbl->rows[description_count].Class);
      if (_cls_idx != INVALID_INDEX)
        _var_pool_id = dm_var_map_find(_cls_idx, description_tbl->rows[description_count].Name, description_tbl->rows[description_count].Type);
    }
    if (_var_pool_id == INVALID_INDEX) {
      if (_t_Operation_ID != -9999.0f || _t_Threshold != -9999.0f || _t_Fault_Code != -9999.0f || _t_Increment != -9999.0f) {
        /* at least one constraints field is real — commit this row */
        constraints_tbl->rows[constraints_count].Operation_ID = _t_Operation_ID;
        constraints_tbl->rows[constraints_count].Threshold = _t_Threshold;
        constraints_tbl->rows[constraints_count].Fault_Code = _t_Fault_Code;
        constraints_tbl->rows[constraints_count].Increment = _t_Increment;
        constraints_tbl->rows[constraints_count].value_ptr = &description_tbl->rows[description_count].Value;
        constraints_tbl->rows[constraints_count].constraints_id = -1;
        description_tbl->rows[description_count].constraint_id = constraints_count;
        if (_t_Increment != -9999.0f) {
          increment_pool.rows[increment_pool.count].Increment = _t_Increment;
          increment_pool.rows[increment_pool.count].value_ptr = &description_tbl->rows[description_count].Value;
          increment_pool.count++;
        }
        constraints_count++;
      } else {
        description_tbl->rows[description_count].constraint_id = -1;
      }

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

      dm_set_value(&description_tbl->rows[description_count],
                   &description_tbl->rows[description_count].Value);

      Serial.printf("[%d] Class=%s  Name=%s  Type=%s  Value=%.4f  constraint_id=%d\n", description_count, description_tbl->rows[description_count].Class ? description_tbl->rows[description_count].Class : "(null)", description_tbl->rows[description_count].Name ? description_tbl->rows[description_count].Name : "(null)", description_tbl->rows[description_count].Type ? description_tbl->rows[description_count].Type : "(null)", description_tbl->rows[description_count].Value, description_tbl->rows[description_count].constraint_id);

      description_count++;
    } else {
      /* ── EXISTING VAR: append constraint to chain; value_ptr → original var's Value ── */
      if (_t_Operation_ID != -9999.0f || _t_Threshold != -9999.0f || _t_Fault_Code != -9999.0f || _t_Increment != -9999.0f) {
        constraints_tbl->rows[constraints_count].Operation_ID = _t_Operation_ID;
        constraints_tbl->rows[constraints_count].Threshold = _t_Threshold;
        constraints_tbl->rows[constraints_count].Fault_Code = _t_Fault_Code;
        constraints_tbl->rows[constraints_count].Increment = _t_Increment;
        constraints_tbl->rows[constraints_count].value_ptr = (float *)var_pool[_var_pool_id].ext_addr;
        constraints_tbl->rows[constraints_count].constraints_id = -1;
        if (var_pool[_var_pool_id].constraint_idx == INVALID_INDEX) {
          var_pool[_var_pool_id].constraint_idx = constraints_count;
        } else {
          int _chain_idx = (int)var_pool[_var_pool_id].constraint_idx;
          while (constraints_tbl->rows[_chain_idx].constraints_id != -1)
            _chain_idx = constraints_tbl->rows[_chain_idx].constraints_id;
          constraints_tbl->rows[_chain_idx].constraints_id = constraints_count;
        }
        if (_t_Increment != -9999.0f) {
          increment_pool.rows[increment_pool.count].Increment = _t_Increment;
          increment_pool.rows[increment_pool.count].value_ptr = (float *)var_pool[_var_pool_id].ext_addr;
          increment_pool.count++;
        }
        constraints_count++;
      }
    }

    pos = row_end + 6;
  }

  description_tbl->count = description_count;
  constraints_tbl->count = constraints_count;
  modbus_tbl->count = modbus_count;
  Serial.printf("[Variables] total rows: %d  constraints: %d  modbus: %d\n", description_count, constraints_count, modbus_count);
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

/* ── load Variables from SPIFFS into all subcat tables; falls back to embedded default if absent or empty ── */
int load_variables_from_spiffs(void) {
  free_variables_description_table(&variables_description_table);
  increment_pool.count = 0;
  String content;
  File f = SPIFFS.open("/Variables.xml", "r");
  if (f) { content = f.readString(); f.close(); }
  int n = (content.length() > 0)
          ? parse_variables_xml(content.c_str(), &variables_description_table, &variables_constraints_table, &variables_modbus_table)
          : 0;
  if (n == 0) {
    Serial.println("[XML] Variables.xml empty/missing -- using default");
    n = parse_variables_xml(VARIABLES_XML_DEFAULT, &variables_description_table, &variables_constraints_table, &variables_modbus_table);
  }
  variables_description_table.version++;
  variables_constraints_table.version++;
  variables_modbus_table.version++;
  sync_all();
  return n;
}


/* ── parse Settings XML — one category per row ── */
static void parse_settings_xml(const char *xml) {
  const char *pos = xml;
  char buf[256];
  while (1) {
    const char *row_start = strstr(pos, "<row>");
    if (!row_start) break;
    const char *row_end = strstr(row_start, "</row>");
    if (!row_end) break;

    /* ── general ── */
    if (strstr(row_start, "<general>")) {
      extract_tag(row_start, "SSID", buf, sizeof(buf));
      if (settings_general.SSID) { free(settings_general.SSID); settings_general.SSID = NULL; }
      if (buf[0]) settings_general.SSID = strdup(buf);
      extract_tag(row_start, "Password", buf, sizeof(buf));
      if (settings_general.Password) { free(settings_general.Password); settings_general.Password = NULL; }
      if (buf[0]) settings_general.Password = strdup(buf);
      extract_tag(row_start, "Class_Pool_Size", buf, sizeof(buf));
      settings_general.Class_Pool_Size = buf[0] ? (int32_t)strtol(buf, NULL, 10) : 0;
      extract_tag(row_start, "Var_Pool_Size", buf, sizeof(buf));
      settings_general.Var_Pool_Size = buf[0] ? (int32_t)strtol(buf, NULL, 10) : 0;
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
      extract_tag(row_start, "Username", buf, sizeof(buf));
      if (settings_mqtt.Username) { free(settings_mqtt.Username); settings_mqtt.Username = NULL; }
      if (buf[0]) settings_mqtt.Username = strdup(buf);
      extract_tag(row_start, "Mqtt_Password", buf, sizeof(buf));
      if (settings_mqtt.Mqtt_Password) { free(settings_mqtt.Mqtt_Password); settings_mqtt.Mqtt_Password = NULL; }
      if (buf[0]) settings_mqtt.Mqtt_Password = strdup(buf);
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

/* ── load Settings: seed defaults → NVS WiFi → SPIFFS override ── */
int load_settings_from_spiffs(void) {
  parse_settings_xml(SETTINGS_XML_DEFAULT);
  /* WiFi credentials from NVS (written by captive portal). */
  {
    Preferences _p;
    _p.begin("wifi", true);
    String _ssid = _p.getString("ssid", "");
    String _pass = _p.getString("pass", "");
    _p.end();
    if (_ssid.length() > 0) {
      if (settings_general.SSID) { free(settings_general.SSID); settings_general.SSID = NULL; }
      settings_general.SSID = strdup(_ssid.c_str());
      if (settings_general.Password) { free(settings_general.Password); settings_general.Password = NULL; }
      if (_pass.length() > 0) settings_general.Password = strdup(_pass.c_str());
    }
  }
  File f = SPIFFS.open("/Settings.xml", "r");
  if (!f) return 0;
  String content = f.readString();
  f.close();
  parse_settings_xml(content.c_str());
  return 1;
}


/* provision_spiffs_xml — intentional no-op.
 * XMLs are created in SPIFFS only by the web UI on first Submit.
 * Load functions fall back to xml_defaults.h when no SPIFFS file exists.
 * Captive portal WiFi credentials are stored in NVS, not SPIFFS. */
void provision_spiffs_xml(void) {}
