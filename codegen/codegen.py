import json
import os
import shutil
import xml.etree.ElementTree as ET
import sys

XSD_NS = "{http://www.w3.org/2001/XMLSchema}"
MAX_ROWS = 128

# ── JS validation rules ──
TYPE_RULES = {
    "float":   ("isNaN(v)",         "Must be a number"),
    "double":  ("isNaN(v)",         "Must be a number"),
    "decimal": ("isNaN(v)",         "Must be a number"),
    "integer": ("!/^-?\\d+$/.test(v)", "Must be a whole number"),
    "int":     ("!/^-?\\d+$/.test(v)", "Must be a whole number"),
    "long":    ("!/^-?\\d+$/.test(v)", "Must be a whole number"),
    "short":   ("!/^-?\\d+$/.test(v)", "Must be a whole number"),
    "boolean": ('v !== "true" && v !== "false"', "Must be true or false"),
}

# ── C type mapping ──
C_TYPE_MAP = {
    "float":   "float",
    "double":  "double",
    "decimal": "float",
    "string":  "char*",
    "integer": "int32_t",
    "int":     "int32_t",
    "long":    "int64_t",
    "short":   "int16_t",
    "boolean": "bool",
}

# XSD types that are numeric (need atof/strtol validation)
C_NUMERIC_TYPES = {"float", "double", "decimal"}
C_INT_TYPES     = {"integer", "int", "long", "short"}


# ═══════════════════════════════════════════════════════
#  XSD PARSER
# ═══════════════════════════════════════════════════════

def parse_xsd(xsd_path):
    root = ET.parse(xsd_path).getroot()
    tables = {}
    subcategories = {}   # table -> OrderedDict { subcat_name: [field_name, ...] }

    for elem in root.findall(XSD_NS + "element"):
        table = elem.attrib["name"]
        fields = []
        subcats = {}

        # Find the <row> element
        row_elem = None
        for row in elem.iter(XSD_NS + "element"):
            if row.attrib.get("name") == "row":
                row_elem = row
                break
        if row_elem is None:
            tables[table] = fields
            continue

        # Get the row's complexType > sequence
        ct = row_elem.find(XSD_NS + "complexType")
        if ct is None:
            tables[table] = fields
            continue
        row_seq = ct.find(XSD_NS + "sequence")
        if row_seq is None:
            tables[table] = fields
            continue

        for child in row_seq.findall(XSD_NS + "element"):
            if "type" in child.attrib:
                # Direct field (no subcategory)
                fields.append((
                    child.attrib["name"],
                    child.attrib["type"].split(":")[1]
                ))
            else:
                # Subcategory wrapper — check if it's a group (contains sub-subcategories)
                # or a flat subcategory (contains direct typed fields)
                subcat_name = child.attrib["name"]
                subcat_field_names = []
                inner_ct = child.find(XSD_NS + "complexType")
                if inner_ct is not None:
                    inner_seq = inner_ct.find(XSD_NS + "sequence")
                    if inner_seq is not None:
                        inner_elements = inner_seq.findall(XSD_NS + "element")
                        # Group: ALL inner elements are themselves complex types (no direct type attr)
                        is_group = inner_elements and all("type" not in e.attrib for e in inner_elements)
                        if is_group:
                            # Each inner element is a sub-subcategory (e.g. wifi, mqtt inside network)
                            group_subcats = {}
                            for inner_el in inner_elements:
                                inner_name = inner_el.attrib["name"]
                                inner_field_names = []
                                inn_ct = inner_el.find(XSD_NS + "complexType")
                                if inn_ct is not None:
                                    inn_seq = inn_ct.find(XSD_NS + "sequence")
                                    if inn_seq is not None:
                                        for sf in inn_seq.findall(XSD_NS + "element"):
                                            if "type" in sf.attrib:
                                                fname = sf.attrib["name"]
                                                ftyp = sf.attrib["type"].split(":")[1]
                                                fields.append((fname, ftyp))
                                                inner_field_names.append(fname)
                                if inner_field_names:
                                    group_subcats[inner_name] = inner_field_names
                            if group_subcats:
                                subcats[subcat_name] = group_subcats  # dict value = group
                        else:
                            # Flat subcategory with direct typed fields
                            for sf in inner_elements:
                                if "type" in sf.attrib:
                                    name = sf.attrib["name"]
                                    typ = sf.attrib["type"].split(":")[1]
                                    fields.append((name, typ))
                                    subcat_field_names.append(name)
                            if subcat_field_names:
                                subcats[subcat_name] = subcat_field_names  # list value = flat

        tables[table] = fields
        if subcats:
            subcategories[table] = subcats

    return tables, subcategories


def parse_xsd_defaults(xsd_path):
    """Return {field_name: default_value} for every XSD element that carries a 'default' attribute."""
    root = ET.parse(xsd_path).getroot()
    defaults = {}
    for el in root.iter(XSD_NS + "element"):
        if "default" in el.attrib and "name" in el.attrib:
            defaults[el.attrib["name"]] = el.attrib["default"]
    return defaults


# ═══════════════════════════════════════════════════════
#  JS GENERATORS (unchanged)
# ═══════════════════════════════════════════════════════

def gen_schema_js(tables, settings_subcats=None):
    js = ""
    for t, fields in tables.items():
        js += f"var {t}_SCHEMA = [\n"
        js += ",\n".join(
            f'  {{ name:"{n}", type:"{typ}" }}'
            for n, typ in fields
        )
        js += "\n];\n\n"

    names = ", ".join(f'"{t}"' for t in tables)
    js += f"var TABLE_LIST = [{names}];\n"

    if settings_subcats:
        other = {k: v for k, v in settings_subcats.items() if k != "header"}
        if other:
            js += "\nvar Settings_SUBCATS = {\n"
            items = []
            for sc_name, sc_val in other.items():
                if isinstance(sc_val, dict):  # group: nested object
                    inner_items = []
                    for inner_name, inner_fields in sc_val.items():
                        fields_str = ", ".join(f'"{f}"' for f in inner_fields)
                        inner_items.append(f'    {inner_name}: [{fields_str}]')
                    items.append(f'  {sc_name}: {{\n' + ',\n'.join(inner_items) + '\n  }')
                else:  # flat: plain array
                    fields_str = ", ".join(f'"{f}"' for f in sc_val)
                    items.append(f'  {sc_name}: [{fields_str}]')
            js += ",\n".join(items)
            js += "\n};\n"

            # Flat ordered list of all field names for table column alignment
            all_cols = []
            for sc_val in other.values():
                if isinstance(sc_val, dict):
                    for inner_fields in sc_val.values():
                        all_cols.extend(inner_fields)
                else:
                    all_cols.extend(sc_val)
            cols_str = ", ".join(f'"{c}"' for c in all_cols)
            js += f"var Settings_TABLE_COLS = [{cols_str}];\n"

    return js


def gen_validator_js(tables):
    types_used = set()
    for fields in tables.values():
        for _, typ in fields:
            types_used.add(typ)

    cases = []
    for typ in sorted(types_used):
        rule = TYPE_RULES.get(typ)
        if rule:
            check, msg = rule
            cases.append(f'    case "{typ}":\n'
                         f'      if ({check}) msg = "{msg}";\n'
                         f'      break;')

    switch_block = ""
    if cases:
        switch_block = "  switch (type) {\n" + "\n".join(cases) + "\n  }\n"

    js = (
        "/* ---------- FIELD VALIDATOR (generated from XSD) ---------- */\n"
        f"/* Types found: {', '.join(sorted(types_used))} */\n"
        "function validateField(input) {\n"
        "  const type = input.dataset.type;\n"
        "  const v = input.value.trim();\n"
        "  const errSpan = input.nextElementSibling;\n"
        "\n"
        '  if (v === "") {\n'
        '    input.classList.remove("invalid");\n'
        '    errSpan.textContent = "";\n'
        "    return true;\n"
        "  }\n"
        "\n"
        '  let msg = "";\n'
        "\n"
        f"{switch_block}"
        "\n"
        "  if (msg) {\n"
        '    input.classList.add("invalid");\n'
        "    errSpan.textContent = msg;\n"
        "    return false;\n"
        "  }\n"
        "\n"
        '  input.classList.remove("invalid");\n'
        '  errSpan.textContent = "";\n'
        "  return true;\n"
        "}\n"
    )
    return js


def gen_table_blocks(tables, validity_field_names=None, subcategories=None, hidden_by_default=None):
    if validity_field_names is None:
        validity_field_names = set()
    if subcategories is None:
        subcategories = {}
    if hidden_by_default is None:
        hidden_by_default = set()
    html = ""
    for t, fields in tables.items():
        subcats = subcategories.get(t)
        is_hidden    = t in hidden_by_default
        half_class   = "half collapsed" if is_hidden else "half"
        btn_arrow    = "&#9660;" if is_hidden else "&#9650;"
        section_attr = ' style="display:none"' if is_hidden else ""

        html += f"""
<div class="{half_class}">
  <h3 class="category-header">
    {t}
    <button class="toggle-btn" onclick="toggleSection('{t}', this)">{btn_arrow}</button>
  </h3>
  <div id="{t}_section"{section_attr}>
  <div id="{t}_form">
"""

        def _render_field(n, typ):
            if n in validity_field_names:
                if n == "Name":
                    oninput = "validateValidityField(this)"
                    extra = f' data-field="{n}" onblur="alertIfInvalid(this)"'
                else:
                    oninput = "validateValidityField(this)"
                    extra = f' data-field="{n}"'
            elif n == "Class" and "Name" in validity_field_names:
                # Class change must re-run Name validation (Name checks matching Class rows)
                oninput = (
                    "validateField(this); "
                    f"var ni=document.getElementById('{t}_form').querySelector('[data-field=Name]'); "
                    "if(ni) validateValidityField(ni)"
                )
                extra = ""
            elif n == "Type" and ("Value" in validity_field_names or "Name" in validity_field_names):
                triggers = ["validateField(this)"]
                if "Value" in validity_field_names:
                    triggers.append(
                        f"var vi=document.getElementById('{t}_form').querySelector('[data-field=Value]'); "
                        "if(vi) validateValidityField(vi)"
                    )
                if "Name" in validity_field_names:
                    triggers.append(
                        f"var ni=document.getElementById('{t}_form').querySelector('[data-field=Name]'); "
                        "if(ni) validateValidityField(ni)"
                    )
                oninput = "; ".join(triggers)
                extra = ""
            else:
                oninput = "validateField(this)"
                extra = ""
            return (f'    <div class="field">\n'
                    f'      <label>{n}</label>\n'
                    f'      <input placeholder="{typ}" data-type="{typ}" data-name="{n}"{extra} oninput="{oninput}">\n'
                    f'      <span class="error-msg"></span>\n'
                    f'    </div>\n')

        if subcats:
            # Build a lookup: field_name -> (name, type)
            field_map = {n: (n, typ) for n, typ in fields}
            for subcat_name, subcat_fields in subcats.items():
                html += f'  <div class="subcat-row">\n'
                html += f'    <span class="subcat-label">{subcat_name}</span>\n'
                html += f'    <div class="form-grid">\n'
                for fn in subcat_fields:
                    n, typ = field_map[fn]
                    html += _render_field(n, typ)
                html += f'    </div>\n'
                html += f'  </div>\n'
        else:
            html += '  <div class="form-grid">\n'
            for n, typ in fields:
                html += _render_field(n, typ)
            html += '  </div>\n'

        html += f"""
  </div>

  <div class="table-wrap">
    <table>
    <thead><tr>
"""
        for n, _ in fields:
            html += f"      <th>{n}</th>\n"
        html += f"""      <th>X</th>
    </tr></thead>
    <tbody id="{t}_body"></tbody>
  </table>
  </div>

  <button type="button" class="insert-btn" onclick="insertRow('{t}', {t}_SCHEMA)">Insert</button>
  </div>
</div>
"""
    return html


