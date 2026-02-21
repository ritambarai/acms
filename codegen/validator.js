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
