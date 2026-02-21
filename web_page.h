#pragma once

const char WEB_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>ACMS Master Portal</title>

<style>
body {
  font-family: Arial;
  margin: 0;
  height: 100vh;
  display: flex;
  flex-direction: column;
}

.half {
  flex: 1;
  padding: 12px;
  border-bottom: 2px solid #ccc;
  display: flex;
  flex-direction: column;
}

/* ===== FORM LAYOUT ===== */
.form-grid {
  display: flex;
  flex-wrap: wrap;
  gap: 14px;
  align-items: flex-end;
  justify-content: space-evenly;
}

.field {
  flex: 0 0 var(--field-width);   /* fixed width */
  min-width: 0;
}

.field label {
  display: block;
  text-align: center;
}

.field input {
  width: 100%;
  padding: 6px;
  box-sizing: border-box;
}

.field input.invalid {
  border: 2px solid red;
}

.error-msg {
  display: block;
  color: red;
  font-size: 11px;
  text-align: center;
  min-height: 14px;
}

.main-header {
  position: relative;
  text-align: center;
  margin: 15px 0;
}

.main-header h1 {
  margin: 0;
  display: inline;
}

#submit-area {
  position: absolute;
  right: 16px;
  top: 50%;
  transform: translateY(-50%);
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 4px;
}

.check-label {
  display: flex;
  align-items: center;
  gap: 5px;
  font-size: 12px;
  cursor: pointer;
  white-space: nowrap;
}

.category-header {
  text-align: center;
  margin-bottom: 10px;
  position: relative;
}

.toggle-btn {
  position: absolute;
  right: 0;
  top: 50%;
  transform: translateY(-50%);
  padding: 2px 8px;
  font-size: 12px;
  cursor: pointer;
}

.half.collapsed {
  flex: 0 0 auto;
}

/* ===== SUBCATEGORY ROWS ===== */
.subcat-row {
  display: flex;
  align-items: flex-end;
  gap: 10px;
  margin-bottom: 6px;
}

.subcat-label {
  writing-mode: vertical-rl;
  text-orientation: mixed;
  transform: rotate(180deg);
  font-size: 11px;
  font-weight: bold;
  text-transform: uppercase;
  color: #666;
  white-space: nowrap;
  padding: 2px 0;
}

.subcat-row .form-grid {
  flex: 1;
}


/* ===== TABLE ===== */
.table-wrap {
  flex: 1;
  overflow-y: auto;
  margin-top: 10px;
}

table {
  width: 100%;
  border-collapse: collapse;
}

th, td {
  border: 1px solid #ccc;
  padding: 6px;
  text-align: center;
}

th {
  background: #eee;
}

.delete {
  color: red;
  cursor: pointer;
  font-weight: bold;
}

/* ===== BUTTON ===== */
button {
  padding: 6px 16px;
  font-weight: bold;
  cursor: pointer;
}

.insert-btn {
  display: block;
  margin: 8px auto;
  padding: 3px 10px;
  font-size: 12px;
}

/* ===== SETTINGS BAR ===== */
.settings-bar {
  padding: 4px 12px 6px;
  border-bottom: 2px solid #ccc;
}

.settings-category {
  margin: 2px 0 4px;
}

.settings-compact .subcat-label {
  font-size: 10px;
}

.settings-form-grid {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  align-items: flex-end;
}

.settings-field {
  min-width: 110px;
}

.settings-field label {
  display: block;
  font-size: 11px;
  text-align: center;
}

.settings-field input {
  width: 100%;
  padding: 2px 5px;
  font-size: 11px;
  box-sizing: border-box;
}
</style>
</head>

<body>
<div class="main-header">
  <h1>ACMS Master Portal</h1>
  <div id="submit-area">
    <button id="submit-btn" onclick="submitAll()">Submit</button>
    <label class="check-label">
  <input type="checkbox" data-name="Download_a_copy" data-type="boolean">
  Download a copy
</label>

  </div>
</div>

<!-- ======== SETTINGS BAR (INJECTED FROM XSD) ======== -->
<div class="settings-bar">
  <h3 class="category-header settings-category">
    Settings
    <button class="toggle-btn" onclick="toggleSettingsSection('Settings', this)">&#9660;</button>
  </h3>
  <div id="Settings_collapsible" style="display:none">
  </div>