# ═══════════════════════════════════════════════════════
#  VALIDITY GENERATORS
# ═══════════════════════════════════════════════════════

def gen_validity_js():
    """Generate validity.js — validates Validity fields against Metadata nested dict."""
    return (
        "/* ---------- VALIDITY CHECKER (generated from XSD) ---------- */\n"
        "\n"
        "/* Build nested Metadata dict: { Class: { Key: Message } } */\n"
        "function buildMetadataDict() {\n"
        "  var dict = {};\n"
        '  if (!state["Metadata"]) return dict;\n'
        "  var schema = Metadata_SCHEMA;\n"
        "  var classIdx = -1, keyIdx = -1, msgIdx = -1;\n"
        "  for (var i = 0; i < schema.length; i++) {\n"
        '    if (schema[i].name === "Class") classIdx = i;\n'
        '    if (schema[i].name === "Key") keyIdx = i;\n'
        '    if (schema[i].name === "Message") msgIdx = i;\n'
        "  }\n"
        "  if (classIdx < 0 || keyIdx < 0 || msgIdx < 0) return dict;\n"
        '  state["Metadata"].forEach(function(row) {\n'
        "    var cls = row[classIdx];\n"
        "    var key = row[keyIdx];\n"
        "    var msg = row[msgIdx];\n"
        "    if (cls !== null) {\n"
        "      if (!dict[cls]) dict[cls] = {};\n"
        '      if (key !== null) dict[cls][key] = msg || "";\n'
        "    }\n"
        "  });\n"
        "  return dict;\n"
        "}\n"
        "\n"
        "/* Build nested Variable dict: { Class: { Name: { TypeVal: { field: value, ... } } } } */\n"
        "function buildVariableDict() {\n"
        "  var dict = {};\n"
        '  if (!state["Variables"]) return dict;\n'
        '  var schema = (typeof Variables_SCHEMA !== "undefined") ? Variables_SCHEMA : [];\n'
        "  var classIdx = -1, nameIdx = -1, typeIdx = -1;\n"
        "  for (var i = 0; i < schema.length; i++) {\n"
        '    if (schema[i].name === "Class") classIdx = i;\n'
        '    else if (schema[i].name === "Name") nameIdx = i;\n'
        '    else if (schema[i].name === "Type") typeIdx = i;\n'
        "  }\n"
        "  if (classIdx < 0 || nameIdx < 0 || typeIdx < 0) return dict;\n"
        '  state["Variables"].forEach(function(row) {\n'
        "    var cls = row[classIdx];\n"
        "    var name = row[nameIdx];\n"
        "    var type = row[typeIdx];\n"
        "    if (cls === null || cls === undefined) return;\n"
        "    if (!dict[cls]) dict[cls] = {};\n"
        "    if (name === null || name === undefined) return;\n"
        "    if (!dict[cls][name]) dict[cls][name] = {};\n"
        "    if (type === null || type === undefined) return;\n"
        "    var info = {};\n"
        "    for (var i = 0; i < schema.length; i++) {\n"
        "      if (i !== classIdx && i !== nameIdx && i !== typeIdx) {\n"
        "        info[schema[i].name] = row[i];\n"
        "      }\n"
        "    }\n"
        "    dict[cls][name][type] = info;\n"
        "  });\n"
        "  return dict;\n"
        "}\n"
        "\n"
        "/* Show alert once per error reason — only called from onblur, never from insertRow */\n"
        "function alertIfInvalid(input) {\n"
        "  var errSpan = input.nextElementSibling;\n"
        "  var msg = errSpan ? errSpan.textContent : \"\";\n"
        '  if (msg === "NO MATCH" || msg === "NO TYPE" || msg === "NO UNIT") {\n'
        '    var half = input.closest(".half");\n'
        '    var btn = half ? half.querySelector(".insert-btn") : null;\n'
        "    if (btn) btn.disabled = true;\n"
        "  }\n"
        '  if (msg === "NO MATCH" && input.dataset.lastAlert !== "nomatch") {\n'
        '    input.dataset.lastAlert = "nomatch";\n'
        '    alert("Must Insert Variable Type And Unit First");\n'
        '  } else if (msg === "NO TYPE" && input.dataset.lastAlert !== "notype") {\n'
        '    input.dataset.lastAlert = "notype";\n'
        '    alert("Must Insert Variable Type");\n'
        "  } else if (msg === \"NO UNIT\" && input.dataset.lastAlert !== \"nounit\") {\n"
        '    input.dataset.lastAlert = "nounit";\n'
        '    alert("Must Insert Variable Unit (Use None if Variable has No Units)");\n'
        "  }\n"
        "}\n"
        "\n"
        "/* Validate a single Validity form field */\n"
        "function validateValidityField(input) {\n"
        "  var fieldName = input.dataset.field;\n"
        "  var v = input.value.trim();\n"
        "  var errSpan = input.nextElementSibling;\n"
        "\n"
        '  if (v === "") {\n'
        '    if (fieldName === "Name") {\n'
        '      var _h = input.closest(".half");\n'
        '      var _b = _h ? _h.querySelector(".insert-btn") : null;\n'
        '      if (_b) _b.disabled = false;\n'
        '      input.dataset.lastAlert = "";\n'
        '    }\n'
        '    input.style.borderColor = "";\n'
        '    input.classList.remove("invalid");\n'
        '    errSpan.textContent = "";\n'
        '    errSpan.style.color = "";\n'
        "    return true;\n"
        "  }\n"
        "\n"
        "  var dict = buildMetadataDict();\n"
        "\n"
        "  /* ── Name field: stepped check using nested Variable dict ── */\n"
        '  if (fieldName === "Name") {\n'
        '    var form = input.closest(\'[id$="_form"]\');\n'
        '    var typeInput = form ? form.querySelector(\'[data-name="Type"]\') : null;\n'
        '    var typeVal = typeInput ? typeInput.value.trim().toLowerCase() : "";\n'
        '    var half = input.closest(".half");\n'
        '    var insertBtn = half ? half.querySelector(".insert-btn") : null;\n'
        '    if (typeVal === "type" || typeVal === "unit" || typeVal === "verification") {\n'
        '      input.style.borderColor = "";\n'
        '      input.classList.remove("invalid");\n'
        '      errSpan.textContent = "";\n'
        '      errSpan.style.color = "";\n'
        '      input.dataset.lastAlert = "";\n'
        "      return true;\n"
        "    }\n"
        '    var classInput = form ? form.querySelector(\'[data-name="Class"]\') : null;\n'
        '    var classVal = classInput ? classInput.value.trim() : "";\n'
        "    var varDict = buildVariableDict();\n"
        "\n"
        "    /* ── Step 0: check class exists in Variables ── */\n"
        "    var classKey = Object.keys(varDict).find(function(k) {\n"
        "      return k.toLowerCase() === classVal.toLowerCase();\n"
        "    });\n"
        "    if (!classKey) {\n"
        '      input.style.borderColor = "red";\n'
        '      input.classList.add("invalid");\n'
        '      errSpan.textContent = "NO MATCH";\n'
        '      errSpan.style.color = "red";\n'
        "      if (insertBtn) insertBtn.disabled = true;\n"
        '      input.dataset.lastAlert = "";\n'
        "      return false;\n"
        "    }\n"
        "\n"
        "    /* ── Step 1: check name exists in class ── */\n"
        "    var nameKey = Object.keys(varDict[classKey]).find(function(k) {\n"
        "      return k.toLowerCase() === v.toLowerCase();\n"
        "    });\n"
        "    if (!nameKey) {\n"
        '      input.style.borderColor = "red";\n'
        '      input.classList.add("invalid");\n'
        '      errSpan.textContent = "NO MATCH";\n'
        '      errSpan.style.color = "red";\n'
        "      if (insertBtn) insertBtn.disabled = true;\n"
        '      input.dataset.lastAlert = "";\n'
        "      return false;\n"
        "    }\n"
        "\n"
        "    var nameDict = varDict[classKey][nameKey];\n"
        "\n"
        "    /* ── Step 2: type row must exist ── */\n"
        "    var typeRowKey = Object.keys(nameDict).find(function(k) {\n"
        '      return k.toLowerCase() === "type";\n'
        "    });\n"
        "    if (!typeRowKey) {\n"
        '      input.style.borderColor = "red";\n'
        '      input.classList.add("invalid");\n'
        '      errSpan.textContent = "NO TYPE";\n'
        '      errSpan.style.color = "red";\n'
        "      if (insertBtn) insertBtn.disabled = true;\n"
        "      return false;\n"
        "    }\n"
        "\n"
        "    /* ── Step 3: resolve type label from Metadata ── */\n"
        '    var tKeyVal = (nameDict[typeRowKey]["Value"] !== null && nameDict[typeRowKey]["Value"] !== undefined)\n'
        '      ? String(nameDict[typeRowKey]["Value"]) : "";\n'
        "    /* normalize float key: \"1.0\" → \"1\" so it matches Metadata Key column */\n"
        "    var tKeyNorm = (tKeyVal !== \"\" && isFinite(tKeyVal)) ? String(parseFloat(tKeyVal)) : tKeyVal;\n"
        "    var tClassKey = Object.keys(dict).find(function(k) {\n"
        '      return k.toLowerCase() === "type";\n'
        '    }) || "type";\n'
        "    var typeLabel = (dict[tClassKey] && dict[tClassKey].hasOwnProperty(tKeyNorm))\n"
        "      ? dict[tClassKey][tKeyNorm].toLowerCase() : \"\";\n"
        '    input.style.borderColor = "green";\n'
        '    input.classList.remove("invalid");\n'
        "    errSpan.textContent = typeLabel.toUpperCase();\n"
        '    errSpan.style.color = "green";\n'
        '    input.dataset.lastAlert = "";\n'
        "\n"
        "    /* ── Step 4: bool/choice → name already validated in Step 1 ── */\n"
        '    if (typeLabel === "bool" || typeLabel === "choice") {\n'
        "      if (insertBtn) insertBtn.disabled = false;\n"
        "      return true;\n"
        "    }\n"
        "\n"
        "    /* ── Step 5: non-bool → unit row must exist ── */\n"
        "    var unitRowKey = Object.keys(nameDict).find(function(k) {\n"
        '      return k.toLowerCase() === "unit";\n'
        "    });\n"
        "    if (!unitRowKey) {\n"
        '      input.style.borderColor = "red";\n'
        '      input.classList.add("invalid");\n'
        '      errSpan.textContent = "NO UNIT";\n'
        '      errSpan.style.color = "red";\n'
        "      if (insertBtn) insertBtn.disabled = true;\n"
        "      return false;\n"
        "    }\n"
        "\n"
        "    /* ── Step 6: resolve unit message from Metadata (use nameKey as Metadata class) ── */\n"
        '    var uKeyVal = (nameDict[unitRowKey]["Value"] !== null && nameDict[unitRowKey]["Value"] !== undefined)\n'
        '      ? String(nameDict[unitRowKey]["Value"]) : "";\n'
        "    /* normalize float key: \"1.0\" → \"1\" so it matches Metadata Key column */\n"
        "    var uKeyNorm = (uKeyVal !== \"\" && isFinite(uKeyVal)) ? String(parseFloat(uKeyVal)) : uKeyVal;\n"
        "    var uClassKey = Object.keys(dict).find(function(k) {\n"
        "      return k.toLowerCase() === nameKey.toLowerCase();\n"
        "    });\n"
        "    var uMsg = (uClassKey && dict[uClassKey].hasOwnProperty(uKeyNorm))\n"
        "      ? dict[uClassKey][uKeyNorm] : null;\n"
        "    if (uMsg !== null) {\n"
        '      input.style.borderColor = "green";\n'
        '      input.classList.remove("invalid");\n'
        "      errSpan.textContent = uMsg.toUpperCase();\n"
        '      errSpan.style.color = "green";\n'
        "      if (insertBtn) insertBtn.disabled = false;\n"
        '      input.dataset.lastAlert = "";\n'
        "      return true;\n"
        "    }\n"
        "    /* unit row present but key missing from Metadata */\n"
        '    input.style.borderColor = "red";\n'
        '    input.classList.add("invalid");\n'
        '    errSpan.textContent = "NO UNIT";\n'
        '    errSpan.style.color = "red";\n'
        "    if (insertBtn) insertBtn.disabled = true;\n"
        "    return false;\n"
        "  }\n"
        "\n"
        "  /* Value field: validate against Metadata when Type is 'unit' or 'type' */\n"
        "  var classKey = fieldName;\n"
        '  if (fieldName === "Value") {\n'
        '    var form = input.closest("[id$=\\"_form\\"]");\n'
        '    var typeInput = form.querySelector(\'[data-name=\"Type\"]\');\n'
        "    var typeVal = typeInput ? typeInput.value.trim().toLowerCase() : \"\";\n"
        "\n"
        '    if (typeVal === "unit") {\n'
        "      /* class key = the Name field value (case-insensitive lookup in Metadata) */\n"
        '      var nameInput = form.querySelector(\'[data-name=\"Name\"]\');\n'
        "      var nameVal = nameInput ? nameInput.value.trim() : \"\";\n"
        "      classKey = Object.keys(dict).find(function(k) { return k.toLowerCase() === nameVal.toLowerCase(); }) || nameVal;\n"
        '    } else if (typeVal === "type") {\n'
        "      /* class key = 'type'; Value must match a Key in Metadata where Class='type' */\n"
        "      classKey = Object.keys(dict).find(function(k) { return k.toLowerCase() === \"type\"; }) || \"type\";\n"
        "    } else {\n"
        "      /* no Metadata validation for other Type values */\n"
        '      input.style.borderColor = "";\n'
        '      input.classList.remove("invalid");\n'
        '      errSpan.textContent = "";\n'
        '      errSpan.style.color = "";\n'
        "      return true;\n"
        "    }\n"
        "  }\n"
        "\n"
        "  /* Step 1: check class exists in Metadata */\n"
        "  if (!dict[classKey]) {\n"
        '    input.style.borderColor = "red";\n'
        '    input.classList.add("invalid");\n'
        '    errSpan.textContent = "NO MATCH";\n'
        '    errSpan.style.color = "red";\n'
        "    return false;\n"
        "  }\n"
        "\n"
        "  /* Step 2: check typed value is a key in the class dict */\n"
        "  if (!dict[classKey].hasOwnProperty(v)) {\n"
        '    input.style.borderColor = "red";\n'
        '    input.classList.add("invalid");\n'
        '    errSpan.textContent = "MISMATCH";\n'
        '    errSpan.style.color = "red";\n'
        "    return false;\n"
        "  }\n"
        "\n"
        "  /* Key found — highlight green, show message */\n"
        '  input.style.borderColor = "green";\n'
        '  input.classList.remove("invalid");\n'
        "  errSpan.textContent = dict[classKey][v].toUpperCase();\n"
        '  errSpan.style.color = "green";\n'
        "  return true;\n"
        "}\n"
    )


