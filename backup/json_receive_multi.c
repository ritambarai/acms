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

    Serial.println("⬅ Command payload:");
    Serial.println(payload);

    StaticJsonDocument<1024> rx;
    DeserializationError err = deserializeJson(rx, payload);
    if (err) {
        send_response("error", "", "", 0, "invalid JSON");
        return;
    }

    /* -----------------------------------------------------
     * MULTI-COMMAND MODE
     * ----------------------------------------------------- */
    if (rx.containsKey("cmds")) {

        JsonArray cmds = rx["cmds"].as<JsonArray>();
        if (!cmds) {
            send_response("error", "", "", 0, "cmds is not array");
            return;
        }

        for (JsonObject cmdObj : cmds) {
            handle_single_command(cmdObj);
        }

        return;
    }

    /* -----------------------------------------------------
     * SINGLE COMMAND (BACKWARD COMPATIBLE)
     * ----------------------------------------------------- */
    handle_single_command(rx.as<JsonObject>());
}

static void handle_single_command(JsonObject rx)
{
    const char *cmd = rx["cmd"];
    if (!cmd) {
        send_response("error", "", "", 0, "missing cmd");
        return;
    }

    /* ================= SET VAR ================= */
    if (strcmp(cmd, "set_var") == 0) {

        const char *cls  = rx["class"];
        const char *var  = rx["var"];
        const char *type = rx["type"];
        float value      = rx["value"] | 0.0f;

        if (!cls || !var || !type) {
            send_response("error", "", "", 0, "missing fields");
            return;
        }

        float *ext = alloc_remote_value(value);
        if (!ext) {
            send_response("error", cls, var, 0, "value pool full");
            return;
        }

        if (!dm_set_value(cls, var, type, ext, value)) {
            send_response("error", cls, var, 0, "dm_set_value failed");
            return;
        }

        send_response("done", cls, var, 0, "variable created");
        return;
    }

    /* ================= UPDATE VAR ================= */
    if (strcmp(cmd, "update_var") == 0) {

        uint16_t id     = rx["id"] | INVALID_INDEX;
        const char *cls = rx["class"];
        const char *var = rx["var"];
        float value     = rx["value"] | 0.0f;

        if (id == INVALID_INDEX || !cls || !var) {
            send_response("error", "", "", id, "missing fields");
            return;
        }

        update_variable_telemetry(id, var, cls, value);
        return;
    }

    /* ================= REMOVE VAR ================= */
    if (strcmp(cmd, "remove_var") == 0) {

        uint16_t id     = rx["id"] | INVALID_INDEX;
        const char *cls = rx["class"];
        const char *var = rx["var"];

        if (id == INVALID_INDEX || !cls || !var) {
            send_response("error", "", "", id, "missing fields");
            return;
        }

        remove_variable(id, var, cls);
        return;
    }

    send_response("error", "", "", 0, "unknown command");
}