</div>


<!-- ======== TABLE BLOCKS (INJECTED FROM XSD) ======== -->

<div class="half collapsed">
  <h3 class="category-header">
    Metadata
    <button class="toggle-btn" onclick="toggleSection('Metadata', this)">&#9660;</button>
  </h3>
  <div id="Metadata_section" style="display:none">
  <div id="Metadata_form">
  <div class="form-grid">
    <div class="field">
      <label>Key</label>
      <input placeholder="float" data-type="float" data-name="Key" oninput="validateField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Message</label>
      <input placeholder="string" data-type="string" data-name="Message" oninput="validateField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Class</label>
      <input placeholder="string" data-type="string" data-name="Class" oninput="validateField(this); var ni=document.getElementById('Metadata_form').querySelector('[data-field="Name"]'); if(ni) validateValidityField(ni)">
      <span class="error-msg"></span>
    </div>
  </div>

  </div>

  <div class="table-wrap">
    <table>
    <thead><tr>
      <th>Key</th>
      <th>Message</th>
      <th>Class</th>
      <th>X</th>
    </tr></thead>
    <tbody id="Metadata_body"></tbody>
  </table>
  </div>

  <button type="button" class="insert-btn" onclick="insertRow('Metadata', Metadata_SCHEMA)">Insert</button>
  </div>
</div>

<div class="half">
  <h3 class="category-header">
    Variables
    <button class="toggle-btn" onclick="toggleSection('Variables', this)">&#9650;</button>
  </h3>
  <div id="Variables_section">
  <div id="Variables_form">
  <div class="subcat-row">
    <span class="subcat-label">description</span>
    <div class="form-grid">
    <div class="field">
      <label>Class</label>
      <input placeholder="string" data-type="string" data-name="Class" oninput="validateField(this); var ni=document.getElementById('Variables_form').querySelector('[data-field="Name"]'); if(ni) validateValidityField(ni)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Name</label>
      <input placeholder="string" data-type="string" data-name="Name" data-field="Name" onblur="alertIfInvalid(this)" oninput="validateValidityField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Type</label>
      <input placeholder="string" data-type="string" data-name="Type" oninput="validateField(this); var vi=document.getElementById('Variables_form').querySelector('[data-field="Value"]'); if(vi) validateValidityField(vi); var ni=document.getElementById('Variables_form').querySelector('[data-field="Name"]'); if(ni) validateValidityField(ni)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Value</label>
      <input placeholder="float" data-type="float" data-name="Value" data-field="Value" oninput="validateValidityField(this)">
      <span class="error-msg"></span>
    </div>
    </div>
  </div>
  <div class="subcat-row">
    <span class="subcat-label">constraints</span>
    <div class="form-grid">
    <div class="field">
      <label>Operation_ID</label>
      <input placeholder="float" data-type="float" data-name="Operation_ID" data-field="Operation_ID" oninput="validateValidityField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Threshold</label>
      <input placeholder="float" data-type="float" data-name="Threshold" oninput="validateField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Fault_Code</label>
      <input placeholder="float" data-type="float" data-name="Fault_Code" data-field="Fault_Code" oninput="validateValidityField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Increment</label>
      <input placeholder="float" data-type="float" data-name="Increment" oninput="validateField(this)">
      <span class="error-msg"></span>
    </div>
    </div>
  </div>
  <div class="subcat-row">
    <span class="subcat-label">modbus</span>
    <div class="form-grid">
    <div class="field">
      <label>Slave_ID</label>
      <input placeholder="float" data-type="float" data-name="Slave_ID" oninput="validateField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Function_ID</label>
      <input placeholder="float" data-type="float" data-name="Function_ID" oninput="validateField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Start_Address</label>
      <input placeholder="float" data-type="float" data-name="Start_Address" oninput="validateField(this)">
      <span class="error-msg"></span>
    </div>
    <div class="field">
      <label>Data_Length</label>
      <input placeholder="float" data-type="float" data-name="Data_Length" oninput="validateField(this)">
      <span class="error-msg"></span>
    </div>
    </div>
  </div>

  </div>

  <div class="table-wrap">
    <table>
    <thead><tr>
      <th>Class</th>
      <th>Name</th>
      <th>Type</th>
      <th>Value</th>
      <th>Operation_ID</th>
      <th>Threshold</th>
      <th>Fault_Code</th>
      <th>Increment</th>
      <th>Slave_ID</th>
      <th>Function_ID</th>
      <th>Start_Address</th>
      <th>Data_Length</th>
      <th>X</th>
    </tr></thead>
    <tbody id="Variables_body"></tbody>
  </table>
  </div>

  <button type="button" class="insert-btn" onclick="insertRow('Variables', Variables_SCHEMA)">Insert</button>
  </div>