# ═══════════════════════════════════════════════════════
#  C GENERATORS
# ═══════════════════════════════════════════════════════

def c_ident(name):
    """Ensure a name is a valid C identifier."""
    return name.replace(" ", "_").replace("-", "_")


def _gen_subcat_block(lines, t, subcat_name, subcat_fields, field_map, has_value_ptr=False, max_rows=MAX_ROWS, has_constraint_id=False):
    """Generate enum, struct, table for one subcategory of a table."""
    tl = t.lower()
    tu = t.upper()
    sl = subcat_name.lower()
    su = subcat_name.upper()
    prefix = f"{tl}_{sl}"
    PREFIX = f"{tu}_{su}"

    fields = [(fn, field_map[fn]) for fn in subcat_fields]
    n_cols = len(fields)

    # ── enum ──
    lines.append(f"typedef enum {{")
    for name, typ in fields:
        ci = c_ident(name).upper()
        lines.append(f"  COL_{PREFIX}_{ci}_{typ.upper()},")
    lines.append(f"}} {prefix}_col_type_t;")
    lines.append("")

    # ── count ──
    lines.append(f"#define {PREFIX}_COL_COUNT {n_cols}")
    lines.append("")

    # ── column_types array ──
    lines.append(f"static const {prefix}_col_type_t {prefix}_column_types[{PREFIX}_COL_COUNT] = {{")
    for name, typ in fields:
        ci = c_ident(name).upper()
        lines.append(f"  COL_{PREFIX}_{ci}_{typ.upper()},")
    lines.append("};")
    lines.append("")

    # ── column_names array ──
    lines.append(f"static const char *{prefix}_column_names[{PREFIX}_COL_COUNT] = {{")
    lines.append("  " + ", ".join(f'"{c_ident(n)}"' for n, _ in fields))
    lines.append("};")
    lines.append("")

    # ── row struct ──
    lines.append(f"typedef struct {{")
    for name, typ in fields:
        c_type = C_TYPE_MAP.get(typ, "char*")
        ci = c_ident(name)
        lines.append(f"  {c_type} {ci};")
    if has_value_ptr:
        lines.append(f"  float *value_ptr;   /* points to description struct Value field */")
    if has_constraint_id:
        lines.append(f"  int constraint_id;   /* index into constraints table, -1 if no constraints row */")
    lines.append(f"}} {prefix}_row_t;")
    lines.append("")

    # ── max rows ──
    lines.append(f"#define MAX_{PREFIX}_ROWS {max_rows}")
    lines.append("")

    # ── table struct ──
    lines.append(f"typedef struct {{")
    lines.append(f"  {prefix}_row_t rows[MAX_{PREFIX}_ROWS];")
    lines.append(f"  int count;")
    lines.append(f"  int version;")
    lines.append(f"}} {prefix}_table_t;")
    lines.append("")

    # ── extern ──
    lines.append(f"extern {prefix}_table_t {prefix}_table;")
    lines.append("")

    # ── validate function ──
    float_cases = []
    int_cases = []
    for name, typ in fields:
        ci = c_ident(name).upper()
        enum_val = f"COL_{PREFIX}_{ci}_{typ.upper()}"
        if typ in C_NUMERIC_TYPES:
            float_cases.append(enum_val)
        elif typ in C_INT_TYPES:
            int_cases.append(enum_val)

    lines.append(f"static inline bool validate_{prefix}_value({prefix}_col_type_t type, const char *v) {{")
    lines.append("  if (!v || v[0] == '\\0') return true;")

    if float_cases or int_cases:
        lines.append("  switch (type) {")
        if float_cases:
            for c in float_cases:
                lines.append(f"    case {c}:")
            lines.append("      return isfinite(atof(v));")
        if int_cases:
            for c in int_cases:
                lines.append(f"    case {c}:")
            lines.append("      { char *end; strtol(v, &end, 10); return *end == '\\0'; }")
        lines.append("    default:")
        lines.append("      return true;")
        lines.append("  }")
    else:
        lines.append("  return true;")

    lines.append("}")
    lines.append("")
    lines.append("")


