
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "network_manager.h"
#include "json_telemetry.h"
#include "data_manager.h"
extern "C" {
#include "schema.h"   /* settings_mqtt_t settings_mqtt */
}

StaticJsonDocument<12288> doc;  // 12 KB
static const char *api_url = "https://acms-sustlabs.vercel.app/api/data";
static const char *CMD_URL  = "https://acms-sustlabs.vercel.app/api/cmd";


/* =========================================================
 * REMOTE VALUE POOL (STABLE ADDRESSES)
 * ========================================================= */

static float value_pool[MAX_REMOTE_VARS];
static bool  value_used[MAX_REMOTE_VARS];
static uint16_t value_last_idx = 0;



/* =========================================================
 * ALLOC REMOTE VALUE
 * ========================================================= */
 
static float *alloc_remote_value(float initial)
{
    if (value_last_idx >= MAX_REMOTE_VARS)
        return NULL;

    uint16_t idx = value_last_idx++;
    value_used[idx] = true;
    value_pool[idx] = initial;
    return &value_pool[idx];
}

/* --------------------------------------------------------
 * BEGIN / RESET JSON FOR ONE CLASS ONLY
 * -------------------------------------------------------- */
void json_begin_class(const char *class_name)
{
    /* Remove only this class entry */
    doc.remove(class_name);
}

void json_add_var(uint16_t var_idx)
{
    //if (!used_var[var_idx])
    //    return;
    //Serial.println(String("Var ID received: ") + var_idx );
    //Serial.println(String("Var used: ") + used_var[var_idx] );
    var_t   *v   = &var_pool[var_idx];
    class_t *cls = &class_pool[v->class_idx];

    /* Get or create class array */
    JsonArray arr;

    if (doc.containsKey(cls->class_name)) {
    arr = doc[cls->class_name].as<JsonArray>();
    } else {
    arr = doc[cls->class_name].to<JsonArray>();  // creates []
    }


    JsonObject obj;   // will point to existing or new object
    bool found = false;

    /* Search for existing variable object */
    for (JsonObject o : arr) {
        if (o["id"] == v->var_idx) {
            obj = o;
            found = true;
            break;
        }
    }

    /* Create if not found */
    if (!found) {
        obj = arr.createNestedObject();
        obj["id"] = v->var_idx;   // set once
    }

    /* Update / overwrite fields */
    obj["name"]          = v->var_name;
    obj["type"]          = v->var_type ? v->var_type : "";
    if (v->constraint_idx == INVALID_INDEX)
        obj["constraint_id"] = nullptr;
    else
        obj["constraint_id"] = v->constraint_idx;

    char val_buf[32];
    snprintf(val_buf, sizeof(val_buf), "%.2f", v->cached_val);
    obj["value"] = val_buf;
}

void json_remove_var(uint16_t var_idx)
{
    if (!used_var[var_idx])
        return;

    
    var_t   *v   = &var_pool[var_idx];
    class_t *cls = &class_pool[v->class_idx];

    JsonVariant classVar = doc[cls->class_name];
    if (classVar.isNull())
        return;   // class doesn't exist

    JsonArray arr = classVar.as<JsonArray>();

    /* Find and remove object */
    for (size_t i = 0; i < arr.size(); i++) {
        JsonObject obj = arr[i];
        if (obj["id"] == v->var_idx) {
            arr.remove(i);
            break;
        }
    }

    /* Remove class if array is now empty */
    if (arr.size() == 0) {
        doc.remove(cls->class_name);
    }
}

/* --------------------------------------------------------
 * SEND ENTIRE JSON (ALL CLASSES)
 * -------------------------------------------------------- */
void json_send(void)
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Not connected, JSON dropped");
        return;
    }

    if (!mqtt_manager_connected()) {
        Serial.println("[MQTT] Not connected, JSON dropped");
        return;
    }

    if (doc.isNull() || doc.size() == 0) {
        //Serial.println("⚠️ JSON empty, nothing to send");
        return;
    }

    String payload;
    serializeJson(doc, payload);

    Serial.println("[MQTT] Publishing JSON:");
    Serial.println(payload);
    Serial.print("[MQTT] Payload size: ");
    Serial.println(payload.length());

    bool ok = mqtt_manager_publish(settings_mqtt.Data_Topic, payload.c_str(), true);

    if (ok) {
        Serial.println("[MQTT] Publish OK");
    } else {
        Serial.println("[MQTT] Publish FAILED");
    }
}