</div>


<script>

/* ======== SCHEMA (INJECTED FROM XSD) ======== */
var Metadata_SCHEMA = [
  { name:"Key", type:"float" },
  { name:"Message", type:"string" },
  { name:"Class", type:"string" }
];

var Variables_SCHEMA = [
  { name:"Class", type:"string" },
  { name:"Name", type:"string" },
  { name:"Type", type:"string" },
  { name:"Value", type:"float" },
  { name:"Operation_ID", type:"float" },
  { name:"Threshold", type:"float" },
  { name:"Fault_Code", type:"float" },
  { name:"Increment", type:"float" },
  { name:"Slave_ID", type:"float" },
  { name:"Function_ID", type:"float" },
  { name:"Start_Address", type:"float" },
  { name:"Data_Length", type:"float" }
];

var TABLE_LIST = ["Metadata", "Variables"];


/* ======== PRELOADED XML (embedded at build time) ======== */
var PRELOAD_XML = {

};

/* ======== STABLE JS LOGIC (INJECTED FROM CODEGEN) ======== */
/* ======== STABLE JS LOGIC ======== */

let state = {};

/* ---------- INITIALIZATION ---------- */
window.addEventListener("DOMContentLoaded", () => {

  /* Compute MAX_COLUMNS safely (cap at 6 per row) */
  const schemas = TABLE_LIST.map(t => window[t + "_SCHEMA"]);
  const MAX_COLUMNS = Math.min(6, Math.max(...schemas.map(s => s.length)));

  const MIN_GAP = 14;

  /* Compute fixed field width */
  const FIELD_WIDTH = `calc((100% - ${(MAX_COLUMNS - 1) * MIN_GAP}px) / ${MAX_COLUMNS})`;

  document.documentElement.style.setProperty("--field-width", FIELD_WIDTH);

  /* Initialize each table; call logStateTables() once all are loaded */
  let _tablesLoaded = 0;
  function _onTableLoaded() {
    if (++_tablesLoaded === TABLE_LIST.length) logStateTables();
  }

  TABLE_LIST.forEach(table => {

    const schema = window[table + "_SCHEMA"];
    state[table] = [];

    buildForm(table);

    /* load from embedded XML (PC) or fetch from SPIFFS (ESP) */
    if (PRELOAD_XML[table]) {
      parseXML(PRELOAD_XML[table], table, schema);
      _onTableLoaded();
    } else {
      loadTable(table, schema, _onTableLoaded);
    }
  });

});


/* ---------- BUILD FORM ---------- */
function buildForm(table) {
  // Flex layout already handles spacing.
}


/* ---------- INSERT ROW ---------- */
function insertRow(table, schema) {

  if (!state[table]) state[table] = [];

  const inputs = document.querySelectorAll(`#${table}_form input`);

  /* validate all fields (no alerts — alertIfInvalid is only wired to onblur) */
  let valid = true;
  inputs.forEach(inp => {
    const fn = inp.dataset.field ? validateValidityField : validateField;
    if (!fn(inp)) valid = false;
  });
  if (!valid) return;

  const row = [];
  for (let i = 0; i < inputs.length; i++) {
    const v = inputs[i].value.trim();
    row.push(v === "" ? null : v);
    inputs[i].value = "";
    inputs[i].style.borderColor = "";
    inputs[i].classList.remove("invalid");
    inputs[i].nextElementSibling.textContent = "";
    inputs[i].nextElementSibling.style.color = "";
  }

  if (row.every(v => v === null)) return;

  state[table].push(row);
  renderTable(table, schema);
}


