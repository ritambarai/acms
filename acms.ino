#include <WiFi.h>
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

  /* ---------------- WiFi ---------------- */
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    yield();
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  /* ---------------- System init ---------------- */
  acms_system_init(WEB_USER, WEB_PASSWORD);
}

void loop()
{
  acms_system_loop();
}
