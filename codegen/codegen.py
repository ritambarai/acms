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
        "  /* ── Name field: stepped check — type row → bool shortcut → unit row ── */\n"
        '  if (fieldName === "Name") {\n'
        '    var form = input.closest(\'[id$="_form"]\');\n'
        '    var typeInput = form ? form.querySelector(\'[data-name="Type"]\') : null;\n'
        '    var typeVal = typeInput ? typeInput.value.trim().toLowerCase() : "";\n'
        '    var half = input.closest(".half");\n'
        '    var insertBtn = half ? half.querySelector(".insert-btn") : null;\n'
        '    if (typeVal === "type" || typeVal === "units" || typeVal === "validity") {\n'
        '      input.style.borderColor = "";\n'
        '      input.classList.remove("invalid");\n'
        '      errSpan.textContent = "";\n'
        '      errSpan.style.color = "";\n'
        '      input.dataset.lastAlert = "";\n'
        "      return true;\n"
        "    }\n"
        '    var classInput = form ? form.querySelector(\'[data-name="Class"]\') : null;\n'
        '    var classVal = classInput ? classInput.value.trim() : "";\n'
        '    var varSchema = (typeof Variables_SCHEMA !== "undefined") ? Variables_SCHEMA : [];\n'
        "    var classIdx = -1, typeIdx = -1, valueIdx = -1, nameIdx = -1;\n"
        "    for (var i = 0; i < varSchema.length; i++) {\n"
        '      if (varSchema[i].name === "Class") classIdx = i;\n'
        '      if (varSchema[i].name === "Type")  typeIdx  = i;\n'
        '      if (varSchema[i].name === "Value") valueIdx = i;\n'
        '      if (varSchema[i].name === "Name")  nameIdx  = i;\n'
        "    }\n"
        '    var varRows = (state && state["Variables"]) ? state["Variables"] : [];\n'
        "    /* gate on class existence first so type/unit alerts can fire */\n"
        "    var classRows = varRows.filter(function(row) {\n"
        "      return classIdx >= 0 && row[classIdx] && classVal &&\n"
        "             row[classIdx].toLowerCase() === classVal.toLowerCase();\n"
        "    });\n"
        "    if (classRows.length === 0) {\n"
        '      input.style.borderColor = "red";\n'
        '      input.classList.add("invalid");\n'
        '      errSpan.textContent = "NO MATCH";\n'
        '      errSpan.style.color = "red";\n'
        "      if (insertBtn) insertBtn.disabled = true;\n"
        '      input.dataset.lastAlert = "";\n'
        "      return false;\n"
        "    }\n"
        "\n"
        "    /* ── Step 1: type row must exist ── */\n"
        "    var typeRow = null;\n"
        "    for (var j = 0; j < classRows.length; j++) {\n"
        "      if (typeIdx >= 0 && classRows[j][typeIdx] &&\n"
        '          classRows[j][typeIdx].toLowerCase() === "type") {\n'
        "        typeRow = classRows[j]; break;\n"
        "      }\n"
        "    }\n"
        "    if (!typeRow) {\n"
        '      input.style.borderColor = "red";\n'
        '      input.classList.add("invalid");\n'
        '      errSpan.textContent = "NO TYPE";\n'
        '      errSpan.style.color = "red";\n'
        "      if (insertBtn) insertBtn.disabled = true;\n"
        "      return false;\n"
        "    }\n"
        "\n"
        "    /* ── Step 2: resolve type label from Metadata ── */\n"
        "    var tClassKey = Object.keys(dict).find(function(k) {\n"
        '      return k.toLowerCase() === "type";\n'
        '    }) || "type";\n'
        "    var tKeyVal = (typeRow[valueIdx] !== null && typeRow[valueIdx] !== undefined)\n"
        '      ? String(typeRow[valueIdx]) : "";\n'
        "    var typeLabel = (dict[tClassKey] && dict[tClassKey].hasOwnProperty(tKeyVal))\n"
        "      ? dict[tClassKey][tKeyVal].toLowerCase() : \"\";\n"
        '    input.style.borderColor = "green";\n'
        '    input.classList.remove("invalid");\n'
        "    errSpan.textContent = typeLabel.toUpperCase();\n"
        '    errSpan.style.color = "green";\n'
        '    input.dataset.lastAlert = "";\n'
        "\n"
        '    /* ── Step 3: bool/choice → skip unit check, still validate name ── */\n'
        '    if (typeLabel === "bool" || typeLabel === "choice") {\n'
        "      var boolNameMatch = classRows.some(function(row) {\n"
        "        return nameIdx >= 0 && row[nameIdx] && v &&\n"
        "               row[nameIdx].toLowerCase() === v.toLowerCase();\n"
        "      });\n"
        "      if (!boolNameMatch) {\n"
        '        input.style.borderColor = "red";\n'
        '        input.classList.add("invalid");\n'
        '        errSpan.textContent = "NO MATCH";\n'
        '        errSpan.style.color = "red";\n'
        "        if (insertBtn) insertBtn.disabled = true;\n"
        '        input.dataset.lastAlert = "";\n'
        "        return false;\n"
        "      }\n"
        "      if (insertBtn) insertBtn.disabled = false;\n"
        "      return true;\n"
        "    }\n"
        "\n"
        "    /* ── Step 4: non-bool → unit row must exist ── */\n"
        "    var unitRow = null;\n"
        "    for (var k = 0; k < classRows.length; k++) {\n"
        "      if (typeIdx >= 0 && classRows[k][typeIdx] &&\n"
        '          classRows[k][typeIdx].toLowerCase() === "unit") {\n'
        "        unitRow = classRows[k]; break;\n"
        "      }\n"
        "    }\n"
        "    if (!unitRow) {\n"
        '      input.style.borderColor = "red";\n'
        '      input.classList.add("invalid");\n'
        '      errSpan.textContent = "NO UNIT";\n'
        '      errSpan.style.color = "red";\n'
        "      if (insertBtn) insertBtn.disabled = true;\n"
        "      return false;\n"
        "    }\n"
        "\n"
        "    /* ── Step 5: resolve unit message from Metadata (use classVal class only) ── */\n"
        "    var uKeyVal = (unitRow[valueIdx] !== null && unitRow[valueIdx] !== undefined)\n"
        '      ? String(unitRow[valueIdx]) : "";\n'
        "    var uClassKey = Object.keys(dict).find(function(k) {\n"
        "      return k.toLowerCase() === classVal.toLowerCase();\n"
        "    });\n"
        "    var uMsg = (uClassKey && dict[uClassKey].hasOwnProperty(uKeyVal))\n"
        "      ? dict[uClassKey][uKeyVal] : null;\n"
        "    if (uMsg !== null) {\n"
        "      /* ── Step 5b: name must also exist in this class ── */\n"
        "      var nameMatch = classRows.some(function(row) {\n"
        "        return nameIdx >= 0 && row[nameIdx] && v &&\n"
        "               row[nameIdx].toLowerCase() === v.toLowerCase();\n"
        "      });\n"
        "      if (!nameMatch) {\n"
        '        input.style.borderColor = "red";\n'
        '        input.classList.add("invalid");\n'
        '        errSpan.textContent = "NO MATCH";\n'
        '        errSpan.style.color = "red";\n'
        "        if (insertBtn) insertBtn.disabled = true;\n"
        '        input.dataset.lastAlert = "";\n'
        "        return false;\n"
        "      }\n"
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


