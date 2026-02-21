#include <SPIFFS.h>
#include "network_manager.h"
#include "acms_web.h"
extern "C" {
#include "schema.h"   /* settings_wifi_t settings_wifi */
}
extern int load_settings_from_spiffs(void);

/* ── Web interface login ── */
#define WEB_USER     "admin"
#define WEB_PASSWORD "admin123"

void setup()
{
    Serial.begin(115200);
    delay(1000);

    /* Mount SPIFFS and populate all settings structs before anything else */
    SPIFFS.begin(true);
    load_settings_from_spiffs();

    /* ---------------- WiFi + DNS/mDNS ---------------- */
    wifi_manager_init(settings_wifi.SSID, settings_wifi.Password);

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
