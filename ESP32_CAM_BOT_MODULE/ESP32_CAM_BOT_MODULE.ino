/*
 * ESP32_S3_CAM_BOT_MODULE
 * Kamera-Roboter-Sketch fuer GOOUUU ESP32-S3-CAM
 * - MJPEG-Stream auf Port 81 (eigener Server-Task)
 * - Steuerseite auf Port 80
 * - VGA, mittlere Qualitaet
 * - Tanksteuerung: Joystick-Interface
 * - Licht-Button & Live-Kalibrierung
 *
 * Motorpins:
 * Motor Links: PIN_L_A / PIN_L_B (GPIO 4 & 5)
 * Motor Rechts: PIN_R_A / PIN_R_B (GPIO 6 & 7)
 * Licht: PIN_LED_WEISS (GPIO 21)
 */
#define SKETCH_NAME "ESP32_S3_CamBot_nach_google_2_"
#define SKETCH_VERSION "V_x"

// ── Kameramodell ─────────────────────────────────────────────────────────────
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/ledc.h"
#include <WiFi.h>

// Einbindung des neuen Moduls für die Weboberfläche
#include "web_interface.h"

// ── WLAN-Zugangsdaten ─────────────────────────────────────────────────────────
#define WIFI_SSID "Pastor Rocha"
#define WIFI_PASS "50869782405465942742"
//#define WIFI_SSID "FRITZ!Box 7530 ZD"
//#define WIFI_PASS "92610962075564612677"

// ── Motorpins (GOOUUU ESP32-S3-CAM) ──────────────────────────────────────────
#define PIN_L_A 1
#define PIN_L_B 2
#define PIN_R_A 19
#define PIN_R_B 20

// ── Licht-Pins (GOOUUU ESP32-S3-CAM) ─────────────────────────────────────────
#define PIN_LED_WEISS 21  // Deine aktuelle Weisse LED (später RGB-Rot)
#define PIN_LED_GRUEN 47  // Für spätere RGB-Erweiterung reserviert
#define PIN_LED_BLAU 48   // Für spätere RGB-Erweiterung reserviert

// Globaler Lichtstatus
bool lichtAn = false;

// ── PWM Frequenzen ───────────────────────────────────────────────────────────
#define PWM_FREQ 1000
#define PWM_RES 8

// --- LIVE-KALIBRIERUNG (Dynamisch veränderbar) ---
int MIN_PWM = 70;            // Mindest-PWM für das Anlaufen
float MAX_SPEED_CAL = 0.85;  // Allgemeine Geschwindigkeitsgrenze (0.0 bis 1.0)
float KORREKTUR_LINKS = 1.00;
float KORREKTUR_RECHTS = 1.00;  // Startwert auf Gleichlauf setzen

// Server Handles für die HTTP-Serverinstanzen
httpd_handle_t ctrl_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

// ════════════════════════════════════════════════════════════════════════════
// Hardware-Setup Pins
// ════════════════════════════════════════════════════════════════════════════
void setupPins() {
  // Der ESP32-S3 nutzt Kanäle für LEDC-PWM
  ledcAttachChannel(PIN_L_A, 5000, 8, 0);  // Pin, Frequenz (5kHz), Auflösung (8-Bit), Kanal 0
  ledcAttachChannel(PIN_L_B, 5000, 8, 1);  // Kanal 1
  ledcAttachChannel(PIN_R_A, 5000, 8, 2);  // Kanal 2
  ledcAttachChannel(PIN_R_B, 5000, 8, 3);  // Kanal 3

  // LED Pins vorbereiten
  pinMode(PIN_LED_WEISS, OUTPUT);
  pinMode(PIN_LED_GRUEN, OUTPUT);
  pinMode(PIN_LED_BLAU, OUTPUT);

  // Alles beim Start AUS
  digitalWrite(PIN_LED_WEISS, LOW);
  digitalWrite(PIN_LED_GRUEN, LOW);
  digitalWrite(PIN_LED_BLAU, LOW);
}

// ════════════════════════════════════════════════════════════════════════════
// Motor-Steuerung (ESP32-S3 optimiert)
// ════════════════════════════════════════════════════════════════════════════
void motorLinks(int pwm) {
  if (pwm == 0) {
    ledcWriteChannel(0, 0);
    ledcWriteChannel(1, 0);
    return;
  }
  bool vorwaerts = (pwm >= 0);
  pwm = abs(pwm);

  // Mapping auf das Minimum-Anlaufmoment
  pwm = MIN_PWM + (pwm * (255 - MIN_PWM) / 255);
  pwm = constrain((int)(pwm * MAX_SPEED_CAL * KORREKTUR_LINKS), 0, 255);

  ledcWriteChannel(0, vorwaerts ? pwm : 0);
  ledcWriteChannel(1, vorwaerts ? 0 : pwm);
}