def gen_c_schema_h(tables, subcategories=None, settings_subcats=None, settings_fields=None, max_rows=MAX_ROWS):
    """Generate schema.h — enums, structs, column arrays, validation."""
    if subcategories is None:
        subcategories = {}
    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <stdbool.h>")
    lines.append("#include <stdint.h>")
    lines.append("#include <stdlib.h>")
    lines.append("#include <math.h>")
    lines.append("#include <string.h>")
    lines.append("")

    for t, fields in tables.items():
        tl = t.lower()
        tu = t.upper()
        subcats = subcategories.get(t)

        lines.append(f"/* ═══════ {t} ═══════ */")
        lines.append("")

        if subcats:
            # Build field_name -> type lookup
            field_map = {n: typ for n, typ in fields}
            subcat_names = list(subcats.keys())

            for sc_name in subcat_names:
                sc_fields = subcats[sc_name]
                has_value_ptr = sc_name.lower() in ("modbus", "constraints")
                has_constraint_id = "Value" in subcats[sc_name]
                lines.append(f"/* ─── {t} / {sc_name} ─── */")
                lines.append("")
                _gen_subcat_block(lines, t, sc_name, sc_fields, field_map, has_value_ptr, max_rows, has_constraint_id)
        else:
            # No subcategories — flat struct (original behaviour)
            n_cols = len(fields)

            lines.append(f"typedef enum {{")
            for name, typ in fields:
                ci = c_ident(name).upper()
                lines.append(f"  COL_{tu}_{ci}_{typ.upper()},")
            lines.append(f"}} {tl}_col_type_t;")
            lines.append("")

            lines.append(f"#define {tu}_COL_COUNT {n_cols}")
            lines.append("")

            lines.append(f"static const {tl}_col_type_t {tl}_column_types[{tu}_COL_COUNT] = {{")
            for name, typ in fields:
                ci = c_ident(name).upper()
                lines.append(f"  COL_{tu}_{ci}_{typ.upper()},")
            lines.append("};")
            lines.append("")

            lines.append(f"static const char *{tl}_column_names[{tu}_COL_COUNT] = {{")
            lines.append("  " + ", ".join(f'"{c_ident(n)}"' for n, _ in fields))
            lines.append("};")
            lines.append("")

            lines.append(f"typedef struct {{")
            for name, typ in fields:
                c_type = C_TYPE_MAP.get(typ, "char*")
                ci = c_ident(name)
                lines.append(f"  {c_type} {ci};")
            lines.append(f"}} {tl}_row_t;")
            lines.append("")

            lines.append(f"#define MAX_{tu}_ROWS {max_rows}")
            lines.append("")

            lines.append(f"typedef struct {{")
            lines.append(f"  {tl}_row_t rows[MAX_{tu}_ROWS];")
            lines.append(f"  int count;")
            lines.append(f"  int version;")
            lines.append(f"}} {tl}_table_t;")
            lines.append("")

            lines.append(f"extern {tl}_table_t {tl}_table;")
            lines.append("")

            float_cases = []
            int_cases = []
            for name, typ in fields:
                ci = c_ident(name).upper()
                enum_val = f"COL_{tu}_{ci}_{typ.upper()}"
                if typ in C_NUMERIC_TYPES:
                    float_cases.append(enum_val)
                elif typ in C_INT_TYPES:
                    int_cases.append(enum_val)

            lines.append(f"static inline bool validate_{tl}_value({tl}_col_type_t type, const char *v) {{")
            lines.append("  if (!v || v[0] == '\\0') return true;")

            if float_cases or int_cases:
                lines.append("  switch (type) {")
                if float_cases:
                    for c in float_cases:
                        lines.append(f"    case {c}:")
                    lines.append("      return isfinite(atof(v));")
                if int_cases:
                    for c in int_cases:
                        lines.append(f"    case {c}:")
                    lines.append("      { char *end; strtol(v, &end, 10); return *end == '\\0'; }")
                lines.append("    default:")
                lines.append("      return true;")
                lines.append("  }")
            else:
                lines.append("  return true;")

            lines.append("}")
            lines.append("")
            lines.append("")

    # ── Settings: singleton structs (one per subcategory, no rows[] array) ──
    if settings_subcats and settings_fields:
        field_map_s = {n: typ for n, typ in settings_fields}
        other_subcats_s = {k: v for k, v in settings_subcats.items() if k != "header"}
        if other_subcats_s:
            lines.append("/* ═══════ Settings ═══════ */")
            lines.append("")
            for sc_name, sc_val in other_subcats_s.items():
                if isinstance(sc_val, dict):  # group: generate struct per inner subcat
                    for inner_name, inner_fields in sc_val.items():
                        isl = inner_name.lower()
                        lines.append(f"/* ─── Settings / {sc_name} / {inner_name} ─── */")
                        lines.append("")
                        lines.append("typedef struct {")
                        for fn in inner_fields:
                            typ = field_map_s.get(fn, "string")
                            c_type = C_TYPE_MAP.get(typ, "char*")
                            ci = c_ident(fn)
                            lines.append(f"  {c_type} {ci};")
                        lines.append(f"}} settings_{isl}_t;")
                        lines.append("")
                        lines.append(f"extern settings_{isl}_t settings_{isl};")
                        lines.append("")
                        lines.append("")
                else:  # flat subcat: existing behaviour
                    sl = sc_name.lower()
                    lines.append(f"/* ─── Settings / {sc_name} ─── */")
                    lines.append("")
                    lines.append("typedef struct {")
                    for fn in sc_val:
                        typ = field_map_s.get(fn, "string")
                        c_type = C_TYPE_MAP.get(typ, "char*")
                        ci = c_ident(fn)
                        lines.append(f"  {c_type} {ci};")
                    lines.append(f"}} settings_{sl}_t;")
                    lines.append("")
                    lines.append(f"extern settings_{sl}_t settings_{sl};")
                    lines.append("")
                    lines.append("")

            # ── Pool-size accessors: default to 32 / 128 when struct value is 0 ──
            # Find which subcat (flat or inside a group named "schema") holds the pool fields.
            schema_fields = []
            pool_struct = "settings_schema"  # fallback for backward compat
            for sc_name, sc_val in other_subcats_s.items():
                if isinstance(sc_val, dict):
                    if "schema" in sc_val:
                        schema_fields = sc_val["schema"]
                        pool_struct = "settings_schema"
                        break
                else:  # flat subcat — search for pool-size fields directly
                    if "Var_Pool_Size" in sc_val or "Class_Pool_Size" in sc_val:
                        schema_fields = sc_val
                        pool_struct = f"settings_{sc_name.lower()}"
                        break
            if "Var_Pool_Size" in schema_fields or "Class_Pool_Size" in schema_fields:
                lines.append("/* ── Runtime pool-size accessors (defaults when struct value is 0) ── */")
                lines.append("")
                if "Var_Pool_Size" in schema_fields:
                    lines.append("static inline int32_t effective_var_pool_size(void) {")
                    lines.append(f"  return {pool_struct}.Var_Pool_Size > 0 ? {pool_struct}.Var_Pool_Size : 128;")
                    lines.append("}")
                    lines.append("")
                if "Class_Pool_Size" in schema_fields:
                    lines.append("static inline int32_t effective_class_pool_size(void) {")
                    lines.append(f"  return {pool_struct}.Class_Pool_Size > 0 ? {pool_struct}.Class_Pool_Size : 32;")
                    lines.append("}")
                    lines.append("")

    return "\n".join(lines)


def _gen_field_extract(lines, name, typ, prefix, PREFIX, tbl_var, idx="count"):
    """Generate extract + assign for one field inside a parse loop."""
    ci = c_ident(name)
    ci_upper = ci.upper()
    enum_val = f"COL_{PREFIX}_{ci_upper}_{typ.upper()}"

    lines.append(f'    extract_tag(row_start, "{ci}", buf, sizeof(buf));')
    lines.append(f"    if (buf[0] && !validate_{prefix}_value({enum_val}, buf)) {{")
    lines.append(f"      pos = row_end + 6; continue;")
    lines.append(f"    }}")

    if typ in C_NUMERIC_TYPES:
        lines.append(f"    {tbl_var}->rows[{idx}].{ci} = buf[0] ? (float)atof(buf) : -9999.0f;")
    elif typ in C_INT_TYPES:
        int_c = C_TYPE_MAP[typ]
        lines.append(f"    {tbl_var}->rows[{idx}].{ci} = buf[0] ? ({int_c})strtol(buf, NULL, 10) : 0;")
    elif typ == "boolean":
        lines.append(f'    {tbl_var}->rows[{idx}].{ci} = (strcmp(buf, "true") == 0);')
    else:  # string
        lines.append(f"    {tbl_var}->rows[{idx}].{ci} = buf[0] ? strdup(buf) : NULL;")
    lines.append("")


