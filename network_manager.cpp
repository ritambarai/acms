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

/* ── AP portal page ───────────────────────────────────────────── */
static const char AP_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ACMS WiFi Setup</title>
  <style>
    *{box-sizing:border-box}
    body{font-family:sans-serif;max-width:420px;margin:60px auto;padding:24px}
    h2{text-align:center;margin-bottom:24px}
    label{display:block;margin-top:16px;font-weight:600;font-size:.9em}
    input[type=text],input[type=password]{width:100%;padding:10px;margin-top:6px;border:1px solid #ccc;border-radius:4px;font-size:1em}
    button{width:100%;padding:12px;margin-top:20px;background:#2c7be5;color:#fff;border:none;border-radius:4px;font-size:1em;cursor:pointer}
    button:hover{background:#1a68d1}
    #scan-status{font-size:.8em;color:#888;margin-top:6px}
    #net-list{margin-top:4px;border:1px solid #ddd;border-radius:4px;max-height:180px;overflow-y:auto;display:none}
    .net-item{padding:8px 12px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #eee}
    .net-item:last-child{border-bottom:none}
    .net-item:hover{background:#f0f4ff}
    .net-ssid{font-size:.95em}
    .net-rssi{font-size:.8em;color:#888;white-space:nowrap;margin-left:8px}
  </style>
</head>
<body>
  <h2>ACMS WiFi Setup</h2>
  <form method="POST" action="/save">
    <label>SSID
      <input id="ssid-input" name="ssid" type="text" placeholder="Network name" required autocomplete="off">
    </label>
    <div id="scan-status">Scanning for networks&hellip;</div>
    <div id="net-list"></div>
    <label>Password
      <input name="password" type="password" placeholder="Network password" autocomplete="off">
    </label>
    <button type="submit">Connect &amp; Reboot</button>
  </form>
  <script>
    function rssiBar(r){
      if(r>=-60)return"&#9602;&#9604;&#9606;&#9608;";
      if(r>=-70)return"&#9602;&#9604;&#9606;&nbsp;";
      if(r>=-80)return"&#9602;&#9604;&nbsp;&nbsp;";
      return"&#9602;&nbsp;&nbsp;&nbsp;";
    }
    fetch('/scan').then(function(r){return r.json();}).then(function(nets){
      var st=document.getElementById('scan-status');
      var list=document.getElementById('net-list');
      if(!nets||nets.length===0){st.textContent='No networks found.';return;}
      st.textContent=nets.length+' network(s) found \u2014 click to select:';
      list.style.display='';
      nets.forEach(function(n){
        var d=document.createElement('div');
        d.className='net-item';
        d.innerHTML='<span class="net-ssid">'+n.ssid+'</span><span class="net-rssi">'+rssiBar(n.rssi)+' '+n.rssi+' dBm</span>';
        d.onclick=function(){document.getElementById('ssid-input').value=n.ssid;};
        list.appendChild(d);
      });
    }).catch(function(){document.getElementById('scan-status').textContent='Scan failed.';});
  </script>
</body>
</html>
)HTML";

/* ── Update only the <wifi> row inside Settings.xml ──────────── */
static void save_wifi_to_spiffs(const char *ssid, const char *pass)
{
    if (settings_wifi.SSID)     { free(settings_wifi.SSID);     settings_wifi.SSID     = NULL; }
    if (settings_wifi.Password) { free(settings_wifi.Password); settings_wifi.Password = NULL; }
    if (ssid && ssid[0]) settings_wifi.SSID     = strdup(ssid);
    if (pass)            settings_wifi.Password = strdup(pass);

    /* Build the replacement wifi row */
    char newRow[512];
    snprintf(newRow, sizeof(newRow),
             "<row><wifi><SSID>%s</SSID><Password>%s</Password></wifi></row>",
             settings_wifi.SSID     ? settings_wifi.SSID     : "",
             settings_wifi.Password ? settings_wifi.Password : "");

    /* Read existing file */
    String content;
    File rf = SPIFFS.open("/Settings.xml", "r");
    if (rf) { content = rf.readString(); rf.close(); }

    /* Replace the wifi row in-place */
    int start = content.indexOf("<row><wifi>");
    if (start >= 0) {
        int end = content.indexOf("</row>", start);
        if (end >= 0) {
            end += 6; /* length of "</row>" */
            content = content.substring(0, start) + newRow + content.substring(end);
        }
    } else {
        /* No wifi row yet — insert before </Settings> */
        int closing = content.indexOf("</Settings>");
        if (closing >= 0)
            content = content.substring(0, closing) + newRow + "\n</Settings>";
        else
            content = String("<Settings>\n") + newRow + "\n</Settings>";
    }

    File wf = SPIFFS.open("/Settings.xml", "w");
    if (!wf) { Serial.println("[AP] SPIFFS write failed"); return; }
    wf.print(content);
    wf.close();
    Serial.println("[AP] Settings.xml wifi row updated");
}

/* ── Blocking AP captive portal — never returns, reboots on save ─ */
static void start_ap_portal(void)
{
    Serial.println("[AP] Starting WiFi setup portal  SSID: ACMS Portal(acms-portal.local)");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ACMS Portal(acms-portal.local)");
    IPAddress apIP = WiFi.softAPIP();

    dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    dns_server.start(WIFI_DNS_PORT, "*", apIP);
    dns_running = true;

    WebServer apServer(80);

    apServer.on("/", HTTP_GET, [&apServer]() {
        apServer.send_P(200, "text/html", AP_PAGE);
    });

    apServer.on("/scan", HTTP_GET, [&apServer]() {
        int n = WiFi.scanNetworks(false, false);   /* blocking, no hidden */
        String json = "[";
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            String ssid = WiFi.SSID(i);
            ssid.replace("\\", "\\\\");
            ssid.replace("\"", "\\\"");
            json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
        }
        json += "]";
        WiFi.scanDelete();
        apServer.send(200, "application/json", json);
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
        if (millis() - last_reconnect > 10000) {   /* retry every 10 s */
            last_reconnect = millis();
            mqtt_manager_connect();
        }
    }
}

/* ── MQTT connect / reconnect (non-blocking, 5 s total timeout) ────── */
void mqtt_manager_connect(void)
{
    mqtt.setServer(settings_mqtt.Host, settings_mqtt.Port);
    mqtt.setBufferSize(MQTT_BUFFER_SIZE);

    const uint32_t timeout_ms = 5000;
    uint32_t start = millis();

    while (!mqtt.connected()) {
        if (millis() - start > timeout_ms) {
            Serial.println("[MQTT] Timed out — continuing without MQTT");
            return;
        }
        Serial.print("[MQTT] Connecting...");
        if (mqtt.connect(MQTT_CLIENT_ID)) {
            Serial.println(" connected");
        } else {
            Serial.print(" failed rc=");
            Serial.print(mqtt.state());
            Serial.println(", retrying");
            delay(500);
            yield();
        }
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
