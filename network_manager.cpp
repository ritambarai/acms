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
#include <WebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <PubSubClient.h>

#include "network_manager.h"
extern "C" {
#include "schema.h"
}

static DNSServer dns_server;
static bool      dns_running = false;

/* ── MQTT ─────────────────────────────────────────────────────────── */
static WiFiClient  espClient;
PubSubClient       mqtt(espClient);

/* ── AP portal page — split so scan results can be injected ──── */
static const char AP_PAGE_TOP[] PROGMEM = R"HTML(<!DOCTYPE html><html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ACMS WiFi Setup</title>
  <style>
    *{box-sizing:border-box}
    body{font-family:sans-serif;max-width:420px;margin:60px auto;padding:24px}
    h2{text-align:center;margin-bottom:24px}
    label{display:block;margin-top:16px;font-weight:600;font-size:.9em}
    input[type=text],input[type=password],select{width:100%;padding:10px;margin-top:6px;border:1px solid #ccc;border-radius:4px;font-size:1em;background:#fff}
    button{width:100%;padding:12px;margin-top:20px;background:#2c7be5;color:#fff;border:none;border-radius:4px;font-size:1em;cursor:pointer}
    button:hover{background:#1a68d1}
  </style>
</head>
<body>
  <h2>ACMS WiFi Setup</h2>
  <form method="POST" action="/save">
    <label>Available SSIDs:
      <select name="ssid" required>
        <option value="">-- select a network --</option>
)HTML";

static const char AP_PAGE_BOT[] PROGMEM = R"HTML(      </select>
    </label>
    <label>Password
      <input name="password" type="password" placeholder="Network password" autocomplete="off">
    </label>
    <button type="submit">Connect &amp; Reboot</button>
  </form>
</body>
</html>
)HTML";

/* ── Write complete Settings.xml with new WiFi credentials ──── */
static void save_wifi_to_spiffs(const char *ssid, const char *pass)
{
    /* Update in-memory general struct */
    if (settings_general.SSID)     { free(settings_general.SSID);     settings_general.SSID     = NULL; }
    if (settings_general.Password) { free(settings_general.Password); settings_general.Password = NULL; }
    if (ssid && ssid[0]) settings_general.SSID     = strdup(ssid);
    if (pass)            settings_general.Password = strdup(pass);

    /* Write the full Settings.xml in the canonical 3-row format.
     * Non-WiFi fields use whatever is already in memory (loaded at boot). */
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "<Settings>\n"
        "<row><general>"
          "<SSID>%s</SSID><Password>%s</Password>"
          "<Class_Pool_Size>%d</Class_Pool_Size><Var_Pool_Size>%d</Var_Pool_Size>"
        "</general></row>\n"
        "<row><mqtt>"
          "<Host>%s</Host><Port>%d</Port>"
          "<Data_Topic>%s</Data_Topic><Alert_Topic>%s</Alert_Topic>"
          "<Username>%s</Username><Mqtt_Password>%s</Mqtt_Password>"
        "</mqtt></row>\n"
        "<row><json>"
          "<Metadata>%s</Metadata><Constraints>%s</Constraints><Modbus>%s</Modbus>"
        "</json></row>\n"
        "</Settings>",
        settings_general.SSID            ? settings_general.SSID            : "",
        settings_general.Password        ? settings_general.Password        : "",
        settings_general.Class_Pool_Size,
        settings_general.Var_Pool_Size,
        settings_mqtt.Host               ? settings_mqtt.Host               : "",
        settings_mqtt.Port,
        settings_mqtt.Data_Topic         ? settings_mqtt.Data_Topic         : "",
        settings_mqtt.Alert_Topic        ? settings_mqtt.Alert_Topic        : "",
        settings_mqtt.Username           ? settings_mqtt.Username           : "",
        settings_mqtt.Mqtt_Password      ? settings_mqtt.Mqtt_Password      : "",
        settings_json.Metadata    ? "true" : "false",
        settings_json.Constraints ? "true" : "false",
        settings_json.Modbus      ? "true" : "false"
    );

    File wf = SPIFFS.open("/Settings.xml", "w");
    if (!wf) { Serial.println("[AP] SPIFFS write failed"); return; }
    wf.print(buf);
    wf.close();
    Serial.println("[AP] Settings.xml updated");
}

