#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

/* ── Portal hostname / domain ──────────────────────────────────────
 *  mDNS  : http://acms.local         (works automatically on most OSes)
 *  DNS   : http://acms.portal.local  (works when the client's DNS server
 *                                    is set to the ESP's IP address)
 * ─────────────────────────────────────────────────────────────────*/
#define WIFI_MDNS_HOST     "acms-portal"
#define WIFI_PORTAL_DOMAIN "acms.portal.local"
#define WIFI_DNS_PORT       53

/* ── MQTT configuration ────────────────────────────────────────────*/
#define MQTT_SERVER      "mqtt-server.ddns.net"
#define MQTT_PORT        1883
#define MQTT_CLIENT_ID   "ACMS_Rey_ESP32"
#define MQTT_BUFFER_SIZE 12288

#ifdef __cplusplus
#include <PubSubClient.h>
extern PubSubClient mqtt;        /* data topic client  */
extern PubSubClient mqtt_alert;  /* alert topic client */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Connect to WiFi, start mDNS (acms.local) and DNS server (acms.portal).
 * Blocks until connected or timeout (10 s).
 * Returns true on success, false on timeout. */
bool wifi_manager_init(const char *ssid, const char *password);

/* Service pending DNS and MQTT keep-alive — call from loop(). */
void wifi_manager_loop(void);

/* Connect (or reconnect) to the MQTT broker. Blocks until connected. */
void mqtt_manager_connect(void);

/* Publish a payload to the data topic. Returns true on success. */
bool mqtt_manager_publish(const char *topic, const char *payload, bool retain);

/* Publish a payload to the alert topic.
 * dual mode  → uses dedicated alert client + Alert_Topic
 * single mode→ uses data client + whichever topic is configured */
bool mqtt_manager_publish_alert(const char *payload, bool retain);

/* Returns true if the data MQTT client is currently connected. */
bool mqtt_manager_connected(void);

/* Returns true if the alert MQTT client is currently connected.
 * In single-client mode this is the same as mqtt_manager_connected(). */
bool mqtt_manager_connected_alert(void);

/* Save WiFi credentials to NVS — persists across reboots and SPIFFS formats.
 * Call after user edits credentials in the web UI or captive portal. */
void wifi_credentials_save(const char *ssid, const char *pass);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_MANAGER_H */