void json_send_http(void)
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi not connected, JSON dropped");
        return;
    }

    if (doc.isNull() || doc.size() == 0) {
        Serial.println("⚠️ JSON empty, nothing to send");
        return;
    }

    String body;
    serializeJson(doc, body);
    if (doc.overflowed()) {
    Serial.println("❌ JSON document overflowed!");
}


    Serial.println("➡ Sending JSON:");
    Serial.println(body);

    HTTPClient http;
    http.begin(api_url);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(body);

    if (code > 0) {
        Serial.print("✅ HTTP ");
        Serial.println(code);
        Serial.println(http.getString());
    } else {
        Serial.print("❌ HTTP error: ");
        Serial.println(http.errorToString(code));
    }

    http.end();
}

static void send_response(const char *status,
                          const char *class_name,
                          const char *var_name,
                          uint16_t var_idx,
                          const char *message)
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi not connected, response not sent");
        return;
    }

    StaticJsonDocument<256> resp;

    resp["status"] = status;
    resp["class"]  = class_name ? class_name : "";
    resp["var"]    = var_name ? var_name : "";
    resp["id"]     = var_idx;
    resp["msg"]    = message ? message : "";

    String body;
    serializeJson(resp, body);

    Serial.println("➡ Sending response:");
    Serial.println(body);

    HTTPClient http;
    http.begin(api_url);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(body);

    if (code > 0) {
        Serial.print("✅ Response HTTP ");
        Serial.println(code);
        Serial.println(http.getString());
    } else {
        Serial.print("❌ Response failed: ");
        Serial.println(http.errorToString(code));
    }

    http.end();
}
bool delete_variable(uint16_t var_idx,
                     const char *var_name,
                     const char *class_name)
{
    /* ----------------------------------------------------
     * BASIC INDEX VALIDATION
     * ---------------------------------------------------- */
    if (var_idx >= MAX_VAR || !used_var[var_idx]) {
        send_response("error", class_name, var_name, var_idx,
                      "invalid or unused var_idx");
        return false;
    }

    var_t   *v   = &var_pool[var_idx];
    class_t *cls = &class_pool[v->class_idx];

    /* ----------------------------------------------------
     * NAME VERIFICATION
     * ---------------------------------------------------- */
    if (!v->var_name || strcmp(v->var_name, var_name) != 0) {
        send_response("error", cls->class_name, var_name, var_idx,
                      "var_name mismatch");
        return false;
    }

    if (!cls->class_name || strcmp(cls->class_name, class_name) != 0) {
        send_response("error", class_name, v->var_name, var_idx,
                      "class_name mismatch");
        return false;
    }

    /* ----------------------------------------------------
     * REMOVE FROM DATA STRUCTURES
     * ---------------------------------------------------- */
    remove_variable(v->ext_addr);
    sync_class(v->class_idx);

    /* ----------------------------------------------------
     * SUCCESS RESPONSE
     * ---------------------------------------------------- */
    send_response("done", class_name, var_name, var_idx,
                  "variable removed");

    return true;
}
bool update_variable_telemetry(uint16_t var_idx,
                               const char *var_name,
                               const char *class_name,
                               float       new_value)
{
    /* ----------------------------------------------------
     * BASIC INDEX VALIDATION
     * ---------------------------------------------------- */
    if (var_idx >= MAX_VAR || !used_var[var_idx]) {
        send_response("error", class_name, var_name, var_idx,
                      "invalid or unused var_idx");
        return false;
    }

    var_t   *v   = &var_pool[var_idx];
    class_t *cls = &class_pool[v->class_idx];

    /* ----------------------------------------------------
     * NAME VERIFICATION
     * ---------------------------------------------------- */
    if (!v->var_name || strcmp(v->var_name, var_name) != 0) {
        send_response("error",
                      cls->class_name,
                      var_name,
                      var_idx,
                      "var_name mismatch");
        return false;
    }

    if (!cls->class_name || strcmp(cls->class_name, class_name) != 0) {
        send_response("error",
                      class_name,
                      v->var_name,
                      var_idx,
                      "class_name mismatch");
        return false;
    }

    /* ----------------------------------------------------
     * UPDATE EXTERNAL VALUE
     * ---------------------------------------------------- */
    if (v->ext_addr == NULL) {
        send_response("error",
                      class_name,
                      var_name,
                      var_idx,
                      "external address NULL");
        return false;
    }

    *((float *)v->ext_addr) = new_value;

    /* ----------------------------------------------------
     * UPDATE DATA MANAGER (MRU + DIRTY)
     * ---------------------------------------------------- */
    update_variable(v->ext_addr);

    /* ----------------------------------------------------
     * SYNC CLASS (JSON + SEND)
     * ---------------------------------------------------- */
    sync_class(v->class_idx);

    /* ----------------------------------------------------
     * SUCCESS RESPONSE
     * ---------------------------------------------------- */
    send_response("done",
                  class_name,
                  var_name,
                  var_idx,
                  "variable updated");

    return true;
}