void motorRechts(int pwm) {
  if (pwm == 0) {
    ledcWriteChannel(2, 0);
    ledcWriteChannel(3, 0);
    return;
  }
  bool vorwaerts = (pwm >= 0);
  pwm = abs(pwm);

  // Mapping auf das Minimum-Anlaufmoment
  pwm = MIN_PWM + (pwm * (255 - MIN_PWM) / 255);
  pwm = constrain((int)(pwm * MAX_SPEED_CAL * KORREKTUR_RECHTS), 0, 255);

  ledcWriteChannel(2, vorwaerts ? pwm : 0);
  ledcWriteChannel(3, vorwaerts ? 0 : pwm);
}

void motorStop() {
  ledcWriteChannel(0, 0);
  ledcWriteChannel(1, 0);
  ledcWriteChannel(2, 0);
  ledcWriteChannel(3, 0);
}

// Mischt die Signale von Vorwärtsfahrt und Kurve für die Ketten/Panzersteuerung
void tankMix(int fahrt, int kurve) {
  if (fahrt == 0 && kurve == 0) {
    motorStop();
    return;
  }
  int l = constrain(fahrt + kurve, -100, 100);
  int r = constrain(fahrt - kurve, -100, 100);
  motorLinks(l * 255 / 100);
  motorRechts(r * 255 / 100);
}

// ════════════════════════════════════════════════════════════════════════════
// Kamera-Initialisierung
// ════════════════════════════════════════════════════════════════════════════
bool kameraInit() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_4;  // 0-3 fuer Motoren reserviert
  cfg.ledc_timer = LEDC_TIMER_1;
  cfg.pin_d0 = Y2_GPIO_NUM;
  cfg.pin_d1 = Y3_GPIO_NUM;
  cfg.pin_d2 = Y4_GPIO_NUM;
  cfg.pin_d3 = Y5_GPIO_NUM;
  cfg.pin_d4 = Y6_GPIO_NUM;
  cfg.pin_d5 = Y7_GPIO_NUM;
  cfg.pin_d6 = Y8_GPIO_NUM;
  cfg.pin_d7 = Y9_GPIO_NUM;
  cfg.pin_xclk = XCLK_GPIO_NUM;
  cfg.pin_pclk = PCLK_GPIO_NUM;
  cfg.pin_vsync = VSYNC_GPIO_NUM;
  cfg.pin_href = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn = PWDN_GPIO_NUM;
  cfg.pin_reset = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 16000000;
  cfg.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    cfg.frame_size = FRAMESIZE_QVGA;
    cfg.jpeg_quality = 15;
    cfg.fb_count = 1;
    cfg.grab_mode = CAMERA_GRAB_LATEST;
    cfg.fb_location = CAMERA_FB_IN_PSRAM;
    Serial.println("PSRAM gefunden");
  } else {
    cfg.frame_size = FRAMESIZE_QVGA;
    cfg.jpeg_quality = 20;
    cfg.fb_count = 1;
    cfg.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    cfg.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("-- kein PSRAM --");
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Fehler 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 0);
  s->set_brightness(s, 2);
  s->set_saturation(s, 1);
  s->set_contrast(s, 1);
  Serial.println("[CAM] OK");
  return true;
}

// ════════════════════════════════════════════════════════════════════════════
// Setup & Loop (Die vom Linker vermissten Pflichtfunktionen!)
// ════════════════════════════════════════════════════════════════════════════
void setup() {
  // Originaler Befehl zum zuverlässigen Deaktivieren des Detektors auf dem ESP32-S3:
  REG_CLR_BIT(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);

  Serial.begin(115200);
  delay(800);
  Serial.printf("\n%s %s\n", SKETCH_NAME, SKETCH_VERSION);
  // Debug-Ausgabe: Dateiname + Datum + Zeit (wie vom Nutzer gefordert)
  Serial.println(__FILE__);
  Serial.println(__DATE__);
  Serial.println(__TIME__);

  // PSRAM-Info
  Serial.println(F("PSRAM size:"));
  Serial.println(ESP.getPsramSize());

  setupPins();
  motorStop();

  if (!kameraInit()) {
    Serial.println("[FATAL] Kamera nicht initialisiert – Neustart in 5s");
    delay(5000);
    ESP.restart();
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] Verbinde mit ");
  Serial.println(WIFI_SSID);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    if (counter > 60) {
      Serial.println("\n[WiFi] Verbindung fehlgeschlagen – Starte neu...");
      ESP.restart();
    }
  }

  WiFi.setSleep(false);
  Serial.printf("\n[WiFi] Verbunden – IP: %s\n", WiFi.localIP().toString().c_str());

  // Server aus der web_interface.cpp Datei starten
  serverStart();
  Serial.println("[OK] Bereit.");
}

void loop() {
  // Da der Webserver asynchron in eigenen Tasks läuft,
  // schlafen wir hier einfach, um die CPU zu entlasten.
  vTaskDelay(pdMS_TO_TICKS(5000));
}
