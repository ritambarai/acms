
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>

extern "C" {
  #include "schema.h"
  #include "data_manager.h"
}
#include "json_telemetry.h"
#include "acms_web.h"
#include "web_page.h"

/* xml_parser.cpp functions */
extern int  load_variables_from_spiffs(void);
extern int  load_metadata_from_spiffs(void);
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
 * SENSOR VARIABLES (global lifetime — registered by pointer)
 * ========================================================= */

static float temperature = 25.5f;
static float pressure    = 101.3f;
static float humidity    = 60.0f;
static float voltage     = 3.3f;
static float online      = 1.0f;

/* Cached class indices — filled once in acms_system_init() */
static uint16_t cls_sensor = INVALID_INDEX;
static uint16_t cls_power  = INVALID_INDEX;

/* =========================================================
 * ADDRESS POOL (STABLE BACKING STORAGE FOR ext_addr)
 * ========================================================= */

float addr_pool[MAX_VAR_POOL];
static uint16_t last_idx = 0;

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

/* ── GET /<Table>.xml → serve from SPIFFS ── */
static void handle_get_xml() {
  if (!require_auth()) return;
  String uri = server.uri();

  if (!SPIFFS.exists(uri)) {
    server.send(404, "text/plain", "Not found");
    return;
  }

  File f = SPIFFS.open(uri, "r");
  server.streamFile(f, "application/xml");
  f.close();
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
  }

  server.send(200, "text/plain", "OK");
}

/* =========================================================
 * GET METADATA → data manager pipeline
 * ========================================================= */

void get_metadata(void)
{
  load_metadata_from_spiffs();

  for (int i = 0; i < metadata_table.count; i++) {
    if (last_idx >= MAX_VAR_POOL) break;

    const char *cls = metadata_table.rows[i].Class;
    const char *msg = metadata_table.rows[i].Message;
    float       key = metadata_table.rows[i].Key;

    if (!cls || !msg) continue;

    addr_pool[last_idx] = key;
    dm_set_value("meta", cls, msg, &addr_pool[last_idx], key);
    last_idx++;
  }

  /* sync the "meta" class to JSON + send */
  uint16_t meta_idx = dm_class_map_find("meta");
  if (meta_idx != INVALID_INDEX) {
    sync_class(meta_idx);
  }
}

/* =========================================================
 * INIT / LOOP  (internal web layer)
 * ========================================================= */

void acms_web_init(void)
{
  if (!SPIFFS.begin(true)) {   /* 'true' formats on first boot — can block several seconds */
    Serial.println("SPIFFS mount failed");
  }
  yield();   /* let WiFi/lwIP tasks run after potentially long SPIFFS format */

  provision_spiffs_xml();   /* write Metadata.xml + Variables.xml from firmware */
  yield();

  server.on("/",       HTTP_GET,  handle_root);
  server.on("/test",   HTTP_GET,  handle_test);
  server.on("/reboot", HTTP_POST, handle_reboot);

  server.on("/Variables.xml", HTTP_GET,  handle_get_xml);
  server.on("/Variables.xml", HTTP_POST, handle_post_xml);

  server.on("/Metadata.xml", HTTP_GET,  handle_get_xml);
  server.on("/Metadata.xml", HTTP_POST, handle_post_xml);

  server.begin();
  Serial.print("Web server started at http://");
  Serial.println(WiFi.localIP());
}

void acms_web_loop(void)
{
  server.handleClient();
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
  acms_web_init();     /* mounts SPIFFS, provisions XML, starts webserver */
  mqtt_connect();
  get_metadata();      /* parse Metadata.xml → register with data manager → sync */

  /* ---------------- Register sensor variables (ONCE) ---------------- */
  dm_set_value("Sensor", "Temp",     "float", &temperature, temperature);
  dm_set_value("Sensor", "Pressure", "float", &pressure,    pressure);
  dm_set_value("Sensor", "Humidity", "float", &humidity,    humidity);

  dm_set_value("Power", "Status",  "bool",  &online,  online);
  dm_set_value("Power", "Voltage", "float", &voltage, voltage);
  sync_all();

  /* Cache class indices for use in loop */
  cls_sensor = dm_class_map_find("Sensor");
  cls_power  = dm_class_map_find("Power");

  Serial.println("Variables registered");
}

/* =========================================================
 * SYSTEM LOOP — call from loop()
 * ========================================================= */

void acms_system_loop(void)
{
  /* Always service HTTP — runs on every loop iteration */
  acms_web_loop();

  if (reboot_pending && millis() >= reboot_at) {
    ESP.restart();
  }

  /* Sensor simulation + sync runs every 2 s, non-blocking */
  static uint32_t last_sync = 0;
  if (millis() - last_sync < 2000) return;
  last_sync = millis();

  /* ---------------- Simulate sensor changes ---------------- */
  temperature += 0.25f;
  if (temperature > 55.5f) temperature = 25.5f;

  pressure += 0.10f;
  if (pressure > 181.3f) pressure = 101.3f;

  humidity += 0.05f;
  if (humidity > 100.0f) humidity = 60.0f;

  /* ---------------- Update + sync Sensor class ---------------- */
  update_variable(&temperature);
  update_variable(&pressure);
  update_variable(&humidity);

  if (cls_sensor != INVALID_INDEX) {
    sync_class(cls_sensor);
  }

  acms_web_loop();

  /* ---------------- Simulate power changes ---------------- */
  voltage += 0.01f;
  if (voltage > 13.3f) voltage = 3.3f;

  online = (online > 0.5f) ? 0.0f : 1.0f;

  /* ---------------- Update + sync Power class ---------------- */
  update_variable(&voltage);
  update_variable(&online);

  if (cls_power != INVALID_INDEX) {
    sync_class(cls_power);
  }
}
