// HMS demo page: asks the local API for the state, draws it, and sends each
// button or form as one request. All module data is inserted with textContent,
// never as HTML. No outside files or network calls.
"use strict";

let state = null;
const filters = { severity: "", state: "", adc: "" };

// ---- Talking to the API ----

async function call(method, path, params) {
  const query = new URLSearchParams(params || {}).toString();
  try {
    const response = await fetch(path + (query ? "?" + query : ""), { method });
    return await response.json();
  } catch (error) {
    return { ok: false, message: "The HMS program is not answering. Is it still running?" };
  }
}

// Sends one action, shows its result (with the module's verdict), then redraws.
async function act(path, params) {
  const result = await call("POST", path, params);
  showMessage(result);
  await refresh();
  return result;
}

function showMessage(result) {
  const box = document.getElementById("message");
  const verdict = result.verdict ? " (" + result.verdict + ")" : "";
  box.textContent = (result.ok ? "Done: " : "Rejected: ") + result.message + verdict;
  box.className = "message " + (result.ok ? "ok" : "bad");
}

async function refresh() {
  const result = await call("GET", "/api/state");
  if (!result.ok) { showMessage(result); return; }
  state = result;
  draw();
}

// ---- Small DOM helpers ----

function el(tag, props, children) {
  const node = document.createElement(tag);
  Object.entries(props || {}).forEach(([key, value]) => {
    if (key === "text") node.textContent = value;
    else if (key === "class") node.className = value;
    else if (key.startsWith("on")) node.addEventListener(key.slice(2), value);
    else node.setAttribute(key, value);
  });
  (children || []).forEach((child) => node.append(child));
  return node;
}

function fillTable(id, rows, emptyText, columns) {
  const body = document.querySelector("#" + id + " tbody");
  body.replaceChildren();
  if (rows.length === 0) {
    body.append(el("tr", { class: "empty" }, [el("td", { colspan: columns, text: emptyText })]));
    return;
  }
  rows.forEach((cells) => body.append(el("tr", {}, cells.map((c) => (c instanceof Node ? el("td", {}, [c]) : el("td", { text: String(c) }))))));
}

// Severity is always written out in words; the colour only adds to it.
function severityBadge(severity) {
  return el("span", { class: "sev " + severity.toLowerCase(), text: severity });
}

function tag(text, good) {
  return el("span", { class: "tag " + (good ? "ok" : "bad"), text });
}

function alarmPlace(a) {
  return a.adc + (a.detail ? " (" + a.detail + ")" : "");
}

// ---- Drawing each screen ----

function draw() {
  document.getElementById("clock").textContent = state.clock.text;
  document.getElementById("role").value = state.user.role.toLowerCase();
  document.getElementById("fail-history").checked = state.failures.history;
  document.getElementById("fail-sender").checked = state.failures.sender;
  drawAdcs();
  drawFilters("dash-filters");
  drawFilters("alarm-filters");
  drawAlarms();
  drawFeed();
  drawStock();
  drawWindows();
  drawPermissions();
}

function adcStatus(a) {
  return tag(a.status === "Online" ? "Online" : a.status === "Offline" ? "OFFLINE" : a.status, a.status === "Online");
}

function drawAdcs() {
  fillTable("adc-table", state.adcs.map((a) => [a.name, adcStatus(a), a.inMaintenance ? "In maintenance" : "No",
    a.silenced ? "Silenced" : "Sending", a.lastHeartbeat]), "No ADCs registered", 5);
  fillTable("sim-adcs", state.adcs.map((a) => [a.name, adcStatus(a), a.silenced ? "Silenced" : "Sending",
    el("div", { class: "row-actions" }, [
      el("button", { class: "small", text: "Send heartbeat", onclick: () => act("/api/heartbeat", { adc: a.id }) }),
      a.silenced
        ? el("button", { class: "small", text: "Resume", onclick: () => act("/api/resume", { adc: a.id }) })
        : el("button", { class: "small secondary", text: "Silence", onclick: () => act("/api/silence", { adc: a.id }) }),
      el("button", { class: "small secondary", text: "Decommission", onclick: () => act("/api/adc/decommission", { adc: a.id }) }),
    ])]), "No ADCs registered", 4);
}

