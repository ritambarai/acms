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
extern PubSubClient mqtt;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Connect to WiFi, start mDNS (acms.local) and DNS server (acms.portal).
 * Blocks until connected or timeout (15 s).
 * Returns true on success, false on timeout. */
bool wifi_manager_init(const char *ssid, const char *password);

/* Service pending DNS and MQTT keep-alive — call from loop(). */
void wifi_manager_loop(void);

/* Connect (or reconnect) to the MQTT broker. Blocks until connected. */
void mqtt_manager_connect(void);

/* Publish a payload to an MQTT topic. Returns true on success. */
bool mqtt_manager_publish(const char *topic, const char *payload, bool retain);

/* Returns true if the MQTT client is currently connected. */
bool mqtt_manager_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_MANAGER_H */
