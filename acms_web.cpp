
#include <Arduino.h>
#include <WiFi.h>       /* for WiFi.localIP() in log message */
#include <WebServer.h>
#include <SPIFFS.h>

extern "C" {
  #include "schema.h"
  #include "data_manager.h"
  #include "alert_manager.h"
}
#include "network_manager.h"
#include "json_telemetry.h"
#include "acms_web.h"
#include "web_page.h"
#include "xml_defaults.h"

#define INCREMENT_LOOP_INTERVAL_MS      1000
#define INCREMENT_THRESHOLD_MARGIN      0.9f  /* ±50 % overshoot margin for numeric cycling */

/* xml_parser.cpp functions */
extern int  load_variables_from_spiffs(void);
extern int  load_metadata_from_spiffs(void);
extern int  load_settings_from_spiffs(void);
extern void free_variables_description_table(variables_description_table_t *tbl);
extern void free_metadata_table(metadata_table_t *tbl);
extern void provision_spiffs_xml(void);   /* writes embedded XML defaults to SPIFFS */
extern bool check_variable_constraints(uint16_t var_pool_id);

/* =========================================================
 * CREDENTIALS (set by acms_system_init)
 * ========================================================= */

static const char *web_user = nullptr;
static const char *web_pass = nullptr;

static bool     reboot_pending = false;
static uint32_t reboot_at      = 0;

/* Deferred reload flags — set by the POST handler, consumed by http_task.
 * Keeps handlers fast and avoids sync_all() / MQTT publish inside a handler,
 * which would race with increment_task (Core 0) modifying the same tables. */
static volatile bool reload_variables_pending = false;
static volatile bool reload_metadata_pending  = false;
static volatile bool reload_settings_pending  = false;

/* Handle to increment_task — captured at task creation so http_task can
 * suspend/resume it around table reloads to prevent SMP race conditions. */
static TaskHandle_t increment_task_handle = NULL;

/* =========================================================
 * ADDRESS POOL (STABLE BACKING STORAGE FOR ext_addr)
 * ========================================================= */

float addr_pool[MAX_VAR_POOL];
static uint16_t last_idx = 0;

increment_pool_t increment_pool = {0};

/* =========================================================
 * WEB SERVER
 * ========================================================= */

static WebServer server(80);