def gen_c_xml_parser(tables, subcategories=None, xml_map=None, always_overwrite=None,
                     settings_subcats=None, settings_fields=None,
                     dm_flat_config=None, dm_subcat_tables=None):
    """Generate xml_parser.c — SPIFFS XML parsing and struct population."""
    if subcategories is None:
        subcategories = {}
    if xml_map is None:
        xml_map = {}
    if always_overwrite is None:
        always_overwrite = set()
    if dm_flat_config is None:
        dm_flat_config = {}
    if dm_subcat_tables is None:
        dm_subcat_tables = set()
    lines = []
    lines.append("/*")
    lines.append(" * xml_parser.c — generated from XSD")
    lines.append(" * Parses XML from SPIFFS and populates row structs.")
    lines.append(" */")
    lines.append("")
    lines.append('extern "C" {')
    lines.append('#include "schema.h"')
    lines.append('#include "data_manager.h"')
    lines.append('}')
    lines.append("#include <SPIFFS.h>")
    lines.append("#include <Preferences.h>")
    lines.append("#include <string.h>")
    lines.append("#include <stdlib.h>")
    lines.append("#include <math.h>")
    if xml_map:
        lines.append('#include "xml_defaults.h"')
    lines.append("")

    # ── define table instances ──
    for t, fields in tables.items():
        tl = t.lower()
        subcats = subcategories.get(t)
        if subcats:
            for sc_name in subcats:
                sl = sc_name.lower()
                prefix = f"{tl}_{sl}"
                lines.append(f"{prefix}_table_t {prefix}_table = {{ .count = 0, .version = 0 }};")
        else:
            lines.append(f"{tl}_table_t {tl}_table = {{ .count = 0, .version = 0 }};")

    # ── Settings singleton global instances ──
    if settings_subcats and settings_fields:
        field_map_s = {n: typ for n, typ in settings_fields}
        other_subcats_s = {k: v for k, v in settings_subcats.items() if k != "header"}
        for sc_name, sc_val in other_subcats_s.items():
            if isinstance(sc_val, dict):  # group: emit one instance per inner subcat
                for inner_name, inner_fields in sc_val.items():
                    isl = inner_name.lower()
                    inits = []
                    for fn in inner_fields:
                        typ = field_map_s.get(fn, "string")
                        if typ == "string":
                            inits.append("NULL")
                        elif typ == "boolean":
                            inits.append("false")
                        else:
                            inits.append("0")
                    lines.append(f"settings_{isl}_t settings_{isl} = {{ {', '.join(inits)} }};")
            else:  # flat subcat: existing behaviour
                sl = sc_name.lower()
                inits = []
                for fn in sc_val:
                    typ = field_map_s.get(fn, "string")
                    if typ == "string":
                        inits.append("NULL")
                    elif typ == "boolean":
                        inits.append("false")
                    else:
                        inits.append("0")
                lines.append(f"settings_{sl}_t settings_{sl} = {{ {', '.join(inits)} }};")

    # ── increment_loop_pool: holds constraints-table indices where Increment != 0 ──
    lines.append("uint16_t increment_loop_pool[MAX_VARIABLES_CONSTRAINTS_ROWS];")
    lines.append("uint16_t increment_loop_count = 0;")

    lines.append("")

    # ── helper: extract tag value from a <row> fragment ──
    lines.append("/* ── extract value between <tag>…</tag> or detect <tag/> ── */")
    lines.append("static bool extract_tag(const char *xml, const char *tag, char *buf, int buflen) {")
    lines.append('  char open[64], self_close[64], close_tag[64];')
    lines.append('  snprintf(open, sizeof(open), "<%s>", tag);')
    lines.append('  snprintf(self_close, sizeof(self_close), "<%s/>", tag);')
    lines.append('  snprintf(close_tag, sizeof(close_tag), "</%s>", tag);')
    lines.append("")
    lines.append("  /* self-closing → empty value */")
    lines.append("  if (strstr(xml, self_close)) { buf[0] = '\\0'; return true; }")
    lines.append("")
    lines.append("  const char *s = strstr(xml, open);")
    lines.append("  if (!s) { buf[0] = '\\0'; return false; }")
    lines.append("  s += strlen(open);")
    lines.append("  const char *e = strstr(s, close_tag);")
    lines.append("  if (!e) { buf[0] = '\\0'; return false; }")
    lines.append("  int len = e - s;")
    lines.append("  if (len >= buflen) len = buflen - 1;")
    lines.append("  memcpy(buf, s, len);")
    lines.append("  buf[len] = '\\0';")
    lines.append("  return true;")
    lines.append("}")
    lines.append("")
    lines.append("")

    for t, fields in tables.items():
        tl = t.lower()
        tu = t.upper()
        subcats = subcategories.get(t)

        if subcats:
            field_map = {n: typ for n, typ in fields}
            subcat_names = list(subcats.keys())

            # Identify source subcat (has "Value") and optional subcats (have value_ptr)
            src_sc = next((s for s in subcat_names if "Value" in subcats[s]), None)
            optional_scs = [s for s in subcat_names if s.lower() in ("constraints", "modbus")]
            has_optional = bool(optional_scs) and src_sc is not None
            src_sl = src_sc.lower() if src_sc else subcat_names[0].lower()
            src_PREFIX = f"{tu}_{src_sc.upper()}" if src_sc else f"{tu}_{subcat_names[0].upper()}"

            # ── parse function that populates all subcat tables ──
            lines.append(f"/* ═══════ {t}: parse XML → {', '.join(tl + '_' + s.lower() for s in subcat_names)} tables ═══════ */")

            # Function signature: takes pointers to all subcat tables
            params = ", ".join(
                f"{tl}_{sc.lower()}_table_t *{sc.lower()}_tbl"
                for sc in subcat_names
            )
            lines.append(f"static int parse_{tl}_xml(const char *xml, {params}) {{")

            if has_optional:
                lines.append(f"  int {src_sl}_count = 0;")
                for osc in optional_scs:
                    lines.append(f"  int {osc.lower()}_count = 0;")
            else:
                lines.append("  int count = 0;")

            lines.append("  const char *pos = xml;")
            lines.append("  char buf[256];")
            lines.append("")

            if has_optional:
                lines.append(f"  while ({src_sl}_count < MAX_{src_PREFIX}_ROWS) {{")
            else:
                lines.append(f"  while (count < MAX_{src_PREFIX}_ROWS) {{")

            lines.append('    const char *row_start = strstr(pos, "<row>");')
            lines.append("    if (!row_start) break;")
            lines.append('    const char *row_end = strstr(row_start, "</row>");')
            lines.append("    if (!row_end) break;")
            lines.append("")

            if has_optional:
                # Source subcat fields — use src_sl_count as index
                sl = src_sc.lower()
                prefix = f"{tl}_{sl}"
                PREFIX = f"{tu}_{src_sc.upper()}"
                tbl_var = f"{sl}_tbl"
                lines.append(f"    /* ── {src_sc} ── */")
                for fn in subcats[src_sc]:
                    typ = field_map[fn]
                    _gen_field_extract(lines, fn, typ, prefix, PREFIX, tbl_var, idx=f"{src_sl}_count")

                # Optional subcats — temp vars then conditional commit
                for osc in optional_scs:
                    osl = osc.lower()
                    osu = osc.upper()
                    prefix_o = f"{tl}_{osl}"
                    PREFIX_O = f"{tu}_{osu}"
                    ofields = [(fn, field_map[fn]) for fn in subcats[osc]]
                    numeric_fields = [(fn, typ) for fn, typ in ofields if typ in C_NUMERIC_TYPES]
                    temp_vars = [(c_ident(fn), f"_t_{c_ident(fn)}") for fn, _ in numeric_fields]

                    lines.append(f"    /* ── {osc}: parse into temporaries, commit only if any field is non-default ── */")
                    if temp_vars:
                        lines.append(f"    float {', '.join(tv for _, tv in temp_vars)};")
                    lines.append("")

                    for fn, typ in ofields:
                        ci = c_ident(fn)
                        ci_upper = ci.upper()
                        enum_val = f"COL_{PREFIX_O}_{ci_upper}_{typ.upper()}"
                        temp = f"_t_{ci}"
                        lines.append(f'    extract_tag(row_start, "{ci}", buf, sizeof(buf));')
                        lines.append(f"    if (buf[0] && !validate_{prefix_o}_value({enum_val}, buf)) {{")
                        lines.append(f"      pos = row_end + 6; continue;")
                        lines.append(f"    }}")
                        if typ in C_NUMERIC_TYPES:
                            lines.append(f"    {temp} = buf[0] ? (float)atof(buf) : -9999.0f;")
                        lines.append("")

                    if temp_vars:
                        cond = " || ".join(f"{tv} != -9999.0f" for _, tv in temp_vars)
                        lines.append(f"    if ({cond}) {{")
                        lines.append(f"      /* at least one {osc} field is real — commit this row */")
                        for fn, typ in ofields:
                            if typ in C_NUMERIC_TYPES:
                                ci = c_ident(fn)
                                temp = f"_t_{ci}"
                                lines.append(f"      {osl}_tbl->rows[{osl}_count].{ci} = {temp};")
                        lines.append(f"      {osl}_tbl->rows[{osl}_count].value_ptr = &{src_sl}_tbl->rows[{src_sl}_count].Value;")
                        if "constraint" in osl:
                            lines.append(f"      {src_sl}_tbl->rows[{src_sl}_count].constraint_id = {osl}_count;")
                            # Save to increment_loop_pool if Increment is present and non-zero
                            inc_temp = next((tv for fn, tv in temp_vars if c_ident(fn) == "Increment"), None)
                            if inc_temp:
                                lines.append(f"      if ({inc_temp} != -9999.0f && {inc_temp} != 0.0f) {{")
                                lines.append(f"        increment_loop_pool[increment_loop_count++] = {osl}_count;")
                                lines.append(f"      }}")
                        lines.append(f"      {osl}_count++;")
                        lines.append("    } else {")
                        if "constraint" in osl:
                            lines.append(f"      {src_sl}_tbl->rows[{src_sl}_count].constraint_id = -1;")
                        lines.append("    }")
                        lines.append("")

                if t in dm_subcat_tables and src_sc:
                    sv = f"{src_sl}_tbl"
                    si = f"{src_sl}_count"
                    lines.append(f"    dm_set_value(&{sv}->rows[{si}],")
                    lines.append(f"                 &{sv}->rows[{si}].Value);")
                    lines.append("")
                    sc_fields_typed = [(fn, field_map[fn]) for fn in subcats[src_sc]]
                    printf_fmts = []
                    printf_args_list = []
                    for fn, ftyp in sc_fields_typed:
                        ci = c_ident(fn)
                        if ftyp in C_NUMERIC_TYPES:
                            printf_fmts.append(f"{fn}=%.4f")
                            printf_args_list.append(f"{sv}->rows[{si}].{ci}")
                        elif ftyp in C_INT_TYPES:
                            printf_fmts.append(f"{fn}=%d")
                            printf_args_list.append(f"(int){sv}->rows[{si}].{ci}")
                        else:
                            printf_fmts.append(f"{fn}=%s")
                            printf_args_list.append(f'{sv}->rows[{si}].{ci} ? {sv}->rows[{si}].{ci} : "(null)"')
                    printf_fmts.append("constraint_id=%d")
                    printf_args_list.append(f"{sv}->rows[{si}].constraint_id")
                    fmt_str = "  ".join(printf_fmts)
                    all_args = ", ".join([si] + printf_args_list)
                    lines.append(f'    Serial.printf("[%d] {fmt_str}\\n", {all_args});')
                    lines.append("")
                lines.append(f"    {src_sl}_count++;")
                lines.append("    pos = row_end + 6;")
                lines.append("  }")
                lines.append("")
                lines.append(f"  {src_sl}_tbl->count = {src_sl}_count;")
                for osc in optional_scs:
                    osl = osc.lower()
                    lines.append(f"  {osl}_tbl->count = {osl}_count;")
                if t in dm_subcat_tables and optional_scs:
                    ocount_fmt = "  ".join(f"{osc.lower()}: %d" for osc in optional_scs)
                    ocount_vars = ", ".join(f"{osc.lower()}_count" for osc in optional_scs)
                    lines.append(f'  Serial.printf("[{t}] total rows: %d  {ocount_fmt}\\n", {src_sl}_count, {ocount_vars});')
                lines.append(f"  return {src_sl}_count;")

            else:
                # Original single-counter behaviour
                for sc_name in subcat_names:
                    sl = sc_name.lower()
                    su = sc_name.upper()
                    prefix = f"{tl}_{sl}"
                    PREFIX = f"{tu}_{su}"
                    tbl_var = f"{sl}_tbl"
                    lines.append(f"    /* ── {sc_name} ── */")
                    for fn in subcats[sc_name]:
                        typ = field_map[fn]
                        _gen_field_extract(lines, fn, typ, prefix, PREFIX, tbl_var)

                if src_sc:
                    for sc in subcat_names:
                        if sc.lower() in ("modbus", "constraints"):
                            sl = sc.lower()
                            lines.append(f"    /* link {sl} value_ptr → {src_sl}.Value */")
                            lines.append(f"    {sl}_tbl->rows[count].value_ptr = &{src_sl}_tbl->rows[count].Value;")
                    lines.append("")

                lines.append("    count++;")
                lines.append("    pos = row_end + 6;")
                lines.append("  }")
                for sc_name in subcat_names:
                    sl = sc_name.lower()
                    lines.append(f"  {sl}_tbl->count = count;")
                lines.append("  return count;")

            lines.append("}")
            lines.append("")

            # ── free functions for each subcat that has strings ──
            for sc_name in subcat_names:
                sl = sc_name.lower()
                prefix = f"{tl}_{sl}"
                sc_fields = [(fn, field_map[fn]) for fn in subcats[sc_name]]
                string_fields = [c_ident(n) for n, typ in sc_fields if typ == "string"]
                if string_fields:
                    lines.append(f"/* ── free {sc_name} strings ── */")
                    lines.append(f"void free_{prefix}_table({prefix}_table_t *tbl) {{")
                    lines.append("  for (int i = 0; i < tbl->count; i++) {")
                    for sf in string_fields:
                        lines.append(f"    if (tbl->rows[i].{sf}) {{ free(tbl->rows[i].{sf}); tbl->rows[i].{sf} = NULL; }}")
                    lines.append("  }")
                    lines.append("  tbl->count = 0;")
                    lines.append("}")
                    lines.append("")

            # ── SPIFFS loader ──
            args = ", ".join(f"&{tl}_{sc.lower()}_table" for sc in subcat_names)
            lines.append(f"/* ── load {t} from SPIFFS into all subcat tables; falls back to embedded default if absent or empty ── */")
            lines.append(f"int load_{tl}_from_spiffs(void) {{")

            # Free strings in all subcats
            for sc_name in subcat_names:
                sl = sc_name.lower()
                prefix = f"{tl}_{sl}"
                sc_fields = [(fn, field_map[fn]) for fn in subcats[sc_name]]
                string_fields = [c_ident(n) for n, typ in sc_fields if typ == "string"]
                if string_fields:
                    lines.append(f"  free_{prefix}_table(&{prefix}_table);")

            if t in xml_map:
                lines.append(f"  String content;")
                lines.append(f'  File f = SPIFFS.open("/{t}.xml", "r");')
                lines.append(f"  if (f) {{ content = f.readString(); f.close(); }}")
                lines.append(f"  int n = (content.length() > 0)")
                lines.append(f"          ? parse_{tl}_xml(content.c_str(), {args})")
                lines.append(f"          : 0;")
                lines.append(f"  if (n == 0) {{")
                lines.append(f'    Serial.println("[XML] {t}.xml empty/missing -- using default");')
                lines.append(f"    n = parse_{tl}_xml({t.upper()}_XML_DEFAULT, {args});")
                lines.append(f"  }}")
            else:
                lines.append(f'  File f = SPIFFS.open("/{t}.xml", "r");')
                lines.append("  if (!f) return 0;")
                lines.append("  String content = f.readString();")
                lines.append("  f.close();")
                lines.append(f"  int n = parse_{tl}_xml(content.c_str(), {args});")

            for sc_name in subcat_names:
                sl = sc_name.lower()
                lines.append(f"  {tl}_{sl}_table.version++;")
            if t in dm_subcat_tables:
                lines.append("  sync_all();")
            lines.append("  return n;")
            lines.append("}")
            lines.append("")
            lines.append("")

        else:
            # ── No subcategories — original flat behaviour ──
            lines.append(f"/* ═══════ {t}: parse XML string → {tl}_table ═══════ */")
            lines.append(f"static int parse_{tl}_xml(const char *xml, {tl}_table_t *tbl) {{")
            lines.append("  int count = 0;")
            lines.append("  const char *pos = xml;")
            lines.append("  char buf[256];")
            lines.append("")
            if t in dm_flat_config:
                cfg = dm_flat_config[t]
                lines.append("  variables_description_row_t row;")
                lines.append(f'  row.Class         = (char *)"{cfg["class"]}";')
                lines.append("  row.constraint_id = -1;")
                lines.append("")
            lines.append(f"  while (count < MAX_{tu}_ROWS) {{")
            lines.append('    const char *row_start = strstr(pos, "<row>");')
            lines.append("    if (!row_start) break;")
            lines.append('    const char *row_end = strstr(row_start, "</row>");')
            lines.append("    if (!row_end) break;")
            lines.append("")

            for name, typ in fields:
                _gen_field_extract(lines, name, typ, tl, tu, "tbl")

            if t in dm_flat_config:
                cfg = dm_flat_config[t]
                nf = c_ident(cfg["name_field"])
                tf = c_ident(cfg["type_field"])
                vf = c_ident(cfg["value_field"])
                lines.append(f"    row.Name  = tbl->rows[count].{nf};")
                lines.append(f"    row.Type  = tbl->rows[count].{tf};")
                lines.append(f"    row.Value = tbl->rows[count].{vf};")
                lines.append(f"    dm_set_value(&row, &tbl->rows[count].{vf});")
                lines.append("")
                printf_fmts = []
                printf_args_list = []
                for fname, ftyp in fields:
                    ci = c_ident(fname)
                    if ftyp in C_NUMERIC_TYPES:
                        printf_fmts.append(f"{fname}=%.2f")
                        printf_args_list.append(f"tbl->rows[count].{ci}")
                    elif ftyp in C_INT_TYPES:
                        printf_fmts.append(f"{fname}=%d")
                        printf_args_list.append(f"(int)tbl->rows[count].{ci}")
                    else:
                        printf_fmts.append(f"{fname}=%s")
                        printf_args_list.append(f'tbl->rows[count].{ci} ? tbl->rows[count].{ci} : "(null)"')
                fmt_str = "  ".join(printf_fmts)
                all_args = ", ".join(["count"] + printf_args_list)
                lines.append(f'    Serial.printf("[%d] {fmt_str}\\n", {all_args});')
                lines.append("")
            lines.append("    count++;")
            lines.append("    pos = row_end + 6;")
            lines.append("  }")
            lines.append("  tbl->count = count;")
            if t in dm_flat_config:
                lines.append(f'  Serial.printf("[{t}] total rows: %d\\n", count);')
            lines.append("  return count;")
            lines.append("}")
            lines.append("")

            string_fields = [c_ident(n) for n, typ in fields if typ == "string"]
            if string_fields:
                lines.append(f"/* ── free dynamically allocated strings ── */")
                lines.append(f"void free_{tl}_table({tl}_table_t *tbl) {{")
                lines.append("  for (int i = 0; i < tbl->count; i++) {")
                for sf in string_fields:
                    lines.append(f"    if (tbl->rows[i].{sf}) {{ free(tbl->rows[i].{sf}); tbl->rows[i].{sf} = NULL; }}")
                lines.append("  }")
                lines.append("  tbl->count = 0;")
                lines.append("}")
                lines.append("")

            lines.append(f"/* ── load {t} from SPIFFS; falls back to embedded default if absent or empty ── */")
            lines.append(f"int load_{tl}_from_spiffs(void) {{")

            if string_fields:
                lines.append(f"  free_{tl}_table(&{tl}_table);")

            if t in xml_map:
                lines.append(f"  String _content;")
                lines.append(f'  File f = SPIFFS.open("/{t}.xml", "r");')
                lines.append(f"  if (f) {{ _content = f.readString(); f.close(); }}")
                lines.append(f"  int n = (_content.length() > 0)")
                lines.append(f"          ? parse_{tl}_xml(_content.c_str(), &{tl}_table)")
                lines.append(f"          : 0;")
                lines.append(f"  if (n == 0) {{")
                lines.append(f'    Serial.println("[XML] {t}.xml empty/missing -- using default");')
                lines.append(f"    n = parse_{tl}_xml({t.upper()}_XML_DEFAULT, &{tl}_table);")
                lines.append(f"  }}")
            else:
                lines.append(f'  File f = SPIFFS.open("/{t}.xml", "r");')
                lines.append(f"  const char *_src;")
                lines.append(f"  String _content;")
                lines.append(f"  if (f) {{")
                lines.append(f"    _content = f.readString();")
                lines.append(f"    f.close();")
                lines.append(f"    _src = _content.c_str();")
                lines.append(f"  }} else {{")
                lines.append(f"    _src = {t.upper()}_XML_DEFAULT;")
                lines.append(f"  }}")
                lines.append(f"  int n = parse_{tl}_xml(_src, &{tl}_table);")

            lines.append(f"  {tl}_table.version++;")
            if t in dm_flat_config:
                cls_name = dm_flat_config[t]["class"]
                lines.append(f'  uint16_t meta_idx = dm_class_map_find("{cls_name}");')
                lines.append(f"  if (meta_idx != INVALID_INDEX) sync_class(meta_idx);")
            lines.append("  return n;")
            lines.append("}")
            lines.append("")
            lines.append("")

    # ── Settings XML parser (one group/subcat per row) ──
    if settings_subcats and settings_fields:
        field_map_s = {n: typ for n, typ in settings_fields}
        other_subcats_s = {k: v for k, v in settings_subcats.items() if k != "header"}
        if other_subcats_s:
            lines.append("/* ── parse Settings XML — one category per row ── */")
            lines.append("static void parse_settings_xml(const char *xml) {")
            lines.append("  const char *pos = xml;")
            lines.append("  char buf[256];")
            lines.append("  while (1) {")
            lines.append('    const char *row_start = strstr(pos, "<row>");')
            lines.append("    if (!row_start) break;")
            lines.append('    const char *row_end = strstr(row_start, "</row>");')
            lines.append("    if (!row_end) break;")
            lines.append("")

            first = True
            for sc_name, sc_val in other_subcats_s.items():
                sl = sc_name.lower()
                cond = "if" if first else "else if"
                first = False
                if isinstance(sc_val, dict):  # group: nested if per inner subcat
                    lines.append(f"    /* ── {sc_name} (group) ── */")
                    lines.append(f'    {cond} (strstr(row_start, "<{sl}>")) {{')
                    for inner_name, inner_fields in sc_val.items():
                        isl = inner_name.lower()
                        lines.append(f"      /* ── {inner_name} ── */")
                        lines.append(f'      if (strstr(row_start, "<{isl}>")) {{')
                        for fn in inner_fields:
                            typ = field_map_s.get(fn, "string")
                            ci = c_ident(fn)
                            lines.append(f'        extract_tag(row_start, "{ci}", buf, sizeof(buf));')
                            if typ == "string":
                                lines.append(f"        if (settings_{isl}.{ci}) {{ free(settings_{isl}.{ci}); settings_{isl}.{ci} = NULL; }}")
                                lines.append(f"        if (buf[0]) settings_{isl}.{ci} = strdup(buf);")
                            elif typ == "boolean":
                                lines.append(f'        settings_{isl}.{ci} = (strcmp(buf, "true") == 0);')
                            elif typ in C_INT_TYPES:
                                int_c = C_TYPE_MAP[typ]
                                lines.append(f"        settings_{isl}.{ci} = buf[0] ? ({int_c})strtol(buf, NULL, 10) : 0;")
                            else:
                                lines.append(f"        settings_{isl}.{ci} = buf[0] ? (float)atof(buf) : 0.0f;")
                        lines.append("      }")
                        lines.append("")
                    lines.append("    }")
                else:  # flat subcat: existing behaviour
                    lines.append(f"    /* ── {sc_name} ── */")
                    lines.append(f'    {cond} (strstr(row_start, "<{sl}>")) {{')
                    for fn in sc_val:
                        typ = field_map_s.get(fn, "string")
                        ci = c_ident(fn)
                        lines.append(f'      extract_tag(row_start, "{ci}", buf, sizeof(buf));')
                        if typ == "string":
                            lines.append(f"      if (settings_{sl}.{ci}) {{ free(settings_{sl}.{ci}); settings_{sl}.{ci} = NULL; }}")
                            lines.append(f"      if (buf[0]) settings_{sl}.{ci} = strdup(buf);")
                        elif typ == "boolean":
                            lines.append(f'      settings_{sl}.{ci} = (strcmp(buf, "true") == 0);')
                        elif typ in C_INT_TYPES:
                            int_c = C_TYPE_MAP[typ]
                            lines.append(f"      settings_{sl}.{ci} = buf[0] ? ({int_c})strtol(buf, NULL, 10) : 0;")
                        else:
                            lines.append(f"      settings_{sl}.{ci} = buf[0] ? (float)atof(buf) : 0.0f;")
                    lines.append("    }")
                lines.append("")

            lines.append("    pos = row_end + 6;")
            lines.append("  }")
            lines.append("}")
            lines.append("")

            # Find which struct holds SSID so we can inject NVS WiFi credentials.
            _wifi_struct = None
            for _sc_name, _sc_val in other_subcats_s.items():
                if isinstance(_sc_val, dict):
                    for _in_name, _in_fields in _sc_val.items():
                        if "SSID" in _in_fields:
                            _wifi_struct = f"settings_{_in_name.lower()}"
                            break
                else:
                    if "SSID" in _sc_val:
                        _wifi_struct = f"settings_{_sc_name.lower()}"
                if _wifi_struct:
                    break

            lines.append("/* ── load Settings: seed defaults → NVS WiFi → SPIFFS override ── */")
            lines.append("int load_settings_from_spiffs(void) {")
            lines.append("  parse_settings_xml(SETTINGS_XML_DEFAULT);")
            if _wifi_struct:
                ws = _wifi_struct
                lines.append("  /* WiFi credentials from NVS (written by captive portal). */")
                lines.append("  {")
                lines.append('    Preferences _p;')
                lines.append('    _p.begin("wifi", true);')
                lines.append('    String _ssid = _p.getString("ssid", "");')
                lines.append('    String _pass = _p.getString("pass", "");')
                lines.append('    _p.end();')
                lines.append('    if (_ssid.length() > 0) {')
                lines.append(f"      if ({ws}.SSID) {{ free({ws}.SSID); {ws}.SSID = NULL; }}")
                lines.append(f"      {ws}.SSID = strdup(_ssid.c_str());")
                lines.append(f"      if ({ws}.Password) {{ free({ws}.Password); {ws}.Password = NULL; }}")
                lines.append(f"      if (_pass.length() > 0) {ws}.Password = strdup(_pass.c_str());")
                lines.append('    }')
                lines.append('  }')
            lines.append('  File f = SPIFFS.open("/Settings.xml", "r");')
            lines.append("  if (!f) return 0;")
            lines.append("  String content = f.readString();")
            lines.append("  f.close();")
            lines.append("  parse_settings_xml(content.c_str());")
            lines.append("  return 1;")
            lines.append("}")
            lines.append("")
            lines.append("")

    # ── provision_spiffs_xml: no-op — XMLs are only created by the web UI ──
    # Load functions fall back to embedded defaults when no SPIFFS file exists.
    # Captive portal credentials go to NVS, not SPIFFS.
    lines.append("/* provision_spiffs_xml — intentional no-op.")
    lines.append(" * XMLs are created in SPIFFS only by the web UI on first Submit.")
    lines.append(" * Load functions fall back to xml_defaults.h when no SPIFFS file exists.")
    lines.append(" * Captive portal WiFi credentials are stored in NVS, not SPIFFS. */")
    lines.append("void provision_spiffs_xml(void) {}")
    lines.append("")

    return "\n".join(lines)


