// ==============================================================================
// Datei: html_data.cpp
// Projekt: ESP32-CAM-BOT-MODULE
// Beschreibung: Enthält ausschließlich das HTML/JS/CSS-Webinterface.
//               Gespeichert im Flash-Speicher (PROGMEM) zur RAM-Schonung.
// ==============================================================================

#include <Arduino.h>

// Das "extern "C"" zwingt den Compiler, den Namen für den Linker im Originalzustand zu lassen
extern "C" const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>CamBot Joystick & Tuning</title>
<style>
:root {
 --bg: #0d0d0d;
 --panel: #1a1a1a;
 --border: #2e2e2e;
 --accent: #e8c547;
 --text: #e0e0e0;
 --muted: #666;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
 background: var(--bg); color: var(--text); font-family: 'Courier New', monospace;
 display: flex; flex-direction: column; align-items: center; min-height: 100vh;
 padding: 12px; gap: 12px; user-select: none; -webkit-user-select: none;
}
h1 { font-size: 1.1rem; letter-spacing: 0.25em; color: var(--accent); text-transform: uppercase; padding: 6px 0; }
#stream-box {
 width: 100%; max-width: 640px; background: #000; border: 1px solid var(--border);
 border-radius: 4px; overflow: hidden; aspect-ratio: 4/3; display: flex; align-items: center; justify-content: center;
}
#stream { width: 100%; height: 100%; object-fit: contain; display: none; }
#stream-placeholder { color: var(--muted); font-size: 0.8rem; letter-spacing: 0.1em; }
.btn-row { display: flex; gap: 10px; width: 100%; max-width: 640px; }
button {
 flex: 1; padding: 10px; border: 1px solid var(--border); border-radius: 3px;
 background: var(--panel); color: var(--text); font-family: inherit; font-size: 0.85rem;
 letter-spacing: 0.1em; transition: border-color 0.15s, color 0.15s; cursor: pointer;
}
button:hover { border-color: var(--accent); color: var(--accent); }
button:active { background: #222; }
#btn-licht.an { border-color: var(--accent); color: var(--accent); }
.controls {
 width: 100%; max-width: 640px; background: var(--panel); border-radius: 4px;
 border: 1px solid var(--border); padding: 20px; display: flex; justify-content: center; align-items: center;
}
#joystick-zone {
 width: 180px; height: 180px; background: #111; border: 2px dashed var(--border);
 border-radius: 50%; position: relative; touch-action: none;
}
#joystick-knob {
 width: 54px; height: 54px; background: var(--panel); border: 2px solid var(--accent);
 border-radius: 50%; position: absolute; left: 61px; top: 61px; cursor: pointer;
}
#status { font-size: 0.7rem; color: var(--muted); letter-spacing: 0.08em; text-align: center; }
.tuning-panel {
 width: 100%; max-width: 640px; background: var(--panel); border: 1px solid var(--border);
 border-radius: 4px; padding: 12px; display: flex; flex-direction: column; gap: 10px;
}
.tuning-title {
 font-size: 0.8rem; color: var(--accent); cursor: pointer; text-align: center;
 letter-spacing: 0.1em; padding: 4px; border-bottom: 1px solid #222;
}
.tuning-content { display: none; flex-direction: column; gap: 8px; }
.tuning-content.open { display: flex; }
.input-group { display: flex; justify-content: space-between; align-items: center; font-size: 0.8rem; }
.input-group input {
 background: #111; border: 1px solid var(--border); color: #fff;
 padding: 6px; width: 80px; text-align: center; font-family: inherit; border-radius: 3px;
}
.input-group input:focus { border-color: var(--accent); outline: none; }
#btn-save { background: #222; margin-top: 5px; max-height: 35px; }
</style>
</head>
<body>
<h1>&#9654; CamBot</h1>
<div id="stream-box">
 <img id="stream" alt="stream">
 <span id="stream-placeholder">STREAM GESTOPPT</span>
</div>
<div class="btn-row">
 <button id="btn-stream" onclick="toggleStream()">&#9654; STREAM</button>
 <button id="btn-licht" onclick="toggleLicht()">&#9788; LICHT</button>
</div>
<div class="controls">
 <div id="joystick-zone">
 <div id="joystick-knob"></div>
 </div>
