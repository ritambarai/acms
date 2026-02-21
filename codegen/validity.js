/* ---------- FIELD VALIDATOR (generated from XSD) ---------- */
/* Types found: float, string */
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

  /* ── Name field: stepped check — type row → bool shortcut → unit row ── */
  if (fieldName === "Name") {
    var form = input.closest('[id$="_form"]');
    var typeInput = form ? form.querySelector('[data-name="Type"]') : null;
    var typeVal = typeInput ? typeInput.value.trim().toLowerCase() : "";
    var half = input.closest(".half");
    var insertBtn = half ? half.querySelector(".insert-btn") : null;
    if (typeVal === "type" || typeVal === "units" || typeVal === "validity") {
      input.style.borderColor = "";
      input.classList.remove("invalid");
      errSpan.textContent = "";
      errSpan.style.color = "";
      input.dataset.lastAlert = "";
      return true;
    }
    var classInput = form ? form.querySelector('[data-name="Class"]') : null;
    var classVal = classInput ? classInput.value.trim() : "";
    var varSchema = (typeof Variables_SCHEMA !== "undefined") ? Variables_SCHEMA : [];
    var classIdx = -1, typeIdx = -1, valueIdx = -1, nameIdx = -1;
    for (var i = 0; i < varSchema.length; i++) {
      if (varSchema[i].name === "Class") classIdx = i;
      if (varSchema[i].name === "Type")  typeIdx  = i;
      if (varSchema[i].name === "Value") valueIdx = i;
      if (varSchema[i].name === "Name")  nameIdx  = i;
    }
    var varRows = (state && state["Variables"]) ? state["Variables"] : [];
    /* gate on class existence first so type/unit alerts can fire */
    var classRows = varRows.filter(function(row) {
      return classIdx >= 0 && row[classIdx] && classVal &&
             row[classIdx].toLowerCase() === classVal.toLowerCase();
    });
    if (classRows.length === 0) {
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO MATCH";
      errSpan.style.color = "red";
      if (insertBtn) insertBtn.disabled = true;
      input.dataset.lastAlert = "";
      return false;
    }

    /* ── Step 1: type row must exist ── */
    var typeRow = null;
    for (var j = 0; j < classRows.length; j++) {
      if (typeIdx >= 0 && classRows[j][typeIdx] &&
          classRows[j][typeIdx].toLowerCase() === "type") {
        typeRow = classRows[j]; break;
      }
    }
    if (!typeRow) {
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO TYPE";
      errSpan.style.color = "red";
      if (insertBtn) insertBtn.disabled = true;
      return false;
    }

    /* ── Step 2: resolve type label from Metadata ── */
    var tClassKey = Object.keys(dict).find(function(k) {
      return k.toLowerCase() === "type";
    }) || "type";
    var tKeyVal = (typeRow[valueIdx] !== null && typeRow[valueIdx] !== undefined)
      ? String(typeRow[valueIdx]) : "";
    var typeLabel = (dict[tClassKey] && dict[tClassKey].hasOwnProperty(tKeyVal))
      ? dict[tClassKey][tKeyVal].toLowerCase() : "";
    input.style.borderColor = "green";
    input.classList.remove("invalid");
    errSpan.textContent = typeLabel.toUpperCase();
    errSpan.style.color = "green";
    input.dataset.lastAlert = "";

    /* ── Step 3: bool/choice → skip unit check, still validate name ── */
    if (typeLabel === "bool" || typeLabel === "choice") {
      var boolNameMatch = classRows.some(function(row) {
        return nameIdx >= 0 && row[nameIdx] && v &&
               row[nameIdx].toLowerCase() === v.toLowerCase();
      });
      if (!boolNameMatch) {
        input.style.borderColor = "red";
        input.classList.add("invalid");
        errSpan.textContent = "NO MATCH";
        errSpan.style.color = "red";
        if (insertBtn) insertBtn.disabled = true;
        input.dataset.lastAlert = "";
        return false;
      }
      if (insertBtn) insertBtn.disabled = false;
      return true;
    }

    /* ── Step 4: non-bool → unit row must exist ── */
    var unitRow = null;
    for (var k = 0; k < classRows.length; k++) {
      if (typeIdx >= 0 && classRows[k][typeIdx] &&
          classRows[k][typeIdx].toLowerCase() === "unit") {
        unitRow = classRows[k]; break;
      }
    }
    if (!unitRow) {
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO UNIT";
      errSpan.style.color = "red";
      if (insertBtn) insertBtn.disabled = true;
      return false;
    }

    /* ── Step 5: resolve unit message from Metadata (use classVal class only) ── */
    var uKeyVal = (unitRow[valueIdx] !== null && unitRow[valueIdx] !== undefined)
      ? String(unitRow[valueIdx]) : "";
    var uClassKey = Object.keys(dict).find(function(k) {
      return k.toLowerCase() === classVal.toLowerCase();
    });
    var uMsg = (uClassKey && dict[uClassKey].hasOwnProperty(uKeyVal))
      ? dict[uClassKey][uKeyVal] : null;
    if (uMsg !== null) {
      /* ── Step 5b: name must also exist in this class ── */
      var nameMatch = classRows.some(function(row) {
        return nameIdx >= 0 && row[nameIdx] && v &&
               row[nameIdx].toLowerCase() === v.toLowerCase();
      });
      if (!nameMatch) {
        input.style.borderColor = "red";
        input.classList.add("invalid");
        errSpan.textContent = "NO MATCH";
        errSpan.style.color = "red";
        if (insertBtn) insertBtn.disabled = true;
        input.dataset.lastAlert = "";
        return false;
      }
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