# ═══════════════════════════════════════════════════════
#  XML DEFAULTS HEADER
# ═══════════════════════════════════════════════════════

def gen_xml_defaults_h(xml_map):
    """Generate xml_defaults.h — XML file contents embedded as C string literals.
    provision_spiffs_xml() writes these to SPIFFS on startup so that a normal
    Arduino 'Upload' also provisions the XML data without a separate data-upload step."""
    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("/* XML defaults embedded at build time by codegen.py.")
    lines.append(" * Call provision_spiffs_xml() once in setup() after SPIFFS.begin()")
    lines.append(" * to ensure these files are always present after flashing. */")
    lines.append("")

    for table_name, xml_content in xml_map.items():
        var_name = f"{table_name.upper()}_XML_DEFAULT"
        lines.append(f"static const char {var_name}[] = R\"XMLRAW(")
        lines.append(xml_content.strip())
        lines.append(")XMLRAW\";")
        lines.append("")

    return "\n".join(lines)


# ═══════════════════════════════════════════════════════
#  SETTINGS BAR GENERATOR
# ═══════════════════════════════════════════════════════

def gen_headers_block(headers_fields, field_map, defaults=None):
    """Generate the checkbox/input controls that live inside #submit-area (below Submit).

    Boolean fields become a checkbox+label row.  All other types get a compact input.
    The 'header' subcategory label is intentionally omitted.
    When a boolean field has default="true" in the XSD (passed via defaults dict), the
    checkbox is pre-checked in the HTML — important for fields like Download_a_Copy that
    are never populated by loadSettings().
    """
    html = ""
    for fn in headers_fields:
        typ = field_map.get(fn, "string")
        label_text = fn.replace("_", " ")
        if typ == "boolean":
            is_checked = defaults and defaults.get(fn) == "true"
            checked_attr = " checked" if is_checked else ""
            html += f'<label class="check-label">\n'
            html += f'  <input type="checkbox" data-name="{fn}" data-type="{typ}"{checked_attr}>\n'
            html += f'  {label_text}\n'
            html += f'</label>\n'
        else:
            html += f'<div class="settings-field">\n'
            html += (f'  <input placeholder="{typ}" data-type="{typ}"'
                     f' data-name="{fn}" oninput="validateField(this)">\n')
            html += f'  <span class="error-msg"></span>\n'
            html += f'</div>\n'
    return html