/* =========================================================
 * COMBINED POLL + RECEIVE
 * ========================================================= */

void json_receive(void)
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    HTTPClient http;
    http.begin(CMD_URL);
    http.addHeader("Content-Type", "application/json");

    int code = http.GET();
    if (code != 200) {
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    if (payload.length() == 0)
        return;

    Serial.println("⬅ Command received:");
    Serial.println(payload);

    /* -----------------------------------------------------
     * PARSE JSON
     * ----------------------------------------------------- */
    StaticJsonDocument<512> rx;
    if (deserializeJson(rx, payload)) {
        send_response("error", "", "", 0, "invalid JSON");
        return;
    }

    const char *cmd = rx["cmd"];
    if (!cmd) {
        send_response("error", "", "", 0, "missing cmd");
        return;
    }

    /* =====================================================
     * SET VAR
     * ===================================================== */
    if (strcmp(cmd, "set_var") == 0) {

        const char *cls   = rx["class"];
        const char *var   = rx["var"];
        const char *type  = rx["type"];
        float value       = rx["value"] | 0.0f;
        uint16_t cid      = rx["constraint_id"] | INVALID_INDEX;

        if (!cls || !var || !type) {
            send_response("error", "", "", 0, "missing fields");
            return;
        }

        float *ext = alloc_remote_value(value);
        if (!ext) {
            send_response("error", cls, var, 0, "value pool full");
            return;
        }

        variables_description_row_t new_row;
        new_row.Class         = (char*)cls;
        new_row.Name          = (char*)var;
        new_row.Type          = (char*)type;
        new_row.Value         = value;
        new_row.constraint_id = (cid == INVALID_INDEX) ? -1 : (int)cid;
        if (!dm_set_value(&new_row, ext)) {
            send_response("error", cls, var, 0, "dm_set_value failed");
            return;
        }

        send_response("done", cls, var, 0, "variable created");
        return;
    }

    /* =====================================================
     * UPDATE VAR
     * ===================================================== */
    if (strcmp(cmd, "update_var") == 0) {

        uint16_t id        = rx["id"] | INVALID_INDEX;
        const char *cls    = rx["class"];
        const char *var    = rx["var"];
        float value        = rx["value"] | 0.0f;

        if (id == INVALID_INDEX || !cls || !var) {
            send_response("error", "", "", id, "missing fields");
            return;
        }

        update_variable_telemetry(id, var, cls, value);
        return;
    }

    /* =====================================================
     * REMOVE VAR
     * ===================================================== */
    if (strcmp(cmd, "remove_var") == 0) {

        uint16_t id        = rx["id"] | INVALID_INDEX;
        const char *cls    = rx["class"];
        const char *var    = rx["var"];

        if (id == INVALID_INDEX || !cls || !var) {
            send_response("error", "", "", id, "missing fields");
            return;
        }

        delete_variable(id, var, cls);
        return;
    }

    /* =====================================================
     * UNKNOWN CMD
     * ===================================================== */
    send_response("error", "", "", 0, "unknown command");
}