def _gen_subcat_block(lines, t, subcat_name, subcat_fields, field_map, is_modbus=False):
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
    if is_modbus:
        lines.append(f"  float *value_ptr;   /* points to values struct Value field */")
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
                is_modbus = (sc_name.lower() == "modbus")
                lines.append(f"/* ─── {t} / {sc_name} ─── */")
                lines.append("")
                _gen_subcat_block(lines, t, sc_name, sc_fields, field_map, is_modbus)
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
            has_modbus = "modbus" in [s.lower() for s in subcat_names]
            has_values = "values" in [s.lower() for s in subcat_names]
            if has_modbus and has_values:
                # Check if Value field exists in values subcat
                values_sc = [s for s in subcat_names if s.lower() == "values"][0]
                if "Value" in subcats[values_sc]:
                    lines.append(f"    /* link modbus value_ptr → values.Value */")
                    lines.append(f"    modbus_tbl->rows[count].value_ptr = &values_tbl->rows[count].Value;")
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
#  MAIN
# ═══════════════════════════════════════════════════════

def main():
    if len(sys.argv) != 3:
        print("Usage: python codegen.py <schema.xsd> <template.html>")
        sys.exit(1)

    xsd_path = sys.argv[1]
    template_path = sys.argv[2]
    tables, subcategories = parse_xsd(xsd_path)

    # First 2 non-Controls categories for form/table + C outputs
    form_tables = {k: v for k, v in list(tables.items())[:2] if k != "Controls"}

    # Display order: Metadata first, then Variables
    if "Metadata" in form_tables and "Variables" in form_tables:
        form_tables = {"Metadata": form_tables["Metadata"], "Variables": form_tables["Variables"]}

    # Validity field names come from Controls > validity subcategory
    validity_field_names = set(subcategories.get("Controls", {}).get("validity", []))

    with open(template_path, "r") as f:
        template = f.read()

    # ── JS outputs ──
    validator_js = gen_validator_js(form_tables)
    validity_js = gen_validity_js() if validity_field_names else ""

    # validity.js contains both validator + validity checker
    combined_js = validator_js + "\n\n" + validity_js if validity_js else validator_js

    with open("validity.js", "w") as f:
        f.write(combined_js)

    # HTML: form_tables only, validity fields get special oninput handler
    base = template.replace("%TABLE_BLOCKS%", gen_table_blocks(
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