</div>
<div class="tuning-panel">
 <div class="tuning-title" onclick="toggleTuning()">&#9881; KALIBRIERUNG (KLICKEN)</div>
 <div id="tuning-box" class="tuning-content">
 <div class="input-group">
 <label>Mindest-PWM (0-255):</label>
 <input type="number" id="inp-min" value="70" min="0" max="255">
 </div>
 <div class="input-group">
 <label>Max Speed (0.0 - 1.0):</label>
 <input type="number" id="inp-max" value="0.85" step="0.05" min="0" max="1">
 </div>
 <div class="input-group">
 <label>Korrektur Links (0.5 - 1.0):</label>
 <input type="number" id="inp-kl" value="1.00" step="0.01" min="0.5" max="1">
 </div>
 <div class="input-group">
 <label>Korrektur Rechts (0.5 - 1.0):</label>
 <input type="number" id="inp-kr" value="1.00" step="0.01" min="0.5" max="1">
 </div>
 <button id="btn-save" onclick="sendCalibration()">WERTE ANWENDEN</button>
 </div>
</div>
<div id="status"></div>
<script>
let streamAktiv = false;
let lichtAn = false;
let sendTimer = null;
let streamUrl = null;
const zone = document.getElementById('joystick-zone');
const knob = document.getElementById('joystick-knob');
const maxDistance = 60;
const startX = 61;
const startY = 61;
let active = false;
fetch('/streamurl').then(r => r.text()).then(u => { streamUrl = u; });
zone.addEventListener('touchstart', startMove, {passive: false});
window.addEventListener('touchend', stopMove);
window.addEventListener('touchmove', move, {passive: false});
zone.addEventListener('mousedown', startMove);
window.addEventListener('mouseup', stopMove);
window.addEventListener('mousemove', move);
function startMove(e) { active = true; move(e); }
function stopMove() {
if (!active) return;
 active = false;
 knob.style.left = startX + 'px'; knob.style.top = startY + 'px';
 update(0, 0); 
}
function move(e) {
if (!active) return;
 e.preventDefault();
 let clientX, clientY;
if (e.touches) { clientX = e.touches[0].clientX; clientY = e.touches[0].clientY; }
else { clientX = e.clientX; clientY = e.clientY; }
const rect = zone.getBoundingClientRect();
const centerX = rect.left + rect.width / 2;
const centerY = rect.top + rect.height / 2;
 let deltaX = clientX - centerX;
 let deltaY = clientY - centerY;
 let distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
if (distance > maxDistance) {
 deltaX = (deltaX / distance) * maxDistance;
 deltaY = (deltaY / distance) * maxDistance;
 }
 knob.style.left = (startX + deltaX) + 'px';
 knob.style.top = (startY + deltaY) + 'px';
 let fahrt = Math.round((-deltaY / maxDistance) * 100);
 let kurve = Math.round((deltaX / maxDistance) * 100);
 update(fahrt, kurve);
}
function update(f, k) {
 clearTimeout(sendTimer);
if (f === 0 && k === 0) { sendCtrl(0, 0); }
else { sendTimer = setTimeout(() => sendCtrl(f, k), 40); }
}
function sendCtrl(f, k) {
 fetch('/ctrl?fahrt=' + f + '&kurve=' + k)
 .then(() => setStatus('fahrt=' + f + ' kurve=' + k))
 .catch(() => setStatus('FEHLER - Verbindung verloren'));
}
function toggleTuning() {
 document.getElementById('tuning-box').classList.toggle('open');
}
function sendCalibration() {
const minPwm = document.getElementById('inp-min').value;
const maxSpeed = document.getElementById('inp-max').value;
const korrL = document.getElementById('inp-kl').value;
const korrR = document.getElementById('inp-kr').value;
 fetch(`/cal?min_pwm=${minPwm}&max_speed=${maxSpeed}&korr_l=${korrL}&korr_r=${korrR}`)
 .then(() => setStatus('Kalibrierung erfolgreich angewendet!'))
 .catch(() => setStatus('FEHLER beim Senden der Kalibrierung'));
}
function toggleStream() {
const img = document.getElementById('stream');
const ph = document.getElementById('stream-placeholder');
const btn = document.getElementById('btn-stream');
 streamAktiv = !streamAktiv;
if (streamAktiv && streamUrl) {
 img.src = streamUrl; img.style.display = 'block'; ph.style.display = 'none';
 btn.textContent = '\u23F9 STREAM';
 setStatus('Stream laeuft auf Port 81');
 } else {
 img.src = ''; img.style.display = 'none'; ph.style.display = 'block';
 btn.textContent = '\u25B6 STREAM';
 streamAktiv = false; setStatus('Stream gestoppt');
 }
}
function toggleLicht() {
 fetch('/licht').then(r => r.text()).then(t => {
 lichtAn = (t === 'AN');
 document.getElementById('btn-licht').classList.toggle('an', lichtAn);
 setStatus('Licht ' + t);
 }).catch(() => setStatus('FEHLER - Licht'));
}
function setStatus(msg) { document.getElementById('status').textContent = msg; }
</script>
</body>
</html>
)rawliteral";
