/*
 * network_manager.cpp
 * Handles WiFi connection, mDNS (acms.local), and DNS server (acms.portal.local).
 *
 * DNS note
 * ────────
 * The built-in DNS server listens on UDP port 53 and replies to
 * "acms.portal.local" queries with the ESP's own IP address.
 * For this to resolve on a client device, that device's DNS server must
 * be set to the ESP's IP (e.g. manually in network settings, or
 * automatically if the ESP is acting as a DHCP/AP server).
 * The mDNS entry "acms.local" works on all modern OSes with no
 * manual configuration required.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

#include "network_manager.h"

static DNSServer dns_server;
static bool      dns_running = false;

/* ── Connect to WiFi ──────────────────────────────────────────── */
bool wifi_manager_init(const char *ssid, const char *password)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("[WiFi] Connecting to ");
    Serial.print(ssid);

    const uint32_t timeout_ms = 15000;
    uint32_t start = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) {
            Serial.println("\n[WiFi] Connection timed out");
            return false;
        }
        delay(500);
        yield();
        Serial.print(".");
    }

    Serial.println();
    Serial.print("[WiFi] Connected  IP: ");
    Serial.println(WiFi.localIP());

    /* ── mDNS: acms.local ─────────────────────────────────────── */
    if (MDNS.begin(WIFI_MDNS_HOST)) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("[mDNS] http://" WIFI_MDNS_HOST ".local");
    } else {
        Serial.println("[mDNS] Failed to start");
    }

    /* ── DNS server: acms.portal ──────────────────────────────── */
    /*  Responds to "acms.portal" queries with the ESP's own IP.   */
    dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    dns_server.start(WIFI_DNS_PORT, WIFI_PORTAL_DOMAIN, WiFi.localIP());
    dns_running = true;
    Serial.println("[DNS]  http://" WIFI_PORTAL_DOMAIN
                   "  (set device DNS to " + WiFi.localIP().toString() + ")");

    return true;
}

/* ── Service DNS requests (call every loop iteration) ─────────── */
void wifi_manager_loop(void)
{
    if (dns_running) {
        dns_server.processNextRequest();
    }
}
