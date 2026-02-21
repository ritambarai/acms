#include "network_manager.h"
#include "acms_web.h"

/* ── WiFi credentials ── */
#define WIFI_SSID     "Airtel_dish_8109"
#define WIFI_PASSWORD "Air@39818"

/* ── Web interface login ── */
#define WEB_USER     "admin"
#define WEB_PASSWORD "admin123"

void setup()
{
    Serial.begin(115200);
    delay(1000);

    /* ---------------- WiFi + DNS/mDNS ---------------- */
    wifi_manager_init(WIFI_SSID, WIFI_PASSWORD);

    /* ---------------- System init ---------------- */
    acms_system_init(WEB_USER, WEB_PASSWORD);
}

void loop()
{
    wifi_manager_loop();
    acms_system_loop();
}
