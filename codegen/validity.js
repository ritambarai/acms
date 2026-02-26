/* ---------- INSERT BUTTON STATE HELPER ---------- */
/* Insert button stays enabled; validation + alertIfInvalid run at insert time. */
function updateInsertBtn(input) {}

/* ---------- FIELD VALIDATOR (generated from XSD) ---------- */
/* Types found: boolean, float, integer, string */
function validateField(input) {
  const type = input.dataset.type;
  const v = input.value.trim();
  const errSpan = input.nextElementSibling;

  if (v === "") {
    input.classList.remove("invalid");
    errSpan.textContent = "";
    updateInsertBtn(input);
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
    updateInsertBtn(input);
    return false;
  }

  input.classList.remove("invalid");
  errSpan.textContent = "";
  updateInsertBtn(input);
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

/* ── Type field: toggle between text input and type/unit dropdown ── */
function _typeFieldSet(nameInput, asDropdown, opts) {
  opts = opts || ["type", "unit"];
  var form = nameInput.closest('[id$="_form"]');
  if (!form) return;
  var typeEl = form.querySelector('[data-name="Type"]');
  if (!typeEl) return;
  var isSelect = typeEl.tagName === "SELECT";
  if (asDropdown && isSelect) {
    var curOpts = (typeEl.dataset.opts || "").split(",").sort().join(",");
    var newOpts = opts.slice().sort().join(",");
    if (curOpts === newOpts) return;
  }
  if (!asDropdown && !isSelect) return;
  var parent = typeEl.parentElement;
  var formId = form.id;
  var savedField = typeEl.dataset.field || "";
  var savedType  = typeEl.dataset.type  || "string";
  if (asDropdown) {
    var sel = document.createElement("select");
    sel.dataset.name = "Type";
    sel.dataset.type = savedType;
    sel.dataset.opts = opts.slice().sort().join(",");
    var emptyOpt = document.createElement("option");
    emptyOpt.value = ""; emptyOpt.textContent = "";
    sel.appendChild(emptyOpt);
    opts.forEach(function(opt) {
      var o = document.createElement("option");
      o.value = opt; o.textContent = opt;
      sel.appendChild(o);
    });
    sel.onchange = function() {
      validateField(this);
      var vi = document.getElementById(formId).querySelector('[data-field="Value"]');
      if (vi) validateValidityField(vi);
      var ni = document.getElementById(formId).querySelector('[data-field="Name"]');
      if (ni) validateValidityField(ni);
    };
    parent.replaceChild(sel, typeEl);
  } else {
    var inp = document.createElement("input");
    inp.dataset.name = "Type";
    inp.dataset.type = savedType;
    inp.placeholder = savedType;
    if (savedField) inp.dataset.field = savedField;
    inp.oninput = function() {
      validateField(this);
      var vi = document.getElementById(formId).querySelector('[data-field="Value"]');
      if (vi) validateValidityField(vi);
      var ni = document.getElementById(formId).querySelector('[data-field="Name"]');
      if (ni) validateValidityField(ni);
    };
    parent.replaceChild(inp, typeEl);
  }
}

/* Show alert once per error reason — only called from onblur, never from insertRow */
function alertIfInvalid(input) {
  var errSpan = input.nextElementSibling;
  var msg = errSpan ? errSpan.textContent : "";
  updateInsertBtn(input);
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
      input.dataset.lastAlert = "";
      _typeFieldSet(input, false);
    }
    input.style.borderColor = "";
    input.classList.remove("invalid");
    errSpan.textContent = "";
    errSpan.style.color = "";
    updateInsertBtn(input);
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
      updateInsertBtn(input);
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
      input.dataset.lastAlert = "";
      updateInsertBtn(input);
      return false;
    }

    /* ── Step 1: check name exists in class ── */
    var nameKey = Object.keys(varDict[classKey]).find(function(k) {
      return k.toLowerCase() === v.toLowerCase();
    });
    if (!nameKey) {
      _typeFieldSet(input, true, ["type"]);
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO MATCH";
      errSpan.style.color = "red";
      input.dataset.lastAlert = "";
      updateInsertBtn(input);
      return false;
    }

    var nameDict = varDict[classKey][nameKey];

    /* ── Step 2: type row must exist ── */
    var typeRowKey = Object.keys(nameDict).find(function(k) {
      return k.toLowerCase() === "type";
    });
    if (!typeRowKey) {
      _typeFieldSet(input, true);
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO TYPE";
      errSpan.style.color = "red";
      updateInsertBtn(input);
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
      _typeFieldSet(input, false);
      updateInsertBtn(input);
      return true;
    }

    /* ── Step 5: non-bool → unit row must exist ── */
    var unitRowKey = Object.keys(nameDict).find(function(k) {
      return k.toLowerCase() === "unit";
    });
    if (!unitRowKey) {
      _typeFieldSet(input, true, ["unit"]);
      input.style.borderColor = "red";
      input.classList.add("invalid");
      errSpan.textContent = "NO UNIT";
      errSpan.style.color = "red";
      updateInsertBtn(input);
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
      _typeFieldSet(input, false);
      input.style.borderColor = "green";
      input.classList.remove("invalid");
      errSpan.textContent = uMsg.toUpperCase();
      errSpan.style.color = "green";
      input.dataset.lastAlert = "";
      updateInsertBtn(input);
      return true;
    }
    /* unit row present but key missing from Metadata */
    input.style.borderColor = "red";
    input.classList.add("invalid");
    errSpan.textContent = "NO UNIT";
    errSpan.style.color = "red";
    updateInsertBtn(input);
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
      /* look up variable data type to decide how to treat Value */
      var clsInp = form.querySelector('[data-name="Class"]');
      var nmInp  = form.querySelector('[data-name="Name"]');
      var clsVal = clsInp ? clsInp.value.trim() : "";
      var nmVal  = nmInp  ? nmInp.value.trim()  : "";
      var varDict = buildVariableDict();
      var clsKey2 = Object.keys(varDict).find(function(k) { return k.toLowerCase() === clsVal.toLowerCase(); });
      var nmKey2  = clsKey2 ? Object.keys(varDict[clsKey2]).find(function(k) { return k.toLowerCase() === nmVal.toLowerCase(); }) : null;
      if (!nmKey2) {
        input.style.borderColor = "";
        input.classList.remove("invalid");
        errSpan.textContent = "";
        errSpan.style.color = "";
        updateInsertBtn(input);
        return true;
      }
      var nmDict = varDict[clsKey2][nmKey2];
      /* resolve type label */
      var tRK = Object.keys(nmDict).find(function(k) { return k.toLowerCase() === "type"; });
      var tLabel = "";
      if (tRK) {
        var tKV = (nmDict[tRK]["Value"] !== null && nmDict[tRK]["Value"] !== undefined) ? String(nmDict[tRK]["Value"]) : "";
        var tKN = (tKV !== "" && isFinite(tKV)) ? String(parseFloat(tKV)) : tKV;
        var tCK = Object.keys(dict).find(function(k) { return k.toLowerCase() === "type"; }) || "type";
        tLabel = (dict[tCK] && dict[tCK].hasOwnProperty(tKN)) ? dict[tCK][tKN].toLowerCase() : "";
      }
      if (tLabel === "choice") {
        /* validate Value as a choice key: Metadata[varName][value] = choice label */
        classKey = Object.keys(dict).find(function(k) { return k.toLowerCase() === nmKey2.toLowerCase(); }) || nmKey2;
        /* fall through to Metadata lookup below */
      } else {
        /* numeric/other: show unit as a green hint, no hard validation */
        var uRK = Object.keys(nmDict).find(function(k) { return k.toLowerCase() === "unit"; });
        var uMsg = null;
        if (uRK) {
          var uKV = (nmDict[uRK]["Value"] !== null && nmDict[uRK]["Value"] !== undefined) ? String(nmDict[uRK]["Value"]) : "";
          var uKN = (uKV !== "" && isFinite(uKV)) ? String(parseFloat(uKV)) : uKV;
          var uCK = Object.keys(dict).find(function(k) { return k.toLowerCase() === nmKey2.toLowerCase(); });
          uMsg = (uCK && dict[uCK].hasOwnProperty(uKN)) ? dict[uCK][uKN] : null;
        }
        input.style.borderColor = uMsg !== null ? "green" : "";
        input.classList.remove("invalid");
        errSpan.textContent = uMsg !== null ? uMsg.toUpperCase() : "";
        errSpan.style.color = uMsg !== null ? "green" : "";
        updateInsertBtn(input);
        return true;
      }
    }
  }

  /* Step 1: check class exists in Metadata */
  if (!dict[classKey]) {
    input.style.borderColor = "red";
    input.classList.add("invalid");
    errSpan.textContent = "NO MATCH";
    errSpan.style.color = "red";
    updateInsertBtn(input);
    return false;
  }

  /* Step 2: check typed value is a key in the class dict */
  if (!dict[classKey].hasOwnProperty(v)) {
    input.style.borderColor = "red";
    input.classList.add("invalid");
    errSpan.textContent = "MISMATCH";
    errSpan.style.color = "red";
    updateInsertBtn(input);
    return false;
  }

  /* Key found — highlight green, show message */
  input.style.borderColor = "green";
  input.classList.remove("invalid");
  errSpan.textContent = dict[classKey][v].toUpperCase();
  errSpan.style.color = "green";
  updateInsertBtn(input);
  return true;
}