/* ── Require HTTP Basic Auth; returns false and sends 401 if denied ── */
static bool require_auth() {
  if (!server.authenticate(web_user, web_pass)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

/* ── GET / → serve web page from PROGMEM ── */
static void handle_root() {
  if (!require_auth()) return;
  Serial.println("HTTP GET /");
  server.send_P(200, "text/html", WEB_PAGE);
}

/* ── GET /test → simple health check (no auth) ── */
static void handle_test() {
  server.send(200, "text/plain", "OK");
}

/* ── POST /reboot → save complete, schedule restart ── */
static void handle_reboot() {
  if (!require_auth()) return;
  server.send(200, "text/plain", "OK");
  reboot_pending = true;
  reboot_at = millis() + 500;   /* 500 ms gives the response time to flush */
}

/* ── GET /<Table>.xml → serve from SPIFFS, fall back to embedded default ── */
static void handle_get_xml() {
  if (!require_auth()) return;
  String uri = server.uri();

  if (SPIFFS.exists(uri)) {
    File f = SPIFFS.open(uri, "r");
    if (f && f.size() > 0) {
      server.streamFile(f, "application/xml");
      f.close();
      return;
    }
    if (f) f.close();
    /* file exists but is empty — fall through to embedded default */
  }

  /* SPIFFS file absent or empty — serve embedded compile-time default.
   * Use send_P to stream directly from flash without a large heap alloc. */
  const char *fallback = nullptr;
  if      (uri == "/Metadata.xml")  fallback = METADATA_XML_DEFAULT;
  else if (uri == "/Variables.xml") fallback = VARIABLES_XML_DEFAULT;

  if (fallback) server.send_P(200, "application/xml", fallback);
  else          server.send(404, "text/plain", "Not found");
}

/* ── GET /Settings.xml → generate from live settings structs ── */
static void handle_get_settings_xml() {
  if (!require_auth()) return;

  char buf[1024];
  snprintf(buf, sizeof(buf),
    "<Settings>\n"
    "<row><general>"
      "<SSID>%s</SSID><Password>%s</Password>"
      "<Class_Pool_Size>%d</Class_Pool_Size><Var_Pool_Size>%d</Var_Pool_Size>"
    "</general></row>\n"
    "<row><mqtt>"
      "<Host>%s</Host><Port>%d</Port>"
      "<Data_Topic>%s</Data_Topic><Alert_Topic>%s</Alert_Topic>"
      "<Mqtt_Username>%s</Mqtt_Username><Mqtt_Password>%s</Mqtt_Password>"
    "</mqtt></row>\n"
    "<row><json_includes>"
      "<Metadata>%s</Metadata><Constraints>%s</Constraints><Type_Unit>%s</Type_Unit>"
    "</json_includes></row>\n"
    "</Settings>",
    settings_general.SSID            ? settings_general.SSID            : "",
    settings_general.Password        ? settings_general.Password        : "",
    settings_general.Class_Pool_Size,
    settings_general.Var_Pool_Size,
    settings_mqtt.Host               ? settings_mqtt.Host               : "",
    settings_mqtt.Port,
    settings_mqtt.Data_Topic         ? settings_mqtt.Data_Topic         : "",
    settings_mqtt.Alert_Topic        ? settings_mqtt.Alert_Topic        : "",
    settings_mqtt.Mqtt_Username      ? settings_mqtt.Mqtt_Username      : "",
    settings_mqtt.Mqtt_Password      ? settings_mqtt.Mqtt_Password      : "",
    settings_json_includes.Metadata    ? "true" : "false",
    settings_json_includes.Constraints ? "true" : "false",
    settings_json_includes.Type_Unit   ? "true" : "false"
  );

  server.send(200, "application/xml", buf);
}

/* ── GET /alert_count → JSON object with total logged alert count (no auth) ── */
static void handle_alert_count() {
  char buf[32];
  snprintf(buf, sizeof(buf), "{\"count\":%u}", alert_log_count);
  server.send(200, "application/json", buf);
}

/* ── GET /alert_log.jsonl → stream JSONL file from SPIFFS as download ── */
static void handle_alert_log() {
  if (!require_auth()) return;
  if (!SPIFFS.exists("/alert_log.jsonl")) {
    server.send(404, "text/plain", "No alert log");
    return;
  }
  File f = SPIFFS.open("/alert_log.jsonl", "r");
  server.sendHeader("Content-Disposition", "attachment; filename=\"alert_log.jsonl\"");
  server.streamFile(f, "application/x-ndjson");
  f.close();
}

/* ── POST /Settings.xml → save to SPIFFS via plain body (small, ~300 bytes) ── */
static void handle_post_xml() {
  if (!require_auth()) return;
  String uri  = server.uri();
  String body = server.arg("plain");

  Serial.printf("POST %s  (%d bytes)\n", uri.c_str(), body.length());

  if (body.length() == 0) {
    server.send(400, "text/plain", "Empty body");
    return;
  }

  File f = SPIFFS.open(uri, "w");
  if (!f) {
    Serial.println("  SPIFFS open failed");
    server.send(500, "text/plain", "SPIFFS error");
    return;
  }
  f.print(body);
  f.close();
  Serial.println("  Saved to SPIFFS");

  /* Respond immediately — before any reload or MQTT publish. */
  server.send(200, "text/plain", "OK");
  reload_settings_pending = true;
}

/* ── Streaming upload handler for large table XMLs (Variables, Metadata).
 * Writes each multipart chunk directly to SPIFFS so the entire body never
 * needs to fit in a single heap allocation — avoiding OOM on ~15 KB tables.
 * Called repeatedly by the WebServer during multipart/form-data parsing. ── */
static File s_upload_file;

static void handle_upload_table_xml() {
  HTTPUpload &up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    String path = server.uri();
    Serial.printf("UPLOAD %s  (start)\n", path.c_str());
    /* Only open the file if the request is authenticated; writes are
     * skipped (file stays closed) if auth will fail in the main handler. */
    if (server.authenticate(web_user, web_pass))
      s_upload_file = SPIFFS.open(path, "w");

  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_upload_file)
      s_upload_file.write(up.buf, up.currentSize);

  } else if (up.status == UPLOAD_FILE_END) {
    if (s_upload_file) {
      Serial.printf("UPLOAD %s  (%zu bytes total)\n",
                    server.uri().c_str(), up.totalSize);
      s_upload_file.close();
    }

  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (s_upload_file) s_upload_file.close();
  }
}

