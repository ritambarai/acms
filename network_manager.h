#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

/* ── Portal hostname / domain ──────────────────────────────────────
 *  mDNS  : http://acms.local         (works automatically on most OSes)
 *  DNS   : http://acms.portal.local  (works when the client's DNS server
 *                                    is set to the ESP's IP address)
 * ─────────────────────────────────────────────────────────────────*/
#define WIFI_MDNS_HOST     "acms"
#define WIFI_PORTAL_DOMAIN "acms.portal.local"
#define WIFI_DNS_PORT       53

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Connect to WiFi, start mDNS (acms.local) and DNS server (acms.portal).
 * Blocks until connected or timeout (15 s).
 * Returns true on success, false on timeout. */
bool wifi_manager_init(const char *ssid, const char *password);

/* Service pending DNS requests — call from loop(). */
void wifi_manager_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_MANAGER_H */