function drawFilters(id) {
  const box = document.getElementById(id);
  const choice = (key, label, values) => {
    const select = el("select", { id: id + "-" + key, onchange: (e) => { filters[key] = e.target.value; draw(); } },
      [el("option", { value: "", text: "any" })].concat(values.map((v) => el("option", { value: v, text: v }))));
    select.value = filters[key];
    return el("label", {}, [label + " ", select]);
  };
  const adcs = [...new Set(state.alarms.map((a) => a.adc))];
  box.replaceChildren(choice("severity", "Severity", ["Critical", "High", "Low"]),
    choice("state", "State", ["Raised", "Acknowledged", "Resolved"]), choice("adc", "Where", adcs));
}

function passesFilters(a) {
  return (!filters.severity || a.severity === filters.severity) && (!filters.state || a.state === filters.state)
    && (!filters.adc || a.adc === filters.adc);
}

const SEVERITY_ORDER = { Critical: 0, High: 1, Low: 2 };

function drawAlarms() {
  const open = state.alarms.filter((a) => a.state !== "Resolved" && passesFilters(a))
    .sort((x, y) => SEVERITY_ORDER[x.severity] - SEVERITY_ORDER[y.severity] || x.id - y.id);
  fillTable("dash-alarms", open.map((a) => ["#" + a.id, severityBadge(a.severity), a.type, alarmPlace(a), a.state,
    a.destination + (a.escalated ? " (escalated)" : ""), a.raisedAt]), "No open alarms", 7);

  const all = state.alarms.filter(passesFilters).slice().reverse();
  fillTable("alarm-table", all.map((a) => {
    const note = el("input", { "aria-label": "Note for alarm " + a.id, placeholder: "note (required)" });
    const actions = a.state === "Resolved" ? "—" : el("div", { class: "row-actions" }, [
      a.state === "Raised" ? el("button", { class: "small", text: "Acknowledge", onclick: () => act("/api/alarm/ack", { id: a.id }) }) : "",
      note,
      el("button", { class: "small", text: "Resolve", onclick: () => act("/api/alarm/resolve", { id: a.id, note: note.value }) }),
    ]);
    return ["#" + a.id, severityBadge(a.severity), a.type, alarmPlace(a), a.state,
      a.destination + (a.escalated ? " (escalated)" : ""), a.raisedAt, actions];
  }), "No alarms", 8);
}

function drawFeed() {
  const list = document.getElementById("feed");
  list.replaceChildren(...state.feed.slice(0, 60).map((f) => el("li", {}, [
    el("span", { class: "when", text: f.time.slice(11) }), el("span", { class: "kind", text: f.kind }), f.text])));
}

function drawStock() {
  fillTable("stock-table", state.stock.map((s) => [s.adc, s.medication, s.count, s.capacity,
    tag(s.low ? "LOW (below 25%)" : "OK", !s.low)]), "No stock", 5);
  fillTable("hospital-table", state.hospital.map((m) => [m.medication,
    m.reserveCount + " of " + m.reserveCapacity, m.adcCount + " of " + m.adcCapacity,
    m.totalCount + " of " + m.totalCapacity, tag(m.verdict === "Low" ? "LOW (below 25%)" : m.verdict === "NotLow" ? "OK" : m.verdict, m.verdict !== "Low"),
    m.alarm]), "No medications", 6);
}

function drawWindows() {
  fillTable("window-table", state.windows.slice().reverse().map((w) => [w.adc, w.startText, w.endText, w.state,
    w.state === "Scheduled" || w.state === "Active"
      ? el("button", { class: "small secondary", text: "Cancel", onclick: () => act("/api/maintenance/cancel", { adc: w.adc, start: w.start }) })
      : "—"]), "No windows scheduled", 5);
}