/* ── Completion handler — called after the upload finishes ── */
static void handle_post_table_xml() {
  if (!require_auth()) return;
  String uri = server.uri();

  /* Respond immediately; reloads are deferred to http_task (see below). */
  server.send(200, "text/plain", "OK");

  if      (uri == "/Variables.xml") reload_variables_pending = true;
  else if (uri == "/Metadata.xml")  reload_metadata_pending  = true;
}

/* =========================================================
 * GET METADATA → data manager pipeline
 * ========================================================= */

void get_metadata(void)
{
  /* dm_set_value + sync are handled inside load_metadata_from_spiffs → parse_metadata_xml */
  load_metadata_from_spiffs();
}

/* =========================================================
 * INIT / LOOP  (internal web layer)
 * ========================================================= */

void acms_web_init(void)
{
  /* SPIFFS is already mounted and XMLs are provisioned in setup() before
   * wifi_manager_init(), so nothing to do here except register routes. */
  server.on("/",       HTTP_GET,  handle_root);
  server.on("/test",   HTTP_GET,  handle_test);
  server.on("/reboot", HTTP_POST, handle_reboot);

  server.on("/Variables.xml", HTTP_GET,  handle_get_xml);
  server.on("/Variables.xml", HTTP_POST, handle_post_table_xml, handle_upload_table_xml);

  server.on("/Metadata.xml", HTTP_GET,  handle_get_xml);
  server.on("/Metadata.xml", HTTP_POST, handle_post_table_xml, handle_upload_table_xml);

  server.on("/Settings.xml", HTTP_GET,  handle_get_settings_xml);
  server.on("/Settings.xml", HTTP_POST, handle_post_xml);

  server.on("/alert_count",     HTTP_GET, handle_alert_count);
  server.on("/alert_log.jsonl", HTTP_GET, handle_alert_log);

  server.begin();
  Serial.print("Web server started at http://");
  Serial.println(WiFi.localIP());
}

void acms_web_loop(void)
{
  server.handleClient();
}

static void http_task(void *pvParameters)
{
    for (;;) {
        server.handleClient();

        /* Process deferred reloads — only when no reboot is scheduled.
         * During a submit sequence the browser always ends with /reboot, so
         * reboot_pending becomes true before these flags are consumed; we skip
         * the reload entirely and let the restart load everything from SPIFFS.
         * For standalone POSTs (no reboot) the reload runs normally here,
         * outside any handler context, safe from reentrancy issues. */
        if (!reboot_pending) {
            if (reload_metadata_pending) {
                reload_metadata_pending = false;
                /* Suspend increment_task (Core 0) while modifying shared tables
                 * to prevent a LoadStoreError SMP race condition. */
                if (increment_task_handle) vTaskSuspend(increment_task_handle);
                load_metadata_from_spiffs();
                am_fault_map_build();
                if (increment_task_handle) vTaskResume(increment_task_handle);
            }
            if (reload_variables_pending) {
                reload_variables_pending = false;
                if (increment_task_handle) vTaskSuspend(increment_task_handle);
                load_variables_from_spiffs();
                if (increment_task_handle) vTaskResume(increment_task_handle);
            }
            if (reload_settings_pending) {
                reload_settings_pending = false;
                /* Settings reload only touches settings_* structs, not var_pool —
                 * no need to suspend increment_task. */
                load_settings_from_spiffs();
                wifi_credentials_save(settings_general.SSID, settings_general.Password);
            }
        }

        if (reboot_pending && millis() >= reboot_at) {
            Serial.flush();   /* drain any buffered Serial output before reset */
            ESP.restart();
        }

        taskYIELD();  /* cooperative yield — resumes immediately if no higher-priority task is ready */
    }
}

