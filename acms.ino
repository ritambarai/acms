#include <SPIFFS.h>
#include <Preferences.h>
#include "network_manager.h"
#include "acms_web.h"
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

    /* ---------------- WiFi + DNS/mDNS ---------------- */
    wifi_manager_init(settings_general.SSID, settings_general.Password);

    /* ---------------- System init ---------------- */
    acms_system_init(WEB_USER, WEB_PASSWORD);

    /* ---------------- MQTT (after HTTP server is fully live) ---------------- */
    mqtt_manager_connect();
}

void loop()
{
    wifi_manager_loop();
    acms_system_loop();
}