/* ---------- RENDER TABLE ---------- */
function renderTable(table, schema) {

  const body = document.getElementById(table + "_body");
  body.innerHTML = "";

  state[table].forEach((r, i) => {
    let tr = "<tr>";
    r.forEach(v => { tr += `<td>${v ?? ""}</td>`; });
    tr += `<td class="delete" onclick="deleteRow('${table}',${i})">X</td></tr>`;
    body.innerHTML += tr;
  });

  console.log("[ACMS] " + table + " state:", JSON.stringify(state[table]));
  if (table === "Metadata") {
    console.log("[ACMS] Metadata dict:", JSON.stringify(buildMetadataDict(), null, 2));
  }
  if (table === "Variables") {
    console.log("[ACMS] Variable dict:", JSON.stringify(buildVariableDict(), null, 2));
  }
}


/* ---------- STATE DUMP (printed on every page load / reload) ---------- */
function logStateTables() {
  console.group("[ACMS] State on page load");
  TABLE_LIST.forEach(t => {
    const schema = window[t + "_SCHEMA"];
    console.group(t + " (" + state[t].length + " rows)");
    console.table(
      state[t].map(row => {
        const obj = {};
        schema.forEach((f, i) => { obj[f.name] = row[i]; });
        return obj;
      })
    );
    console.groupEnd();
  });
  console.log("[ACMS] Metadata dict:", JSON.stringify(buildMetadataDict(), null, 2));
  console.log("[ACMS] Variable dict:", JSON.stringify(buildVariableDict(), null, 2));
  console.groupEnd();
}

/* Fire logStateTables() again when the page is restored from the back/forward cache. */
window.addEventListener("pageshow", function(event) {
  if (event.persisted) logStateTables();
});


/* ---------- DELETE ROW ---------- */
function deleteRow(table, i) {
  if (!state[table]) return;
  state[table].splice(i, 1);
  renderTable(table, window[table + "_SCHEMA"]);
}


/* ---------- XML TAG HELPER ---------- */
function xmlTag(name) {
  return name.replace(/ /g, "_");
}


/* ---------- CONVERT TO XML ---------- */
function rowsToXML(table, schema) {

  let xml = `<${table}>\n`;

  state[table].forEach(r => {
    let line = "<row>";
    schema.forEach((f, i) => {
      const tag = xmlTag(f.name);
      const v = r[i];
      line += v === null ? `<${tag}/>` : `<${tag}>${v}</${tag}>`;
    });
    xml += line + "</row>\n";
  });

  xml += `</${table}>`;
  return xml;
}


/* ---------- PARSE XML INTO TABLE ---------- */
function parseXML(xmlStr, table, schema) {
  const parser = new DOMParser();
  const doc = parser.parseFromString(xmlStr, "application/xml");
  const rows = doc.getElementsByTagName("row");

  if (!state[table]) state[table] = [];

  for (let r = 0; r < rows.length; r++) {
    const row = [];
    schema.forEach(f => {
      const el = rows[r].getElementsByTagName(xmlTag(f.name))[0];
      const v = el && el.textContent.trim();
      row.push(v ? v : null);
    });
    state[table].push(row);
  }

  renderTable(table, schema);
}


/* ---------- LOAD XML (SPIFFS on ESP / same dir on PC) ---------- */
function loadTable(table, schema, onDone) {
  const url = (location.hostname !== "") ? "/" + table + ".xml" : table + ".xml";
  fetch(url)
    .then(r => { if (r.ok) return r.text(); throw new Error("HTTP " + r.status); })
    .then(xml => { parseXML(xml, table, schema); if (onDone) onDone(); })
    .catch(e => { console.error("[ACMS] Failed to load " + table + ".xml:", e); if (onDone) onDone(); });
}


/* ---------- TOGGLE SECTION (Metadata / Variables) ---------- */
function toggleSection(table, btn) {
  const section = document.getElementById(table + "_section");
  const half    = btn.closest(".half");
  const hidden  = section.style.display === "none";
  section.style.display = hidden ? "" : "none";
  btn.innerHTML = hidden ? "&#9650;" : "&#9660;";
  half.classList.toggle("collapsed", !hidden);
}


/* ---------- TOGGLE SETTINGS SECTION ---------- */
/* Collapses all Settings subcategories except headers (always visible). */
function toggleSettingsSection(name, btn) {
  var collapsible = document.getElementById(name + "_collapsible");
  if (!collapsible) return;
  var hidden = collapsible.style.display === "none";
  collapsible.style.display = hidden ? "" : "none";
  btn.innerHTML = hidden ? "&#9650;" : "&#9660;";
}