/* =========================================================
 * INCREMENT TASK — FreeRTOS task, started from acms_system_init()
 * Iterates every constraints table row, adds Increment to *value_ptr,
 * calls update_variable, then sync_all_nochange after each full pass.
 * ========================================================= */

static void increment_task(void *pvParameters)
{
    /* ── resolve metaData type-indicator var IDs once at task start ── */
    uint16_t _meta_cls    = dm_class_map_find("metaData");
    uint16_t _choice_vid  = INVALID_INDEX;
    uint16_t _numeric_vid = INVALID_INDEX;
    if (_meta_cls != INVALID_INDEX) {
        _choice_vid  = dm_var_map_find(_meta_cls, "type", "choice");
        _numeric_vid = dm_var_map_find(_meta_cls, "type", "numeric");
    }

    for (;;) {
        for (int i = 0; i < increment_pool.count; i++) {
            increment_pool_row_t *r = &increment_pool.rows[i];
            if (r->value_ptr == NULL) continue;

            /* ── find var_pool entry via address map, then look up its type ── */
            bool     _is_choice = false;
            uint16_t _vpid      = dm_addr_map_find(r->value_ptr);
            if (_vpid != INVALID_INDEX) {
                uint16_t _type_vid = dm_var_map_find(var_pool[_vpid].class_idx,
                                                     var_pool[_vpid].var_name,
                                                     "type");
                if (_type_vid != INVALID_INDEX && var_pool[_type_vid].ext_addr != NULL) {
                    float _tval = *(float *)var_pool[_type_vid].ext_addr;
                    float _cv   = (_choice_vid  != INVALID_INDEX && var_pool[_choice_vid].ext_addr)
                                  ? *(float *)var_pool[_choice_vid].ext_addr  : -9999.0f;
                    float _nv   = (_numeric_vid != INVALID_INDEX && var_pool[_numeric_vid].ext_addr)
                                  ? *(float *)var_pool[_numeric_vid].ext_addr : -9999.0f;
                    const char *_cls_name = class_pool[var_pool[_vpid].class_idx].class_name;
                    if (_tval == _cv) {
                        _is_choice = true;
                        Serial.printf("[Increment] [%d] %s/%s → type=choice\n",
                                      i, _cls_name ? _cls_name : "?", var_pool[_vpid].var_name);
                    } else if (_tval == _nv) {
                        Serial.printf("[Increment] [%d] %s/%s → type=numeric\n",
                                      i, _cls_name ? _cls_name : "?", var_pool[_vpid].var_name);
                    } else {
                        Serial.printf("[Increment] [%d] %s/%s → type=unknown(%.0f)\n",
                                      i, _cls_name ? _cls_name : "?", var_pool[_vpid].var_name, _tval);
                    }
                }

                /* ── choice var: scan metaData class for max Key matching this var name ── */
                if (_is_choice && _meta_cls != INVALID_INDEX) {
                    float _max = -9999.0f;
                    var_t *_start = class_pool[_meta_cls].head;
                    if (_start != NULL) {
                        var_t *_v = _start;
                        do {
                            if (_v->var_name &&
                                var_pool[_vpid].var_name &&
                                strcmp(_v->var_name, var_pool[_vpid].var_name) == 0 &&
                                _v->ext_addr != NULL) {
                                float _key = *(float *)_v->ext_addr;
                                if (_key > _max) _max = _key;
                            }
                            _v = _v->next;
                        } while (_v != NULL && _v != _start);
                    }
                    if (_max != -9999.0f) {
                        r->threshold = _max;
                        Serial.printf("[Increment] [%d] choice threshold set to %.0f\n", i, _max);
                    }
                }
            }

            /* ── Choice stability gate ──────────────────────────────────────────
             * Roll a uniform [0,1) float each tick.
             * If roll < MARGIN → stable: keep old value, skip this entry.
             * Probability of change = 1 - MARGIN. */
            if (_is_choice) {
                float _roll = (float)random(0, 10000) / 10000.0f;
                if (_roll < INCREMENT_THRESHOLD_MARGIN) {
                    Serial.printf("[Increment] [%d] stable (roll=%.4f < margin=%.2f) — no change\n",
                                  i, _roll, INCREMENT_THRESHOLD_MARGIN);
                    continue;
                }
            }

            float old_val = *r->value_ptr;
            float new_val;
            if (_is_choice && r->Step == 0.0f) {
                /* randomise uniformly between 0 and threshold (inclusive) */
                Serial.printf("[Increment] [%d] mode=randomised\n", i);
                long _hi = (r->threshold != -9999.0f) ? (long)r->threshold : 1L;
                new_val = (float)random(0, _hi + 1);
                Serial.printf("[Increment] [%d] choice random → new=%.0f (0..%ld)\n",
                              i, new_val, _hi);
            } else {
                /* cycle: add Step */
                Serial.printf("[Increment] [%d] mode=%s\n", i,
                              r->Step > 0.0f ? "positive" : "negative");
                new_val = old_val + r->Step;
                float _lower = -9999.0f, _upper = -9999.0f;
                if (r->threshold != -9999.0f) {
                    if (_is_choice) {
                        /* discrete cycling: exact boundary — reset the moment we hit threshold */
                        bool _hit = (r->Step >= 0.0f) ? (new_val >= r->threshold)
                                                       : (new_val <= r->threshold);
                        if (_hit) {
                            new_val = r->ini_val;
                            Serial.printf("[Increment] [%d] threshold hit — reset to ini_val=%.4f\n",
                                          i, new_val);
                        }
                    } else {
                        /* numeric: symmetric bounds (1 ± MARGIN)×|threshold−ini_val| around
                         * ini_val.  Sign of ± follows Step.  Crossing either bound wraps to
                         * the opposite bound. */
                        float _half = (r->Step >= 0.0f
                                       ? 1.0f + INCREMENT_THRESHOLD_MARGIN
                                       : 1.0f - INCREMENT_THRESHOLD_MARGIN)
                                      * fabsf(r->threshold - r->ini_val);
                        _lower = r->ini_val - _half;
                        _upper = r->ini_val + _half;
                        if (new_val >= _upper) {
                            new_val = _lower;
                            Serial.printf("[Increment] [%d] upper bound hit → wrap to lower=%.4f\n",
                                          i, new_val);
                        } else if (new_val <= _lower) {
                            new_val = _upper;
                            Serial.printf("[Increment] [%d] lower bound hit → wrap to upper=%.4f\n",
                                          i, new_val);
                        }
                    }
                }
                if (!_is_choice && _lower != -9999.0f)
                    Serial.printf("[Increment] [%d] old=%.4f  step=%.4f  new=%.4f  bounds=[%.4f, %.4f]\n",
                                  i, old_val, r->Step, new_val, _lower, _upper);
                else
                    Serial.printf("[Increment] [%d] old=%.4f  step=%.4f  new=%.4f\n",
                                  i, old_val, r->Step, new_val);
            }
            *r->value_ptr = new_val;
            update_variable(r->value_ptr);
            if (_vpid != INVALID_INDEX)
                check_variable_constraints(_vpid);
        }
        sync_all();
        vTaskDelay(pdMS_TO_TICKS(INCREMENT_LOOP_INTERVAL_MS));
    }
}