def gen_settings_block(name, subcats, fields):
    """Generate the Settings bar: heading + collapse button + collapsible subcategory rows.

    The 'header' subcategory is excluded here — it is rendered inside #submit-area
    via gen_headers_block() so the controls appear directly below the Submit button.

    Each other subcategory renders as one horizontal row using the same .field / .form-grid
    classes as Metadata and Variables.  A single Save button sits at the bottom of the
    collapsible (after all subcategory rows), matching the Insert-button pattern.
    Boolean fields become checkboxes; all other types get text inputs with validateField.
    """
    field_map = {n: typ for n, typ in fields}

    html  = f'<div class="settings-bar">\n'
    html += f'  <h3 class="category-header settings-category">\n'
    html += f'    {name}\n'
    html += f'    <button class="toggle-btn" '
    html += f'onclick="toggleSettingsSection(\'{name}\', this)">&#9660;</button>\n'
    html += f'  </h3>\n'

    # ── subcategories other than 'header': one row each, collapsible ──
    other_subcats = {k: v for k, v in subcats.items() if k != "header"}
    html += f'  <div id="{name}_collapsible" style="display:none">\n'
    for sc_name, sc_val in other_subcats.items():
        html += f'    <div class="subcat-row">\n'
        html += f'      <span class="subcat-label">{sc_name}</span>\n'
        if isinstance(sc_val, dict):  # group: render one column per inner subcat
            html += f'      <div class="settings-subcat-group">\n'
            for inner_sc_name, inner_fields in sc_val.items():
                html += f'        <div class="settings-subcat-col">\n'
                html += f'          <div class="subcat-label-inner">{inner_sc_name}</div>\n'
                html += f'          <div class="form-grid">\n'
                for fn in inner_fields:
                    typ = field_map[fn]
                    label = fn.replace("_", " ")
                    html += f'            <div class="field">\n'
                    html += f'              <label>{label}</label>\n'
                    if typ == "boolean":
                        html += (f'              <input type="checkbox" data-type="{typ}"'
                                 f' data-name="{fn}" class="settings-checkbox">\n')
                    else:
                        html += (f'              <input placeholder="{typ}" data-type="{typ}"'
                                 f' data-name="{fn}" oninput="validateField(this)">\n')
                    html += f'              <span class="error-msg"></span>\n'
                    html += f'            </div>\n'
                html += f'          </div>\n'
                html += f'        </div>\n'
            html += f'      </div>\n'
        else:  # flat subcat: original single form-grid
            html += f'      <div class="form-grid">\n'
            for fn in sc_val:
                typ = field_map[fn]
                label = fn.replace("_", " ")
                html += f'        <div class="field">\n'
                html += f'          <label>{label}</label>\n'
                if typ == "boolean":
                    html += (f'          <input type="checkbox" data-type="{typ}"'
                             f' data-name="{fn}" class="settings-checkbox">\n')
                else:
                    html += (f'          <input placeholder="{typ}" data-type="{typ}"'
                             f' data-name="{fn}" oninput="validateField(this)">\n')
                html += f'          <span class="error-msg"></span>\n'
                html += f'        </div>\n'
            html += f'      </div>\n'
        html += f'    </div>\n'
    # Single Save button at the bottom of all subcategory rows
    html += f'    <button onclick="saveSettings()" class="insert-btn">Save</button>\n'

    # Settings table — 2 fixed rows showing current values
    table_cols = []
    for sc_val in other_subcats.values():
        if isinstance(sc_val, dict):
            for inner_fields in sc_val.values():
                table_cols.extend(inner_fields)
        else:
            table_cols.extend(sc_val)

    html += f'    <div class="table-wrap">\n'
    html += f'      <table>\n'
    html += f'      <thead><tr>\n'
    html += f'        <th>Category</th>\n'
    for col in table_cols:
        html += f'        <th>{col}</th>\n'
    html += f'      </tr></thead>\n'
    html += f'      <tbody id="Settings_body"></tbody>\n'
    html += f'      </table>\n'
    html += f'    </div>\n'

    html += f'  </div>\n'

    html += f'</div>\n'
    return html


# ═══════════════════════════════════════════════════════
#  STABLE JS GENERATOR
# ═══════════════════════════════════════════════════════

def gen_stable_js():
    """Generate all stable JS logic (previously hardcoded in template.html).

    Dynamic parts:
      - submitAll() reads Download_a_Copy from the Settings_form input.
      - toggleSettingsSection() collapses all Settings subcats except header.
    """
    return """\
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

  state["Settings"] = {};
  loadSettings();

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

  let xml = `<${table}>\\n`;

  state[table].forEach(r => {
    let line = "<row>";
    schema.forEach((f, i) => {
      const tag = xmlTag(f.name);
      const v = r[i];
      line += v === null ? `<${tag}/>` : `<${tag}>${v}</${tag}>`;
    });
    xml += line + "</row>\\n";
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
/* Collapses all Settings subcategories except header (always visible). */
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
  if (Object.keys(state["Settings"] || {}).length > 0) {
    files.push({ name: "Settings.xml", data: settingsToXML() });
  }
  const zip  = createZIP(files);
  const blob = new Blob([zip], { type: "application/zip" });
  const a    = document.createElement("a");
  a.href     = URL.createObjectURL(blob);
  a.download = "acms_xml.zip";
  document.body.appendChild(a);
  a.click();
  a.remove();
}


/* ---------- UPDATE SETTINGS TABLE ---------- */
function updateSettingsTable() {
  var tbody = document.getElementById("Settings_body");
  if (!tbody) return;
  tbody.innerHTML = "";
  Object.keys(Settings_SUBCATS).forEach(function(cat) {
    var cat_val = Settings_SUBCATS[cat];
    var tr = document.createElement("tr");
    var catTd = document.createElement("td");
    catTd.textContent = cat;
    tr.appendChild(catTd);
    Settings_TABLE_COLS.forEach(function(col) {
      var td = document.createElement("td");
      var belongs = false;
      if (Array.isArray(cat_val)) {
        belongs = cat_val.indexOf(col) !== -1;
      } else {
        Object.keys(cat_val).forEach(function(inner) {
          if (cat_val[inner].indexOf(col) !== -1) belongs = true;
        });
      }
      td.textContent = belongs ? (state["Settings"][col] !== undefined ? state["Settings"][col] : "") : "\u2014";
      tr.appendChild(td);
    });
    tbody.appendChild(tr);
  });
}


/* ---------- SAVE SETTINGS (stores to state, POSTs to /Settings.xml on ESP) ---------- */
async function saveSettings() {
  state["Settings"] = {};
  document.querySelectorAll(".settings-bar input[data-name]").forEach(function(inp) {
    if (inp.dataset.name === "Download_a_Copy") return;
    var val = inp.type === "checkbox" ? String(inp.checked) : inp.value.trim();
    state["Settings"][inp.dataset.name] = val;
  });
  console.group("[ACMS] Settings state");
  console.table(state["Settings"]);
  console.groupEnd();
  updateSettingsTable();
  if (location.hostname !== "") {
    try {
      const r = await fetch("/Settings.xml", {
        method: "POST",
        headers: { "Content-Type": "application/xml" },
        body: settingsToXML()
      });
      if (!r.ok) throw new Error("Save failed (" + r.status + ")");
    } catch(e) {
      alert("Error saving settings: " + e.message);
      return;
    }
  }
  alert("Updates have been saved successfully");
}


/* ---------- SETTINGS TO XML ---------- */
/* Produces one <row> per category. Groups emit nested inner-subcat elements. */
function settingsToXML() {
  var xml = "<Settings>\\n";
  Object.keys(Settings_SUBCATS).forEach(function(sc) {
    var sc_val = Settings_SUBCATS[sc];
    xml += "  <row><" + sc + ">";
    if (Array.isArray(sc_val)) {
      sc_val.forEach(function(field) {
        var val = (state["Settings"][field] !== undefined) ? state["Settings"][field] : "";
        xml += "<" + field + ">" + val + "</" + field + ">";
      });
    } else {
      Object.keys(sc_val).forEach(function(inner_sc) {
        xml += "<" + inner_sc + ">";
        sc_val[inner_sc].forEach(function(field) {
          var val = (state["Settings"][field] !== undefined) ? state["Settings"][field] : "";
          xml += "<" + field + ">" + val + "</" + field + ">";
        });
        xml += "</" + inner_sc + ">";
      });
    }
    xml += "</" + sc + "></row>\\n";
  });
  xml += "</Settings>";
  return xml;
}


/* ---------- LOAD SETTINGS (ESP only — populate inputs + state from /Settings.xml) ---------- */
/* Uses simple indexOf-based tag extraction (mirrors C++ extract_tag) instead of
 * DOMParser so it never silently fails on slightly malformed XML. */
function loadSettings() {
  if (location.hostname === "") return;
  fetch("/Settings.xml", { cache: "no-store" })
    .then(function(r) { if (r.ok) return r.text(); })
    .then(function(xml) {
      if (!xml) return;
      state["Settings"] = {};

      /* Extract the text content between <Tag>…</Tag>. Returns null if absent. */
      function extractTag(str, tag) {
        var open  = "<" + tag + ">";
        var close = "</" + tag + ">";
        var s = str.indexOf(open);
        if (s === -1) return null;
        s += open.length;
        var e = str.indexOf(close, s);
        if (e === -1) return null;
        return str.substring(s, e).trim();
      }

      Settings_TABLE_COLS.forEach(function(field) {
        var val = extractTag(xml, field);
        if (val === null) return;
        state["Settings"][field] = val;
        var inp = document.querySelector(".settings-bar input[data-name='" + field + "']");
        if (!inp) return;
        if (inp.type === "checkbox") inp.checked = (val === "true");
        else inp.value = val;
      });
    })
    .then(function() { updateSettingsTable(); })
    .catch(function(e) { console.error("[ACMS] loadSettings failed:", e); });
}


/* ---------- SUBMIT ---------- */
async function submitAll() {

  const isESP = location.hostname !== "";

  /* Read Download_a_Copy checkbox from submit-area */
  var dlInput = document.querySelector('#submit-area [data-name="Download_a_Copy"]');
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

      if (Object.keys(state["Settings"] || {}).length === 0) {
        document.querySelectorAll(".settings-bar input[data-name]").forEach(function(inp) {
          if (inp.dataset.name === "Download_a_Copy") return;
          state["Settings"] = state["Settings"] || {};
          var val = inp.type === "checkbox" ? String(inp.checked) : inp.value.trim();
          state["Settings"][inp.dataset.name] = val;
        });
      }
      {
        const r = await fetch("/Settings.xml", {
          method: "POST",
          headers: { "Content-Type": "application/xml" },
          body: settingsToXML()
        });
        if (!r.ok) throw new Error("Settings save failed (" + r.status + ")");
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
      const toDownload = TABLE_LIST.filter(t => state[t].length > 0).map(t => ({
        name: t + ".xml", data: rowsToXML(t, window[t + "_SCHEMA"])
      }));
      if (Object.keys(state["Settings"] || {}).length > 0) {
        toDownload.push({ name: "Settings.xml", data: settingsToXML() });
      }
      toDownload.forEach(({ name, data }, idx) => {
        setTimeout(() => {
          const blob = new Blob([data], { type: "application/xml" });
          const a = document.createElement("a");
          a.href = URL.createObjectURL(blob);
          a.download = name;
          document.body.appendChild(a);
          a.click();
          a.remove();
        }, idx * 200);
      });
    }
  }
}
"""