function drawPermissions() {
  fillTable("perm-table", state.permissions.map((p) => [p.action,
    p.Pharmacist ? "yes" : "no", p.Technician ? "yes" : "no", p.Administrator ? "yes" : "no"]), "", 4);
}

// ---- Tabs ----

function showTab(name) {
  document.querySelectorAll(".tabs button").forEach((b) => b.setAttribute("aria-selected", String(b.dataset.tab === name)));
  document.querySelectorAll(".panel").forEach((p) => { p.hidden = p.id !== name; });
  try { localStorage.setItem("hms-tab", name); } catch (e) { /* storage may be blocked; the tab just isn't remembered */ }
}

// ---- Wiring the controls ----

function formValues(form) {
  const values = {};
  new FormData(form).forEach((value, key) => { if (String(value).trim() !== "") values[key] = String(value).trim(); });
  return values;
}

function onSubmit(id, handler) {
  document.getElementById(id).addEventListener("submit", (event) => {
    event.preventDefault();
    handler(formValues(event.target), event.submitter);
  });
}

function setUp() {
  document.querySelectorAll(".tabs button").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.tab)));
  let tab = "dashboard";
  try { tab = localStorage.getItem("hms-tab") || tab; } catch (e) { /* use the default */ }
  showTab(document.getElementById(tab) ? tab : "dashboard");

  document.getElementById("role").addEventListener("change", (e) => act("/api/role", { role: e.target.value }));
  document.getElementById("restart").addEventListener("click", () => act("/api/restart"));
  document.getElementById("to-six").addEventListener("click", () => act("/api/advance-to-six"));
  document.querySelectorAll("[data-advance]").forEach((b) =>
    b.addEventListener("click", () => act("/api/advance", { seconds: b.dataset.advance })));
  document.querySelectorAll("[data-scenario]").forEach((b) =>
    b.addEventListener("click", () => act("/api/scenario", { name: b.dataset.scenario })));
  document.getElementById("fail-history").addEventListener("change", (e) => act("/api/fail/history", { on: e.target.checked ? 1 : 0 }));
  document.getElementById("fail-sender").addEventListener("change", (e) => act("/api/fail/sender", { on: e.target.checked ? 1 : 0 }));

  onSubmit("form-advance", (v) => act("/api/advance", v));
  onSubmit("form-withdraw", (v, button) => {
    const op = button && button.value === "deposit" ? "deposit" : "withdraw";
    act("/api/stock/" + op, v);
  });
  onSubmit("form-set", (v) => act("/api/stock/set", v));
  onSubmit("form-reserve", (v) => act("/api/reserve/set", v));
  onSubmit("form-schedule", (v) => act("/api/maintenance/schedule", v));
  onSubmit("form-add-adc", (v) => act("/api/adc/add", v));
  onSubmit("form-raise", (v) => act("/api/alarm/raise", v));
  onSubmit("form-daily", async () => {
    const r = await act("/api/report/daily");
    document.getElementById("report-text").textContent = r.report || "(no report: " + (r.verdict || r.message) + ")";
  });
  onSubmit("form-summary", async (v) => {
    const r = await act("/api/report/summary", v);
    document.getElementById("report-text").textContent = r.report || "(no summary: " + (r.verdict || r.message) + ")";
  });
  onSubmit("form-history", async (v) => {
    const r = await call("GET", "/api/history", v);
    showMessage(r);
    fillTable("history-table", (r.entries || []).map((e) => [e.timeText, e.who, e.kind, e.adc, e.medication, e.detail]),
      r.ok ? "No matching entries" : "History could not be read", 6);
  });

  refresh().then(() => { if (state) showMessage({ ok: true, message: "Connected to the HMS at " + state.clock.text }); });
}

setUp();