/* =========================================================
 * SYSTEM INIT — call once from setup()
 * ========================================================= */

void acms_system_init(const char *login_user, const char *login_pass)
{
  web_user = login_user;
  web_pass = login_pass;

  /* ---------------- Core subsystems ---------------- */
  dm_system_init();
  alert_system_init();
  acms_web_init();            /* mounts SPIFFS, provisions XML, starts webserver */
  load_settings_from_spiffs(); /* parse Settings.xml → populate settings_general/mqtt/json structs */
  get_metadata();             /* parse Metadata.xml → register with data manager → sync */
  am_fault_map_build();       /* build fault-code → message LUT from metadata Fault_Code rows */
  load_variables_from_spiffs(); /* parse Variables.xml → populate description/constraints/modbus tables */
  alert_mqtt_task_start();    /* start FreeRTOS task: drains alert_table → MQTT */

  // ---- Dump populated struct tables ---- 
  /*
  Serial.printf("\n=== Metadata Table (%d rows) ===\n", metadata_table.count);
  for (int i = 0; i < metadata_table.count; i++) {
    Serial.printf("  [%d] Key=%.2f  Message=%s  Class=%s\n", i,
      metadata_table.rows[i].Key,
      metadata_table.rows[i].Message ? metadata_table.rows[i].Message : "(null)",
      metadata_table.rows[i].Class   ? metadata_table.rows[i].Class   : "(null)");
  }

  Serial.printf("\n=== Variables Description Table (%d rows) ===\n", variables_description_table.count);
  for (int i = 0; i < variables_description_table.count; i++) {
    Serial.printf("  [%d] Class=%s  Name=%s  Type=%s  Value=%.4f  constraint_id=%d\n", i,
      variables_description_table.rows[i].Class ? variables_description_table.rows[i].Class : "(null)",
      variables_description_table.rows[i].Name  ? variables_description_table.rows[i].Name  : "(null)",
      variables_description_table.rows[i].Type  ? variables_description_table.rows[i].Type  : "(null)",
      variables_description_table.rows[i].Value,
      variables_description_table.rows[i].constraint_id);
  }

  Serial.printf("\n=== Variables Constraints Table (%d rows) ===\n", variables_constraints_table.count);
  for (int i = 0; i < variables_constraints_table.count; i++) {
    Serial.printf("  [%d] Op_ID=%.0f  Threshold=%.2f  Fault_Code=%.0f  Increment=%.4f  constraint_id=%d\n", i,
      variables_constraints_table.rows[i].Operation_ID,
      variables_constraints_table.rows[i].Threshold,
      variables_constraints_table.rows[i].Fault_Code,
      variables_constraints_table.rows[i].Increment,
      variables_constraints_table.rows[i].constraints_id);
  }

  Serial.printf("\n=== Variables Modbus Table (%d rows) ===\n", variables_modbus_table.count);
  for (int i = 0; i < variables_modbus_table.count; i++) {
    Serial.printf("  [%d] Slave_ID=%.0f  Function_ID=%.0f  Start_Address=%.0f  Data_Length=%.0f\n", i,
      variables_modbus_table.rows[i].Slave_ID,
      variables_modbus_table.rows[i].Function_ID,
      variables_modbus_table.rows[i].Start_Address,
      variables_modbus_table.rows[i].Data_Length);
  }
  Serial.println();
  */
  /* increment_pool is populated by load_variables_from_spiffs() during XML parsing
   * (only rows where Increment is non-null are included). */
  Serial.printf("[Increment] Pool has %d row(s)\n", increment_pool.count);

  sync_all();

  xTaskCreate(increment_task, "increment", 4096, NULL, 1, &increment_task_handle);
  xTaskCreatePinnedToCore(http_task, "http", 8192, NULL, 10, NULL, 1);

  Serial.println("Variables registered");
}

/* =========================================================
 * SYSTEM LOOP — call from loop()
 * ========================================================= */

void acms_system_loop(void)
{
  vTaskDelay(pdMS_TO_TICKS(10));  /* yield — HTTP, increment, modbus run in their own tasks */
}
