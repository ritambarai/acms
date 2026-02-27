#include <SPIFFS.h>
#include <Preferences.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include "network_manager.h"
#include "acms_web.h"
#include "modbus_manager.h"
extern "C" {
#include "schema.h"   /* settings_general_t settings_general */
}
extern int  load_settings_from_spiffs(void);
extern void provision_spiffs_xml(void);

/* ── Web interface login ── */
#define WEB_USER     "admin"
#define WEB_PASSWORD "admin123"

/* Build ID changes every compile — used to detect a fresh flash. */
static const char FIRMWARE_BUILD_ID[] = __DATE__ " " __TIME__;

void setup()
{
    /* Suppress TWDT noise before anything else — background WiFi auto-reconnect
     * (from old NVS credentials) can start immediately at boot and generate
     * harmless "task not found" errors before Serial is even open.
     * esp_task_wdt_deinit() permanently disables the TWDT if it was already
     * initialized; its own error is silenced by the preceding log suppression. */
    esp_log_level_set("task_wdt", ESP_LOG_NONE);
    esp_task_wdt_deinit();

    Serial.begin(115200);
    delay(1000);

    /* ── Wipe SPIFFS on every new flash ──
     * Compare the firmware build ID stored in NVS against the current one.
     * A mismatch means new firmware was just flashed → format SPIFFS so stale
     * XML files from a previous build are never used.  On plain reboots the IDs
     * match and SPIFFS is left intact. */
    {
        Preferences prefs;
        prefs.begin("acms", false);
        String stored = prefs.getString("build_id", "");
        if (stored != FIRMWARE_BUILD_ID) {
            Serial.println("New firmware detected — formatting SPIFFS...");
            SPIFFS.format();
            prefs.putString("build_id", FIRMWARE_BUILD_ID);
            Serial.println("SPIFFS formatted.");
        }
        prefs.end();
    }

    /* Mount SPIFFS; write XMLs from firmware defaults if absent (first flash),
     * then load live settings — so every boot either uses the user-saved XML
     * or falls back to the default XML created right here. */
    SPIFFS.begin(true);
    provision_spiffs_xml();     /* create Metadata/Variables/Settings.xml if missing */
    load_settings_from_spiffs(); /* read Settings.xml (always present after provision) */

    /* ---------------- WiFi credentials from NVS (written by AP portal / web UI) --
     * NVS is preferred over Settings.xml so credentials can be saved without
     * rewriting the entire XML.  When NVS has values, sync them back into
     * settings_general so the web UI GET /Settings.xml shows live values. */
    {
        Preferences prefs;
        prefs.begin("wifi", true);   /* read-only */
        String nvs_ssid = prefs.getString("ssid", "");
        String nvs_pass = prefs.getString("pass", "");
        prefs.end();

        if (nvs_ssid.length()) {
            /* Sync NVS → settings_general so GET /Settings.xml is always accurate */
            if (settings_general.SSID)     { free(settings_general.SSID);     }
            if (settings_general.Password) { free(settings_general.Password); }
            settings_general.SSID     = strdup(nvs_ssid.c_str());
            settings_general.Password = strdup(nvs_pass.c_str());
        }

        wifi_manager_init(settings_general.SSID, settings_general.Password);
    }

    mqtt_manager_connect();

    /* ---------------- System init ---------------- */
    acms_system_init(WEB_USER, WEB_PASSWORD);

    modbus_setup();
}

void loop()
{
    wifi_manager_loop();
    acms_system_loop();

}
