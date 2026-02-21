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
                # Subcategory wrapper — extract inner fields
                subcat_name = child.attrib["name"]
                subcat_field_names = []
                inner_ct = child.find(XSD_NS + "complexType")
                if inner_ct is not None:
                    inner_seq = inner_ct.find(XSD_NS + "sequence")
                    if inner_seq is not None:
                        for sf in inner_seq.findall(XSD_NS + "element"):
                            if "type" in sf.attrib:
                                name = sf.attrib["name"]
                                typ = sf.attrib["type"].split(":")[1]
                                fields.append((name, typ))
                                subcat_field_names.append(name)
                if subcat_field_names:
                    subcats[subcat_name] = subcat_field_names

        tables[table] = fields
        if subcats:
            subcategories[table] = subcats

    return tables, subcategories


# ═══════════════════════════════════════════════════════
#  JS GENERATORS (unchanged)
# ═══════════════════════════════════════════════════════

def gen_schema_js(tables):
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
                    f"var ni=document.getElementById('{t}_form').querySelector('[data-field=\"Name\"]'); "
                    "if(ni) validateValidityField(ni)"
                )
                extra = ""
            elif n == "Type" and ("Value" in validity_field_names or "Name" in validity_field_names):
                triggers = ["validateField(this)"]
                if "Value" in validity_field_names:
                    triggers.append(
                        f"var vi=document.getElementById('{t}_form').querySelector('[data-field=\"Value\"]'); "
                        "if(vi) validateValidityField(vi)"
                    )
                if "Name" in validity_field_names:
                    triggers.append(
                        f"var ni=document.getElementById('{t}_form').querySelector('[data-field=\"Name\"]'); "
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
        '    if (typeVal === "type" || typeVal === "units" || typeVal === "verification") {\n'
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
        "    /* ── Step 6: resolve unit message from Metadata (use classKey class only) ── */\n"
        '    var uKeyVal = (nameDict[unitRowKey]["Value"] !== null && nameDict[unitRowKey]["Value"] !== undefined)\n'
        '      ? String(nameDict[unitRowKey]["Value"]) : "";\n'
        "    /* normalize float key: \"1.0\" → \"1\" so it matches Metadata Key column */\n"
        "    var uKeyNorm = (uKeyVal !== \"\" && isFinite(uKeyVal)) ? String(parseFloat(uKeyVal)) : uKeyVal;\n"
        "    var uClassKey = Object.keys(dict).find(function(k) {\n"
        "      return k.toLowerCase() === classKey.toLowerCase();\n"
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


def _gen_subcat_block(lines, t, subcat_name, subcat_fields, field_map, has_value_ptr=False):
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
    lines.append(f"}} {prefix}_row_t;")
    lines.append("")

    # ── max rows ──
    lines.append(f"#define MAX_{PREFIX}_ROWS {MAX_ROWS}")
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


def gen_c_schema_h(tables, subcategories=None):
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
                lines.append(f"/* ─── {t} / {sc_name} ─── */")
                lines.append("")
                _gen_subcat_block(lines, t, sc_name, sc_fields, field_map, has_value_ptr)
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

            lines.append(f"#define MAX_{tu}_ROWS {MAX_ROWS}")
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

    return "\n".join(lines)


def _gen_field_extract(lines, name, typ, prefix, PREFIX, tbl_var):
    """Generate extract + assign for one field inside a parse loop."""
    ci = c_ident(name)
    ci_upper = ci.upper()
    enum_val = f"COL_{PREFIX}_{ci_upper}_{typ.upper()}"

    lines.append(f'    extract_tag(row_start, "{ci}", buf, sizeof(buf));')
    lines.append(f"    if (buf[0] && !validate_{prefix}_value({enum_val}, buf)) {{")
    lines.append(f"      pos = row_end + 6; continue;")
    lines.append(f"    }}")

    if typ in C_NUMERIC_TYPES:
        lines.append(f"    {tbl_var}->rows[count].{ci} = buf[0] ? (float)atof(buf) : -9999.0f;")
    elif typ in C_INT_TYPES:
        int_c = C_TYPE_MAP[typ]
        lines.append(f"    {tbl_var}->rows[count].{ci} = buf[0] ? ({int_c})strtol(buf, NULL, 10) : 0;")
    elif typ == "boolean":
        lines.append(f'    {tbl_var}->rows[count].{ci} = (strcmp(buf, "true") == 0);')
    else:  # string
        lines.append(f"    {tbl_var}->rows[count].{ci} = buf[0] ? strdup(buf) : NULL;")
    lines.append("")


def gen_c_xml_parser(tables, subcategories=None, xml_map=None, always_overwrite=None):
    """Generate xml_parser.c — SPIFFS XML parsing and struct population."""
    if subcategories is None:
        subcategories = {}
    if xml_map is None:
        xml_map = {}
    if always_overwrite is None:
        always_overwrite = set()
    lines = []
    lines.append("/*")
    lines.append(" * xml_parser.c — generated from XSD")
    lines.append(" * Parses XML from SPIFFS and populates row structs.")
    lines.append(" */")
    lines.append("")
    lines.append('#include "schema.h"')
    lines.append("#include <SPIFFS.h>")
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

            # Use the first subcat's MAX for the shared row limit
            first_prefix = f"{tu}_{subcat_names[0].upper()}"

            # ── single parse function that populates all subcat tables ──
            lines.append(f"/* ═══════ {t}: parse XML → {', '.join(tl + '_' + s.lower() for s in subcat_names)} tables ═══════ */")

            # Function signature: takes pointers to all subcat tables
            params = ", ".join(
                f"{tl}_{sc.lower()}_table_t *{sc.lower()}_tbl"
                for sc in subcat_names
            )
            lines.append(f"static int parse_{tl}_xml(const char *xml, {params}) {{")
            lines.append("  int count = 0;")
            lines.append("  const char *pos = xml;")
            lines.append("  char buf[256];")
            lines.append("")
            lines.append(f"  while (count < MAX_{first_prefix}_ROWS) {{")
            lines.append('    const char *row_start = strstr(pos, "<row>");')
            lines.append("    if (!row_start) break;")
            lines.append('    const char *row_end = strstr(row_start, "</row>");')
            lines.append("    if (!row_end) break;")
            lines.append("")

            # Extract fields for each subcategory
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

            # Link modbus value_ptr to values struct's Value field
            # Find the source subcat that owns the "Value" field (e.g. description)
            src_sc = next((s for s in subcat_names if "Value" in subcats[s]), None)
            if src_sc:
                src_sl = src_sc.lower()
                for sc in subcat_names:
                    if sc.lower() in ("modbus", "constraints"):
                        sl = sc.lower()
                        lines.append(f"    /* link {sl} value_ptr → {src_sl}.Value */")
                        lines.append(f"    {sl}_tbl->rows[count].value_ptr = &{src_sl}_tbl->rows[count].Value;")
                lines.append("")

            lines.append("    count++;")
            lines.append("    pos = row_end + 6;")
            lines.append("  }")

            # Set count on all subcat tables
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
            lines.append(f"/* ── load {t} from SPIFFS into all subcat tables ── */")
            lines.append(f"int load_{tl}_from_spiffs(void) {{")

            # Free strings in all subcats
            for sc_name in subcat_names:
                sl = sc_name.lower()
                prefix = f"{tl}_{sl}"
                sc_fields = [(fn, field_map[fn]) for fn in subcats[sc_name]]
                string_fields = [c_ident(n) for n, typ in sc_fields if typ == "string"]
                if string_fields:
                    lines.append(f"  free_{prefix}_table(&{prefix}_table);")

            lines.append(f'  File f = SPIFFS.open("/{t}.xml", "r");')
            lines.append("  if (!f) return 0;")
            lines.append("  String content = f.readString();")
            lines.append("  f.close();")

            args = ", ".join(f"&{tl}_{sc.lower()}_table" for sc in subcat_names)
            lines.append(f"  int n = parse_{tl}_xml(content.c_str(), {args});")
            for sc_name in subcat_names:
                sl = sc_name.lower()
                lines.append(f"  {tl}_{sl}_table.version++;")
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
            lines.append(f"  while (count < MAX_{tu}_ROWS) {{")
            lines.append('    const char *row_start = strstr(pos, "<row>");')
            lines.append("    if (!row_start) break;")
            lines.append('    const char *row_end = strstr(row_start, "</row>");')
            lines.append("    if (!row_end) break;")
            lines.append("")

            for name, typ in fields:
                _gen_field_extract(lines, name, typ, tl, tu, "tbl")

            lines.append("    count++;")
            lines.append("    pos = row_end + 6;")
            lines.append("  }")
            lines.append("  tbl->count = count;")
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

            lines.append(f"/* ── load {t} from SPIFFS into {tl}_table ── */")
            lines.append(f"int load_{tl}_from_spiffs(void) {{")

            if string_fields:
                lines.append(f"  free_{tl}_table(&{tl}_table);")

            lines.append(f'  File f = SPIFFS.open("/{t}.xml", "r");')
            lines.append("  if (!f) return 0;")
            lines.append("  String content = f.readString();")
            lines.append("  f.close();")
            lines.append(f"  int n = parse_{tl}_xml(content.c_str(), &{tl}_table);")
            lines.append(f"  {tl}_table.version++;")
            lines.append("  return n;")
            lines.append("}")
            lines.append("")
            lines.append("")

    # ── provision_spiffs_xml: write embedded defaults to SPIFFS on startup ──
    if xml_map:
        always_tables = [t for t in xml_map if t in always_overwrite]
        once_tables   = [t for t in xml_map if t not in always_overwrite]

        lines.append("/* ═══════ SPIFFS provisioning — writes embedded XML defaults ═══════ */")
        lines.append("/* Call once in setup() after SPIFFS.begin() / acms_web_init(). */")
        lines.append("void provision_spiffs_xml(void) {")

        if always_tables:
            n = len(always_tables)
            lines.append(f"  /* always overwrite — developer-managed tables */")
            lines.append(f"  const struct {{ const char *path; const char *data; }} always[{n}] = {{")
            for t in always_tables:
                lines.append(f'    {{ "/{t}.xml", {t.upper()}_XML_DEFAULT }},')
            lines.append("  };")
            lines.append(f"  for (int i = 0; i < {n}; i++) {{")
            lines.append("    File f = SPIFFS.open(always[i].path, \"w\");")
            lines.append("    if (f) { f.print(always[i].data); f.close(); }")
            lines.append("    yield();")
            lines.append("  }")

        if once_tables:
            n = len(once_tables)
            lines.append(f"  /* only write if absent — user-managed tables */")
            lines.append(f"  const struct {{ const char *path; const char *data; }} once[{n}] = {{")
            for t in once_tables:
                lines.append(f'    {{ "/{t}.xml", {t.upper()}_XML_DEFAULT }},')
            lines.append("  };")
            lines.append(f"  for (int i = 0; i < {n}; i++) {{")
            lines.append("    if (!SPIFFS.exists(once[i].path)) {")
            lines.append("      File f = SPIFFS.open(once[i].path, \"w\");")
            lines.append("      if (f) { f.print(once[i].data); f.close(); }")
            lines.append("    }")
            lines.append("    yield();")
            lines.append("  }")

        lines.append("}")
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

def gen_headers_block(headers_fields, field_map):
    """Generate the checkbox/input controls that live inside #submit-area (below Submit).

    Boolean fields become a checkbox+label row.  All other types get a compact input.
    The 'headers' subcategory label is intentionally omitted.
    """
    html = ""
    for fn in headers_fields:
        typ = field_map.get(fn, "string")
        label_text = fn.replace("_", " ")
        if typ == "boolean":
            html += f'<label class="check-label">\n'
            html += f'  <input type="checkbox" data-name="{fn}" data-type="{typ}">\n'
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
    """Generate the Settings bar: heading + collapse button + collapsible for future subcats.

    The 'headers' subcategory is excluded here — it is rendered inside #submit-area
    via gen_headers_block() so the controls appear directly below the Submit button.
    """
    field_map = {n: typ for n, typ in fields}

    html  = f'<div class="settings-bar">\n'
    html += f'  <h3 class="category-header settings-category">\n'
    html += f'    {name}\n'
    html += f'    <button class="toggle-btn" '
    html += f'onclick="toggleSettingsSection(\'{name}\', this)">&#9660;</button>\n'
    html += f'  </h3>\n'

    # ── subcategories other than 'headers': collapsible (hidden by default) ──
    other_subcats = {k: v for k, v in subcats.items() if k != "headers"}
    html += f'  <div id="{name}_collapsible" style="display:none">\n'
    for sc_name, sc_fields in other_subcats.items():
        html += f'    <div class="subcat-row settings-compact">\n'
        html += f'      <span class="subcat-label">{sc_name}</span>\n'
        html += f'      <div class="settings-form-grid">\n'
        for fn in sc_fields:
            typ = field_map[fn]
            label = fn.replace("_", " ")
            html += f'        <div class="settings-field">\n'
            html += f'          <label>{label}</label>\n'
            html += (f'          <input placeholder="{typ}" data-type="{typ}"'
                     f' data-name="{fn}" oninput="validateField(this)">\n')
            html += f'          <span class="error-msg"></span>\n'
            html += f'        </div>\n'
        html += f'      </div>\n'
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
      - submitAll() reads Download_a_copy from the Settings_form input.
      - toggleSettingsSection() collapses all Settings subcats except headers.
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

    # Settings: client-only, not persisted to SPIFFS, excluded from C gen.
    settings_fields = tables.get("Settings", [])
    settings_subcats = subcategories.get("Settings", {})

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
    headers_fields = settings_subcats.get("headers", [])
    headers_field_map = {n: typ for n, typ in settings_fields}
    headers_block = gen_headers_block(headers_fields, headers_field_map) \
        if headers_fields else ""
    stable_js = gen_stable_js()

    # HTML: form_tables only, validity fields get special oninput handler
    base = template.replace("%SETTINGS_BLOCK%", settings_block)
    base = base.replace("%HEADERS_BLOCK%", headers_block)
    base = base.replace("%STABLE_JS%", stable_js)
    base = base.replace("%TABLE_BLOCKS%", gen_table_blocks(
        form_tables, validity_field_names, subcategories,
        hidden_by_default={"Metadata"}))
    base = base.replace("%SCHEMA_JS%", gen_schema_js(form_tables))
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
        f.write(gen_c_schema_h(form_tables, subcategories))

    with open("xml_parser.cpp", "w") as f:
        f.write(gen_c_xml_parser(form_tables, subcategories, xml_map,
                                 always_overwrite={"Metadata"}))

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
