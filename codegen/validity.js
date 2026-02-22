/* ---------- FIELD VALIDATOR (generated from XSD) ---------- */
/* Types found: boolean, float, integer, string */
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
    case "integer":
      if (!/^-?\d+$/.test(v)) msg = "Must be a whole number";
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
    if (typeVal === "type" || typeVal === "unit" || typeVal === "verification") {
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

    /* ── Step 6: resolve unit message from Metadata (use nameKey as Metadata class) ── */
    var uKeyVal = (nameDict[unitRowKey]["Value"] !== null && nameDict[unitRowKey]["Value"] !== undefined)
      ? String(nameDict[unitRowKey]["Value"]) : "";
    /* normalize float key: "1.0" → "1" so it matches Metadata Key column */
    var uKeyNorm = (uKeyVal !== "" && isFinite(uKeyVal)) ? String(parseFloat(uKeyVal)) : uKeyVal;
    var uClassKey = Object.keys(dict).find(function(k) {
      return k.toLowerCase() === nameKey.toLowerCase();
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