/* ---------- WAIT FOR REBOOT ---------- */
async function waitForReboot(btn) {
  btn.textContent = "Rebooting...";
  await new Promise(r => setTimeout(r, 3000));

  const deadline = Date.now() + 30000;
  while (Date.now() < deadline) {
    btn.textContent = "Waiting...";
    try {
      const r = await fetch("/test", { cache: "no-store" });
      if (r.ok) { location.reload(); return; }
    } catch (e) { /* ESP still offline */ }
    await new Promise(r => setTimeout(r, 1500));
  }

  btn.disabled = false;
  btn.textContent = "Submit";
  alert("Reboot timed out — please refresh the page manually.");
}


/* ---------- CREATE ZIP (STORE, no compression, pure JS) ---------- */
function createZIP(files) {

  function crc32(bytes) {
    const t = new Uint32Array(256);
    for (let i = 0; i < 256; i++) {
      let c = i;
      for (let j = 0; j < 8; j++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
      t[i] = c;
    }
    let crc = 0xFFFFFFFF;
    for (let i = 0; i < bytes.length; i++) crc = t[(crc ^ bytes[i]) & 0xFF] ^ (crc >>> 8);
    return (crc ^ 0xFFFFFFFF) >>> 0;
  }

  const u16 = n => [n & 0xFF, (n >> 8) & 0xFF];
  const u32 = n => [n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, (n >> 24) & 0xFF];
  const enc = new TextEncoder();
  const sb  = s => Array.from(enc.encode(s));

  const locals = [], centrals = [];
  let offset = 0;

  files.forEach(({name, data}) => {
    const nb = sb(name), db = Array.from(enc.encode(data));
    const crc = crc32(db), size = db.length;

    const local = [
      0x50,0x4B,0x03,0x04,
      ...u16(20), ...u16(0), ...u16(0),
      ...u16(0), ...u16(0),
      ...u32(crc), ...u32(size), ...u32(size),
      ...u16(nb.length), ...u16(0),
      ...nb, ...db
    ];
    const central = [
      0x50,0x4B,0x01,0x02,
      ...u16(20), ...u16(20), ...u16(0), ...u16(0),
      ...u16(0), ...u16(0),
      ...u32(crc), ...u32(size), ...u32(size),
      ...u16(nb.length), ...u16(0), ...u16(0),
      ...u16(0), ...u16(0), ...u32(0), ...u32(offset),
      ...nb
    ];

    locals.push(local);
    centrals.push(central);
    offset += local.length;
  });

  const cdOffset = offset;
  const cdSize   = centrals.reduce((s, c) => s + c.length, 0);
  const eocd = [
    0x50,0x4B,0x05,0x06,
    ...u16(0), ...u16(0),
    ...u16(files.length), ...u16(files.length),
    ...u32(cdSize), ...u32(cdOffset),
    ...u16(0)
  ];

  return new Uint8Array([...locals.flat(), ...centrals.flat(), ...eocd]);
}


/* ---------- DOWNLOAD ZIP HELPER ---------- */
function downloadZIP() {
  const files = TABLE_LIST.map(t => ({
    name: t + ".xml",
    data: rowsToXML(t, window[t + "_SCHEMA"])
  }));
  const zip  = createZIP(files);
  const blob = new Blob([zip], { type: "application/zip" });
  const a    = document.createElement("a");
  a.href     = URL.createObjectURL(blob);
  a.download = "acms_xml.zip";
  document.body.appendChild(a);
  a.click();
  a.remove();
}


/* ---------- SUBMIT ---------- */
async function submitAll() {

  const isESP = location.hostname !== "";

  /* Read Download_a_copy checkbox from submit-area */
  var dlInput = document.querySelector('#submit-area [data-name="Download_a_copy"]');
  var dlChecked = dlInput && dlInput.checked;

  if (isESP) {
    const btn = document.getElementById("submit-btn");
    btn.disabled = true;
    btn.textContent = "Saving...";

    try {
      for (const t of TABLE_LIST) {
        const xml = rowsToXML(t, window[t + "_SCHEMA"]);
        const r = await fetch("/" + t + ".xml", {
          method: "POST",
          headers: { "Content-Type": "application/xml" },
          body: xml
        });
        if (!r.ok) throw new Error(t + " save failed (" + r.status + ")");
      }

      if (dlChecked) downloadZIP();

      await fetch("/reboot", { method: "POST" }).catch(() => {});
      await waitForReboot(btn);

    } catch(e) {
      btn.disabled = false;
      btn.textContent = "Submit";
      alert("Error: " + e.message);
    }

  } else {
    /* PC mode */
    if (dlChecked) {
      downloadZIP();
    } else {
      TABLE_LIST.filter(t => state[t].length > 0).forEach((t, idx) => {
        const xml = rowsToXML(t, window[t + "_SCHEMA"]);
        setTimeout(() => {
          const blob = new Blob([xml], { type: "application/xml" });
          const a = document.createElement("a");
          a.href = URL.createObjectURL(blob);
          a.download = t + ".xml";
          document.body.appendChild(a);
          a.click();
          a.remove();
        }, idx * 200);
      });
    }
  }
}



/* ======== FIELD VALIDATOR + VALIDITY CHECKER (INJECTED FROM CODEGEN) ======== */
/* ---------- FIELD VALIDATOR (generated from XSD) ---------- */
/* Types found: boolean, float, string */
function validateField(input) {
  const type = input.dataset.type;
  const v = input.value.trim();
  const errSpan = input.nextElementSibling;

  if (v === "") {
    input.classList.remove("invalid");
    errSpan.textContent = "";
    return true;
  }

  let msg = "";

  switch (type) {
    case "boolean":
      if (v !== "true" && v !== "false") msg = "Must be true or false";
      break;
    case "float":
      if (isNaN(v)) msg = "Must be a number";
      break;
  }

  if (msg) {
    input.classList.add("invalid");
    errSpan.textContent = msg;
    return false;
  }

  input.classList.remove("invalid");
  errSpan.textContent = "";
  return true;
}


/* ---------- VALIDITY CHECKER (generated from XSD) ---------- */

/* Build nested Metadata dict: { Class: { Key: Message } } */
function buildMetadataDict() {
  var dict = {};
  if (!state["Metadata"]) return dict;
  var schema = Metadata_SCHEMA;
  var classIdx = -1, keyIdx = -1, msgIdx = -1;
  for (var i = 0; i < schema.length; i++) {
    if (schema[i].name === "Class") classIdx = i;
    if (schema[i].name === "Key") keyIdx = i;
    if (schema[i].name === "Message") msgIdx = i;
  }
  if (classIdx < 0 || keyIdx < 0 || msgIdx < 0) return dict;
  state["Metadata"].forEach(function(row) {
    var cls = row[classIdx];
    var key = row[keyIdx];
    var msg = row[msgIdx];
    if (cls !== null) {
      if (!dict[cls]) dict[cls] = {};
      if (key !== null) dict[cls][key] = msg || "";
    }
  });
  return dict;
}

/* Build nested Variable dict: { Class: { Name: { TypeVal: { field: value, ... } } } } */
function buildVariableDict() {
  var dict = {};
  if (!state["Variables"]) return dict;
  var schema = (typeof Variables_SCHEMA !== "undefined") ? Variables_SCHEMA : [];
  var classIdx = -1, nameIdx = -1, typeIdx = -1;
  for (var i = 0; i < schema.length; i++) {
    if (schema[i].name === "Class") classIdx = i;
    else if (schema[i].name === "Name") nameIdx = i;
    else if (schema[i].name === "Type") typeIdx = i;
  }
  if (classIdx < 0 || nameIdx < 0 || typeIdx < 0) return dict;
  state["Variables"].forEach(function(row) {
    var cls = row[classIdx];
    var name = row[nameIdx];
    var type = row[typeIdx];
    if (cls === null || cls === undefined) return;
    if (!dict[cls]) dict[cls] = {};
    if (name === null || name === undefined) return;
    if (!dict[cls][name]) dict[cls][name] = {};
    if (type === null || type === undefined) return;
    var info = {};
    for (var i = 0; i < schema.length; i++) {
      if (i !== classIdx && i !== nameIdx && i !== typeIdx) {
        info[schema[i].name] = row[i];
      }
    }
    dict[cls][name][type] = info;
  });
  return dict;
}

/* Show alert once per error reason — only called from onblur, never from insertRow */
function alertIfInvalid(input) {
  var errSpan = input.nextElementSibling;
  var msg = errSpan ? errSpan.textContent : "";
  if (msg === "NO MATCH" || msg === "NO TYPE" || msg === "NO UNIT") {
    var half = input.closest(".half");
    var btn = half ? half.querySelector(".insert-btn") : null;
    if (btn) btn.disabled = true;
  }
  if (msg === "NO MATCH" && input.dataset.lastAlert !== "nomatch") {
    input.dataset.lastAlert = "nomatch";
    alert("Must Insert Variable Type And Unit First");
  } else if (msg === "NO TYPE" && input.dataset.lastAlert !== "notype") {
    input.dataset.lastAlert = "notype";
    alert("Must Insert Variable Type");
  } else if (msg === "NO UNIT" && input.dataset.lastAlert !== "nounit") {
    input.dataset.lastAlert = "nounit";
    alert("Must Insert Variable Unit (Use None if Variable has No Units)");
  }
}

/* Validate a single Validity form field */
function validateValidityField(input) {
  var fieldName = input.dataset.field;
  var v = input.value.trim();
  var errSpan = input.nextElementSibling;

  if (v === "") {
    if (fieldName === "Name") {
      var _h = input.closest(".half");
      var _b = _h ? _h.querySelector(".insert-btn") : null;
      if (_b) _b.disabled = false;
      input.dataset.lastAlert = "";
    }
    input.style.borderColor = "";
    input.classList.remove("invalid");
    errSpan.textContent = "";
    errSpan.style.color = "";
    return true;
  }

  var dict = buildMetadataDict();

  /* ── Name field: stepped check using nested Variable dict ── */
  if (fieldName === "Name") {
    var form = input.closest('[id$="_form"]');
    var typeInput = form ? form.querySelector('[data-name="Type"]') : null;
    var typeVal = typeInput ? typeInput.value.trim().toLowerCase() : "";
    var half = input.closest(".half");
    var insertBtn = half ? half.querySelector(".insert-btn") : null;
    if (typeVal === "type" || typeVal === "units" || typeVal === "verification") {
      input.style.borderColor = "";
      input.classList.remove("invalid");
      errSpan.textContent = "";
      errSpan.style.color = "";
      input.dataset.lastAlert = "";
      return true;
    }
    var classInput = form ? form.querySelector('[data-name="Class"]') : null;
    var classVal = classInput ? classInput.value.trim() : "";
    var varDict = buildVariableDict();

    /* ── Step 0: check class exists in Variables ── */
    var classKey = Object.keys(varDict).find(function(k) {
      return k.toLowerCase() === classVal.toLowerCase();
    });
    if (!classKey) {
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO MATCH";
      errSpan.style.color = "red";
      if (insertBtn) insertBtn.disabled = true;
      input.dataset.lastAlert = "";
      return false;
    }

    /* ── Step 1: check name exists in class ── */
    var nameKey = Object.keys(varDict[classKey]).find(function(k) {
      return k.toLowerCase() === v.toLowerCase();
    });
    if (!nameKey) {
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO MATCH";
      errSpan.style.color = "red";
      if (insertBtn) insertBtn.disabled = true;
      input.dataset.lastAlert = "";
      return false;
    }

    var nameDict = varDict[classKey][nameKey];

    /* ── Step 2: type row must exist ── */
    var typeRowKey = Object.keys(nameDict).find(function(k) {
      return k.toLowerCase() === "type";
    });
    if (!typeRowKey) {
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO TYPE";
      errSpan.style.color = "red";
      if (insertBtn) insertBtn.disabled = true;
      return false;
    }

    /* ── Step 3: resolve type label from Metadata ── */
    var tKeyVal = (nameDict[typeRowKey]["Value"] !== null && nameDict[typeRowKey]["Value"] !== undefined)
      ? String(nameDict[typeRowKey]["Value"]) : "";
    /* normalize float key: "1.0" → "1" so it matches Metadata Key column */
    var tKeyNorm = (tKeyVal !== "" && isFinite(tKeyVal)) ? String(parseFloat(tKeyVal)) : tKeyVal;
    var tClassKey = Object.keys(dict).find(function(k) {
      return k.toLowerCase() === "type";
    }) || "type";
    var typeLabel = (dict[tClassKey] && dict[tClassKey].hasOwnProperty(tKeyNorm))
      ? dict[tClassKey][tKeyNorm].toLowerCase() : "";
    input.style.borderColor = "green";
    input.classList.remove("invalid");
    errSpan.textContent = typeLabel.toUpperCase();
    errSpan.style.color = "green";
    input.dataset.lastAlert = "";

    /* ── Step 4: bool/choice → name already validated in Step 1 ── */
    if (typeLabel === "bool" || typeLabel === "choice") {
      if (insertBtn) insertBtn.disabled = false;
      return true;
    }

    /* ── Step 5: non-bool → unit row must exist ── */
    var unitRowKey = Object.keys(nameDict).find(function(k) {
      return k.toLowerCase() === "unit";
    });
    if (!unitRowKey) {
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO UNIT";
      errSpan.style.color = "red";
      if (insertBtn) insertBtn.disabled = true;
      return false;
    }

    /* ── Step 6: resolve unit message from Metadata (use classKey class only) ── */
    var uKeyVal = (nameDict[unitRowKey]["Value"] !== null && nameDict[unitRowKey]["Value"] !== undefined)
      ? String(nameDict[unitRowKey]["Value"]) : "";
    /* normalize float key: "1.0" → "1" so it matches Metadata Key column */
    var uKeyNorm = (uKeyVal !== "" && isFinite(uKeyVal)) ? String(parseFloat(uKeyVal)) : uKeyVal;
    var uClassKey = Object.keys(dict).find(function(k) {
      return k.toLowerCase() === classKey.toLowerCase();
    });
    var uMsg = (uClassKey && dict[uClassKey].hasOwnProperty(uKeyNorm))
      ? dict[uClassKey][uKeyNorm] : null;
    if (uMsg !== null) {
      input.style.borderColor = "green";
      input.classList.remove("invalid");
      errSpan.textContent = uMsg.toUpperCase();
      errSpan.style.color = "green";
      if (insertBtn) insertBtn.disabled = false;
      input.dataset.lastAlert = "";
      return true;
    }
    /* unit row present but key missing from Metadata */
    input.style.borderColor = "red";
    input.classList.add("invalid");
    errSpan.textContent = "NO UNIT";
    errSpan.style.color = "red";
    if (insertBtn) insertBtn.disabled = true;
    return false;
  }

  /* Value field: validate against Metadata when Type is 'unit' or 'type' */
  var classKey = fieldName;
  if (fieldName === "Value") {
    var form = input.closest("[id$=\"_form\"]");
    var typeInput = form.querySelector('[data-name="Type"]');
    var typeVal = typeInput ? typeInput.value.trim().toLowerCase() : "";

    if (typeVal === "unit") {
      /* class key = the Name field value (case-insensitive lookup in Metadata) */
      var nameInput = form.querySelector('[data-name="Name"]');
      var nameVal = nameInput ? nameInput.value.trim() : "";
      classKey = Object.keys(dict).find(function(k) { return k.toLowerCase() === nameVal.toLowerCase(); }) || nameVal;
    } else if (typeVal === "type") {
      /* class key = 'type'; Value must match a Key in Metadata where Class='type' */
      classKey = Object.keys(dict).find(function(k) { return k.toLowerCase() === "type"; }) || "type";
    } else {
      /* no Metadata validation for other Type values */
      input.style.borderColor = "";
      input.classList.remove("invalid");
      errSpan.textContent = "";
      errSpan.style.color = "";
      return true;
    }
  }

  /* Step 1: check class exists in Metadata */
  if (!dict[classKey]) {
    input.style.borderColor = "red";
    input.classList.add("invalid");
    errSpan.textContent = "NO MATCH";
    errSpan.style.color = "red";
    return false;
  }

  /* Step 2: check typed value is a key in the class dict */
  if (!dict[classKey].hasOwnProperty(v)) {
    input.style.borderColor = "red";
    input.classList.add("invalid");
    errSpan.textContent = "MISMATCH";
    errSpan.style.color = "red";
    return false;
  }

  /* Key found — highlight green, show message */
  input.style.borderColor = "green";
  input.classList.remove("invalid");
  errSpan.textContent = dict[classKey][v].toUpperCase();
  errSpan.style.color = "green";
  return true;
}



</script>

</body>
</html>

)HTML";
