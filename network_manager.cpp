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
static WiFiClient  espClientAlert;
PubSubClient       mqtt(espClient);            /* data topic client       */
PubSubClient       mqtt_alert(espClientAlert); /* alert topic client      */
static bool        dual_mqtt = false;          /* true when topics differ */

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

/* ── Save WiFi credentials to NVS (read back at boot before Settings.xml) ── */
void wifi_credentials_save(const char *ssid, const char *pass)
{
    Preferences prefs;
    prefs.begin("wifi", false);   /* read-write */
    prefs.putString("ssid", ssid ? ssid : "");
    prefs.putString("pass", pass ? pass : "");
    prefs.end();
    Serial.printf("[WiFi] Credentials saved to NVS  ssid=%s\n", ssid ? ssid : "");
}

/* ── Blocking AP captive portal — never returns, reboots on save ─ */
static void start_ap_portal(void)
{
    Serial.println("[AP] Starting WiFi setup portal  SSID: ACMS-Setup");
    /* Cycle through WIFI_OFF so the stack fully tears down its internal tasks
     * and they deregister from the TWDT before we restart in AP+STA mode.
     * Without this, old task handles trigger "task not found" WDT errors. */
    WiFi.disconnect(false);    /* disconnect without stopping the stack */
    WiFi.mode(WIFI_OFF);       /* full stop — internal tasks clean up */
    delay(500);                /* wait for TWDT deregistrations to complete */
    WiFi.mode(WIFI_AP_STA);   /* STA radio must stay on for scanNetworks() to work */
    delay(200);
    WiFi.softAP("ACMS-Setup(192.168.4.1)");
    delay(500);                /* give AP time to come up before reading IP */
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
        wifi_credentials_save(newSsid.c_str(), newPass.c_str());
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

    const uint32_t timeout_ms = 10000;
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

    /* MQTT is intentionally NOT connected here.
     * acms_system_init() (XML parsing, task creation) runs after this call,
     * and loop() → wifi_manager_loop() → mqtt.loop() won't run until setup()
     * returns.  If we connected now the broker would drop the connection before
     * the first keepalive ping.  mqtt_manager_connect() is called explicitly in
     * setup() after acms_system_init() completes. */

    return true;
}

/* ── Service DNS requests (call every loop iteration) ─────────── */
void wifi_manager_loop(void)
{
    if (dns_running) {
        dns_server.processNextRequest();
    }

    /* Service keep-alive loops */
    if (mqtt.connected())                      mqtt.loop();
    if (dual_mqtt && mqtt_alert.connected())   mqtt_alert.loop();

    /* Reconnect any dropped client every 30 s */
    bool needs_reconnect = !mqtt.connected() || (dual_mqtt && !mqtt_alert.connected());
    if (needs_reconnect) {
        static uint32_t last_reconnect = 0;
        if (millis() - last_reconnect > 30000) {
            last_reconnect = millis();
            mqtt_manager_connect();
        }
    }
}

/* ── helper: single connect attempt with credentials ── */
static bool connect_mqtt_client(PubSubClient &client, const char *client_id)
{
    bool ok;
    if (settings_mqtt.Mqtt_Username && settings_mqtt.Mqtt_Username[0]) {
        const char *user = settings_mqtt.Mqtt_Username;
        const char *pw   = (settings_mqtt.Mqtt_Password && settings_mqtt.Mqtt_Password[0])
                           ? settings_mqtt.Mqtt_Password : nullptr;
        ok = client.connect(client_id, user, pw);
    } else {
        ok = client.connect(client_id);
    }
    return ok;
}

/* ── MQTT connect ──────────────────────────────────────────────────
 * Cases:
 *   neither topic set          → no-op (no broker needed)
 *   both set AND different     → dual clients, unique client IDs
 *   both same OR only one set  → single client, shared topic
 *
 * Single attempt per call; wifi_manager_loop() retries every 30 s. */
void mqtt_manager_connect(void)
{
    if (!settings_mqtt.Host || !settings_mqtt.Host[0]) return;

    const char *dt = (settings_mqtt.Data_Topic  && settings_mqtt.Data_Topic[0])
                     ? settings_mqtt.Data_Topic  : nullptr;
    const char *at = (settings_mqtt.Alert_Topic && settings_mqtt.Alert_Topic[0])
                     ? settings_mqtt.Alert_Topic : nullptr;

    /* Case: neither topic configured → nothing to connect for */
    if (!dt && !at) return;

    /* Determine mode */
    if (dt && at && strcmp(dt, at) != 0) {
        /* Both topics set and different → dual clients */
        dual_mqtt = true;
    } else {
        /* Same topic, or only one is set → single client */
        dual_mqtt = false;
        if (!dt) dt = at;   /* only Alert_Topic configured → use it as data topic */
    }

    /* ── data client (always used) ── */
    mqtt.setServer(settings_mqtt.Host, settings_mqtt.Port);
    mqtt.setBufferSize(MQTT_BUFFER_SIZE);
    espClient.setTimeout(1000);

    Serial.printf("[MQTT] Connecting%s to %s...",
                  dual_mqtt ? " data client" : "", settings_mqtt.Host);
    if (connect_mqtt_client(mqtt, MQTT_CLIENT_ID)) {
        Serial.println(" connected");
    } else {
        Serial.printf(" failed rc=%d\n", mqtt.state());
    }

    /* ── alert client (dual mode only) ── */
    if (dual_mqtt) {
        mqtt_alert.setServer(settings_mqtt.Host, settings_mqtt.Port);
        mqtt_alert.setBufferSize(MQTT_BUFFER_SIZE);
        espClientAlert.setTimeout(1000);

        Serial.printf("[MQTT] Connecting alert client to %s...", settings_mqtt.Host);
        if (connect_mqtt_client(mqtt_alert, MQTT_CLIENT_ID "_A")) {
            Serial.println(" connected");
        } else {
            Serial.printf(" failed rc=%d\n", mqtt_alert.state());
        }
    }
}

/* ── Publish to the data topic ── */
bool mqtt_manager_publish(const char *topic, const char *payload, bool retain)
{
    return mqtt.publish(topic, payload, retain);
}

/* ── Publish to the alert topic ──────────────────────────────────
 * dual mode  → uses dedicated alert client + Alert_Topic
 * single mode→ uses data client + whichever topic is configured  */
bool mqtt_manager_publish_alert(const char *payload, bool retain)
{
    if (dual_mqtt) {
        return mqtt_alert.publish(settings_mqtt.Alert_Topic, payload, retain);
    }
    /* Single client: pick whichever topic is set */
    const char *topic = (settings_mqtt.Data_Topic && settings_mqtt.Data_Topic[0])
                        ? settings_mqtt.Data_Topic
                        : settings_mqtt.Alert_Topic;
    return mqtt.publish(topic, payload, retain);
}

bool mqtt_manager_connected(void)
{
    return mqtt.connected();
}

bool mqtt_manager_connected_alert(void)
{
    return dual_mqtt ? mqtt_alert.connected() : mqtt.connected();
}
