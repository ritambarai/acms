
#include <Arduino.h>
#include <WiFi.h>       /* for WiFi.localIP() in log message */
#include <WebServer.h>
#include <SPIFFS.h>

extern "C" {
  #include "schema.h"
  #include "data_manager.h"
}
#include "network_manager.h"
#include "json_telemetry.h"
#include "acms_web.h"
#include "web_page.h"
#include "xml_defaults.h"

#define INCREMENT_LOOP_INTERVAL_MS  1000

/* xml_parser.cpp functions */
extern int  load_variables_from_spiffs(void);
extern int  load_metadata_from_spiffs(void);
extern int  load_settings_from_spiffs(void);
extern void free_variables_description_table(variables_description_table_t *tbl);
extern void free_metadata_table(metadata_table_t *tbl);
extern void provision_spiffs_xml(void);   /* writes embedded XML defaults to SPIFFS */

/* =========================================================
 * CREDENTIALS (set by acms_system_init)
 * ========================================================= */

static const char *web_user = nullptr;
static const char *web_pass = nullptr;

static bool     reboot_pending = false;
static uint32_t reboot_at      = 0;

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
    server.streamFile(f, "application/xml");
    f.close();
    return;
  }

  /* SPIFFS file absent — serve the embedded compile-time default */
  const char *fallback = nullptr;
  if      (uri == "/Metadata.xml")  fallback = METADATA_XML_DEFAULT;
  else if (uri == "/Variables.xml") fallback = VARIABLES_XML_DEFAULT;

  if (fallback) server.send(200, "application/xml", fallback);
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
      "<Username>%s</Username><Mqtt_Password>%s</Mqtt_Password>"
    "</mqtt></row>\n"
    "<row><json>"
      "<Metadata>%s</Metadata><Constraints>%s</Constraints><Modbus>%s</Modbus>"
    "</json></row>\n"
    "</Settings>",
    settings_general.SSID            ? settings_general.SSID            : "",
    settings_general.Password        ? settings_general.Password        : "",
    settings_general.Class_Pool_Size,
    settings_general.Var_Pool_Size,
    settings_mqtt.Host               ? settings_mqtt.Host               : "",
    settings_mqtt.Port,
    settings_mqtt.Data_Topic         ? settings_mqtt.Data_Topic         : "",
    settings_mqtt.Alert_Topic        ? settings_mqtt.Alert_Topic        : "",
    settings_mqtt.Username           ? settings_mqtt.Username           : "",
    settings_mqtt.Mqtt_Password      ? settings_mqtt.Mqtt_Password      : "",
    settings_json.Metadata    ? "true" : "false",
    settings_json.Constraints ? "true" : "false",
    settings_json.Modbus      ? "true" : "false"
  );

  server.send(200, "application/xml", buf);
}

/* ── POST /<Table>.xml → save to SPIFFS, reload table ── */
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

  /* reload the corresponding table struct */
  if (uri == "/Variables.xml") {
    load_variables_from_spiffs();
  } else if (uri == "/Metadata.xml") {
    load_metadata_from_spiffs();
  } else if (uri == "/Settings.xml") {
    load_settings_from_spiffs();
    /* Mirror WiFi credentials to NVS so they survive SPIFFS formats */
    wifi_credentials_save(settings_general.SSID, settings_general.Password);
  }

  server.send(200, "text/plain", "OK");
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
  server.on("/Variables.xml", HTTP_POST, handle_post_xml);

  server.on("/Metadata.xml", HTTP_GET,  handle_get_xml);
  server.on("/Metadata.xml", HTTP_POST, handle_post_xml);

  server.on("/Settings.xml", HTTP_GET,  handle_get_settings_xml);
  server.on("/Settings.xml", HTTP_POST, handle_post_xml);

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
        if (reboot_pending && millis() >= reboot_at) {
            ESP.restart();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* =========================================================
 * INCREMENT TASK — FreeRTOS task, started from acms_system_init()
 * Iterates every constraints table row, adds Increment to *value_ptr,
 * calls update_variable, then sync_all_nochange after each full pass.
 * ========================================================= */

static void increment_task(void *pvParameters)
{
    for (;;) {
        for (int i = 0; i < increment_pool.count; i++) {
            increment_pool_row_t *r = &increment_pool.rows[i];
            if (r->value_ptr == NULL) continue;
            float old_val = *r->value_ptr;
            *r->value_ptr += r->Increment;
            Serial.printf("[Increment] [%d] old=%.4f  inc=%.4f  new=%.4f\n",
                          i, old_val, r->Increment, *r->value_ptr);
            update_variable(r->value_ptr);
        }
        sync_all_nochange();
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
  acms_web_init();            /* mounts SPIFFS, provisions XML, starts webserver */
  get_metadata();             /* parse Metadata.xml → register with data manager → sync */
  load_variables_from_spiffs(); /* parse Variables.xml → populate description/constraints/modbus tables */

  /* ---- Dump populated struct tables ---- */
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
    Serial.printf("  [%d] Op_ID=%.0f  Threshold=%.2f  Fault_Code=%.0f  Increment=%.4f\n", i,
      variables_constraints_table.rows[i].Operation_ID,
      variables_constraints_table.rows[i].Threshold,
      variables_constraints_table.rows[i].Fault_Code,
      variables_constraints_table.rows[i].Increment);
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

  /* increment_pool is populated by load_variables_from_spiffs() during XML parsing
   * (only rows where Increment is non-null are included). */
  Serial.printf("[Increment] Pool has %d row(s)\n", increment_pool.count);

  sync_all();

  xTaskCreate(increment_task, "increment", 4096, NULL, 1, NULL);
  xTaskCreatePinnedToCore(http_task, "http", 8192, NULL, 2, NULL, 1);

  Serial.println("Variables registered");
}

/* =========================================================
 * SYSTEM LOOP — call from loop()
 * ========================================================= */

void acms_system_loop(void)
{
  vTaskDelay(pdMS_TO_TICKS(10));  /* yield — HTTP, increment, modbus run in their own tasks */
}