# ═══════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════

def import_zip(zip_path):
    """Extract XMLs from zip_path into codegen working dir, then delete the zip."""
    import zipfile
    work_dir = os.path.dirname(os.path.abspath(__file__))
    zip_path  = os.path.abspath(zip_path)

    if not os.path.isfile(zip_path):
        print(f"ERROR: ZIP not found: {zip_path}")
        sys.exit(1)

    with zipfile.ZipFile(zip_path, "r") as zf:
        extracted = []
        for member in zf.namelist():
            # only extract .xml files, strip any directory prefix
            if member.lower().endswith(".xml"):
                basename = os.path.basename(member)
                dest = os.path.join(work_dir, basename)
                with zf.open(member) as src, open(dest, "wb") as dst:
                    dst.write(src.read())
                extracted.append(basename)

    os.remove(zip_path)

    if extracted:
        print(f"Imported from ZIP: {', '.join(extracted)}")
        print(f"Deleted: {zip_path}")
    else:
        print("WARNING: ZIP contained no .xml files.")


def read_var_pool_size(xml_path, default):
    """Parse Var_Pool_Size integer from a Settings.xml; returns default if absent or invalid."""
    try:
        tree = ET.parse(xml_path)
        for el in tree.iter("Var_Pool_Size"):
            v = (el.text or "").strip()
            if v.isdigit():
                return int(v)
    except Exception:
        pass
    return default


def gen_default_settings_xml(settings_subcats, settings_fields, max_rows, max_class=32):
    """Generate a default Settings.xml using max_rows / max_class as schema defaults."""
    field_map = {n: typ for n, typ in settings_fields}
    other = {k: v for k, v in settings_subcats.items() if k != "header"}

    def _field_val(fn):
        typ = field_map.get(fn, "string")
        if fn == "Var_Pool_Size":
            return str(max_rows)
        elif fn == "Class_Pool_Size":
            return str(max_class)
        elif typ == "boolean":
            return "true"
        elif typ in C_INT_TYPES or typ in C_NUMERIC_TYPES:
            return "0"
        return ""

    xml = "<Settings>\n"
    for sc_name, sc_val in other.items():
        xml += f"<row><{sc_name}>"
        if isinstance(sc_val, dict):  # group: wrap each inner subcat
            for inner_name, inner_fields in sc_val.items():
                xml += f"<{inner_name}>"
                for fn in inner_fields:
                    xml += f"<{fn}>{_field_val(fn)}</{fn}>"
                xml += f"</{inner_name}>"
        else:  # flat subcat
            for fn in sc_val:
                xml += f"<{fn}>{_field_val(fn)}</{fn}>"
        xml += f"</{sc_name}></row>\n"
    xml += "</Settings>"
    return xml


def main():
    if len(sys.argv) not in (3, 4):
        print("Usage: python codegen.py <schema.xsd> <template.html> [acms_xml.zip]")
        sys.exit(1)

    xsd_path = sys.argv[1]
    template_path = sys.argv[2]

    # Optional third arg: path to a ZIP previously downloaded from the browser.
    # XMLs inside are extracted to the codegen working dir, replacing older files,
    # and the ZIP is deleted before code generation runs.
    if len(sys.argv) == 4:
        import_zip(sys.argv[3])
    tables, subcategories = parse_xsd(xsd_path)
    xsd_defaults = parse_xsd_defaults(xsd_path)

    # Settings: client-only, not persisted to SPIFFS, excluded from C gen.
    settings_fields = tables.get("Settings", [])
    settings_subcats = subcategories.get("Settings", {})

    # Determine MAX_ROWS: read Var_Pool_Size from Settings.xml if present, else use default.
    max_rows = MAX_ROWS
    if os.path.isfile("Settings.xml"):
        parsed = read_var_pool_size("Settings.xml", MAX_ROWS)
        if parsed != MAX_ROWS:
            print(f"  Var_Pool_Size={parsed} (from Settings.xml) overrides MAX_ROWS={MAX_ROWS}")
        max_rows = parsed

    # Form tables: exclude Settings and Controls (neither maps to SPIFFS XML tables).
    form_tables = {k: v for k, v in tables.items() if k not in ("Settings", "Controls")}

    # Display order: Metadata first, then Variables.
    if "Metadata" in form_tables and "Variables" in form_tables:
        form_tables = {"Metadata": form_tables["Metadata"], "Variables": form_tables["Variables"]}

    # Validity field names come from Controls > validation + verification subcategories.
    controls_subcats = subcategories.get("Controls", {})
    validity_field_names = set(
        controls_subcats.get("validation", []) + controls_subcats.get("verification", [])
    )

    with open(template_path, "r") as f:
        template = f.read()

    # ── JS outputs ──
    # Include Settings fields so boolean validation rule is generated.
    all_js_tables = dict(form_tables)
    if settings_fields:
        all_js_tables["Settings"] = settings_fields
    validator_js = gen_validator_js(all_js_tables)
    validity_js = gen_validity_js() if validity_field_names else ""

    # validity.js contains both validator + validity checker
    combined_js = validator_js + "\n\n" + validity_js if validity_js else validator_js

    with open("validity.js", "w") as f:
        f.write(combined_js)

    # ── Settings bar + headers block + stable JS ──
    settings_block = gen_settings_block("Settings", settings_subcats, settings_fields) \
        if settings_fields else ""
    headers_fields = settings_subcats.get("header", [])
    headers_field_map = {n: typ for n, typ in settings_fields}
    headers_block = gen_headers_block(headers_fields, headers_field_map, xsd_defaults) \
        if headers_fields else ""
    stable_js = gen_stable_js()

    # HTML: form_tables only, validity fields get special oninput handler
    base = template.replace("%SETTINGS_BLOCK%", settings_block)
    base = base.replace("%HEADERS_BLOCK%", headers_block)
    base = base.replace("%STABLE_JS%", stable_js)
    base = base.replace("%TABLE_BLOCKS%", gen_table_blocks(
        form_tables, validity_field_names, subcategories,
        hidden_by_default={"Metadata"}))
    base = base.replace("%SCHEMA_JS%", gen_schema_js(form_tables, settings_subcats))
    base = base.replace("%VALIDATOR_JS%", combined_js)

    # PC version (embed existing XML for form tables only)
    # Also collect xml_map for SPIFFS provisioning header
    preload_entries = []
    xml_map = {}
    for t in form_tables:
        xml_path = t + ".xml"
        if os.path.isfile(xml_path):
            with open(xml_path, "r") as f:
                xml_content = f.read()
            preload_entries.append(f"  {json.dumps(t)}: {json.dumps(xml_content)}")
            xml_map[t] = xml_content
            print(f"  Embedding {xml_path}")

    # Add Settings to xml_map for SPIFFS provisioning (once-only, user-managed).
    if settings_subcats and settings_fields:
        if os.path.isfile("Settings.xml"):
            with open("Settings.xml", "r") as f:
                settings_xml_content = f.read()
        else:
            settings_xml_content = gen_default_settings_xml(
                settings_subcats, settings_fields, max_rows)
            with open("Settings.xml", "w") as f:
                f.write(settings_xml_content)
        xml_map["Settings"] = settings_xml_content
        print(f"  Embedding Settings.xml")

    pc_page = base.replace("%PRELOAD_XML%", ",\n".join(preload_entries))
    with open("generated_page.html", "w") as f:
        f.write(pc_page)

    # ESP version (empty PRELOAD_XML)
    esp_page = base.replace("%PRELOAD_XML%", "")
    with open("web_page.h", "w") as f:
        f.write("#pragma once\n\n")
        f.write('const char WEB_PAGE[] PROGMEM = R"HTML(\n')
        f.write(esp_page)
        f.write('\n)HTML";\n')

    # ── C outputs ──
    with open("schema.h", "w") as f:
        f.write(gen_c_schema_h(form_tables, subcategories,
                               settings_subcats=settings_subcats,
                               settings_fields=settings_fields,
                               max_rows=max_rows))

    dm_flat_config = {
        "Metadata": {
            "class":       "metaData",
            "name_field":  "Class",
            "type_field":  "Message",
            "value_field": "Key",
        }
    }
    dm_subcat_tables = {"Variables"}

    with open("xml_parser.cpp", "w") as f:
        f.write(gen_c_xml_parser(form_tables, subcategories, xml_map,
                                 always_overwrite=set(),
                                 settings_subcats=settings_subcats,
                                 settings_fields=settings_fields,
                                 dm_flat_config=dm_flat_config,
                                 dm_subcat_tables=dm_subcat_tables))

    if xml_map:
        with open("xml_defaults.h", "w") as f:
            f.write(gen_xml_defaults_h(xml_map))

    print("Generated:")
    print("  validity.js           (JS field validator + Validity checker)")
    print("  generated_page.html   (PC — XML embedded)")
    print("  web_page.h            (ESP — fetches from SPIFFS)")
    print("  schema.h              (C enums, structs, validation)")
    print("  xml_parser.cpp        (C++ SPIFFS XML parser)")
    if xml_map:
        print("  xml_defaults.h        (XML defaults for SPIFFS provisioning)")

    # ── Copy ESP files to sketch directory so Arduino IDE can flash directly ──
    sketch_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    esp_files = ["web_page.h", "schema.h", "xml_parser.cpp"]
    if xml_map:
        esp_files.append("xml_defaults.h")

    print("\nCopied to sketch directory:")
    for fname in esp_files:
        dst = os.path.join(sketch_dir, fname)
        shutil.copy(fname, dst)
        print(f"  {fname} → {os.path.normpath(dst)}")

if __name__ == "__main__":
    main()