/* ── Blocking AP captive portal — never returns, reboots on save ─ */
static void start_ap_portal(void)
{
    Serial.println("[AP] Starting WiFi setup portal  SSID: ACMS Portal(acms-portal.local)");
    WiFi.mode(WIFI_AP_STA);   /* STA radio must stay on for scanNetworks() to work */
    WiFi.softAP("ACMS Portal(acms-portal.local)");
    IPAddress apIP = WiFi.softAPIP();

    dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    dns_server.start(WIFI_DNS_PORT, "*", apIP);
    dns_running = true;

    WebServer apServer(80);

    apServer.on("/", HTTP_GET, [&apServer]() {
        /* Scan first — results are embedded directly in the page HTML */
        int n = WiFi.scanNetworks(false, false);   /* blocking, no hidden */

        String page = String(FPSTR(AP_PAGE_TOP));
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            ssid.replace("&", "&amp;");
            ssid.replace("<", "&lt;");
            ssid.replace("\"", "&quot;");
            page += "        <option value=\"" + ssid + "\">"
                    + ssid + "  (" + WiFi.RSSI(i) + " dBm)</option>\n";
        }
        WiFi.scanDelete();
        page += String(FPSTR(AP_PAGE_BOT));

        apServer.send(200, "text/html", page);
    });

    apServer.on("/save", HTTP_POST, [&apServer, &apIP]() {
        String newSsid = apServer.arg("ssid");
        String newPass = apServer.arg("password");
        save_wifi_to_spiffs(newSsid.c_str(), newPass.c_str());
        apServer.send(200, "text/html",
            "<html><body style='font-family:sans-serif;text-align:center;margin-top:80px'>"
            "<h2>Saved. Rebooting&hellip;</h2></body></html>");
        delay(500);
        ESP.restart();
    });

    /* Redirect every unknown URL to the setup page (captive portal behaviour) */
    apServer.onNotFound([&apServer, &apIP]() {
        apServer.sendHeader("Location",
            String("http://") + apIP.toString() + "/", true);
        apServer.send(302, "text/plain", "");
    });

    apServer.begin();
    Serial.printf("[AP] Portal at http://%s\n", apIP.toString().c_str());

    while (true) {
        dns_server.processNextRequest();
        apServer.handleClient();
        yield();
    }
}

/* ── Connect to WiFi ──────────────────────────────────────────── */
bool wifi_manager_init(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0]) {
        Serial.println("[WiFi] No SSID configured");
        start_ap_portal();   /* never returns */
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("[WiFi] Connecting to ");
    Serial.print(ssid);

    const uint32_t timeout_ms = 15000;
    uint32_t start = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) {
            Serial.println("\n[WiFi] Connection timed out");
            start_ap_portal();   /* never returns */
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

    /* ── DNS server: captive portal ──────────────────────────────
     *  Wildcard "*" catches every DNS query and replies with the
     *  ESP's IP, so clients reach the portal at acms.portal.local
     *  without any manual DNS configuration.                       */
    dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    dns_server.start(WIFI_DNS_PORT, "*", WiFi.localIP());
    dns_running = true;
    Serial.println("[DNS]  http://" WIFI_PORTAL_DOMAIN
                   "  -> " + WiFi.localIP().toString());

    return true;
}

/* ── Service DNS requests (call every loop iteration) ─────────── */
void wifi_manager_loop(void)
{
    if (dns_running) {
        dns_server.processNextRequest();
    }
    if (mqtt.connected()) {
        mqtt.loop();
    } else {
        static uint32_t last_reconnect = 0;
        if (millis() - last_reconnect > 30000) {   /* retry every 30 s */
            last_reconnect = millis();
            mqtt_manager_connect();
        }
    }
}

/* ── MQTT connect — single attempt, returns immediately on success or failure.
 *   Retries are scheduled by wifi_manager_loop() every 30 s so the HTTP server
 *   is never blocked for more than one TCP connect attempt (~1 s worst case). */
void mqtt_manager_connect(void)
{
    if (!settings_mqtt.Host || !settings_mqtt.Host[0]) return;  /* no server configured */

    mqtt.setServer(settings_mqtt.Host, settings_mqtt.Port);
    mqtt.setBufferSize(MQTT_BUFFER_SIZE);

    /* Limit how long the TCP handshake can block the main loop. */
    espClient.setTimeout(1000);   /* 1 s read/connect timeout */

    Serial.print("[MQTT] Connecting to ");
    Serial.print(settings_mqtt.Host);
    Serial.print("...");

    bool ok;
    if (settings_mqtt.Username && settings_mqtt.Username[0]) {
        const char *user = settings_mqtt.Username;
        const char *pw   = (settings_mqtt.Mqtt_Password && settings_mqtt.Mqtt_Password[0])
                           ? settings_mqtt.Mqtt_Password : nullptr;
        ok = mqtt.connect(MQTT_CLIENT_ID, user, pw);
    } else {
        ok = mqtt.connect(MQTT_CLIENT_ID);
    }
    if (ok) {
        Serial.println(" connected");
    } else {
        Serial.print(" failed rc=");
        Serial.println(mqtt.state());
    }
}

bool mqtt_manager_publish(const char *topic, const char *payload, bool retain)
{
    return mqtt.publish(topic, payload, retain);
}

bool mqtt_manager_connected(void)
{
    return mqtt.connected();
}
