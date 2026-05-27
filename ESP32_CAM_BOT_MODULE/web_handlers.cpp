// ==============================================================================
// Datei: web_handlers.cpp
// Projekt: ESP32-CAM-BOT-MODULE
// Beschreibung: Enthält alle HTTP-Handler-Funktionen für die Routen des
//               Webservers (Steuerung, Stream, Kalibrierung und Licht).
// ==============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// ── Externe Variablen aus der Hauptdatei (ESP32-CAM-BOT-MODULE.ino) ──────────
extern int MIN_PWM;
extern float MAX_SPEED_CAL;
extern float KORREKTUR_LINKS;
extern float KORREKTUR_RECHTS;
extern bool lichtAn;
extern bool apModus;

// ── Externe Funktionen aus der Hauptdatei ─────────────────────────────────────
void tankMix(int fahrt, int kurve);
void motorStop();

// ── Externer Verweis auf die HTML-Daten (html_data.cpp) ──────────────────────
extern const char INDEX_HTML[] PROGMEM;

// ── Stream-Definitionen für den MJPEG-Server ─────────────────────────────────
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ==============================================================================
// HTTP-Handler – Routen-Logik
// ==============================================================================

/**
 * Handler für die Hauptseite (/)
 * Sendet den im Flash gespeicherten HTML/JS/CSS-Code an den Browser.
 */
esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
    return ESP_OK;
}

/**
 * Handler für die Fahrzeugsteuerung (/ctrl)
 * Liest die Parameter 'fahrt' und 'kurve' aus und steuert die Motoren an.
 */
esp_err_t ctrl_handler(httpd_req_t *req) {
    char buf[64];
    int fahrt = 0, kurve = 0;
    
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char val[16];
        if (httpd_query_key_value(buf, "fahrt", val, sizeof(val)) == ESP_OK) fahrt = atoi(val);
        if (httpd_query_key_value(buf, "kurve", val, sizeof(val)) == ESP_OK) kurve = atoi(val);
    }
    
    Serial.printf("[CTRL] fahrt=%d kurve=%d\n", fahrt, kurve);
    tankMix(fahrt, kurve);
    
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

/**
 * Handler für die Live-Kalibrierung (/cal)
 * Nimmt Tuning-Parameter entgegen und aktualisiert die globalen Variablen.
 */
esp_err_t cal_handler(httpd_req_t *req) {
    char buf[128];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char val[16];
        if (httpd_query_key_value(buf, "min_pwm", val, sizeof(val)) == ESP_OK) MIN_PWM = atoi(val);
        if (httpd_query_key_value(buf, "max_speed", val, sizeof(val)) == ESP_OK) MAX_SPEED_CAL = atof(val);
        if (httpd_query_key_value(buf, "korr_l", val, sizeof(val)) == ESP_OK) KORREKTUR_LINKS = atof(val);
        if (httpd_query_key_value(buf, "korr_r", val, sizeof(val)) == ESP_OK) KORREKTUR_RECHTS = atof(val);
    }
    
    Serial.printf("[CAL] min=%d speed=%.2f L=%.2f R=%.2f\n", MIN_PWM, MAX_SPEED_CAL, KORREKTUR_LINKS, KORREKTUR_RECHTS);
    
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

/**
 * Handler für die Scheinwerfer-Lichtsteuerung (/licht)
 * Schaltet die weiße Ausleucht-LED (GPIO 21) um.
 */
esp_err_t licht_handler(httpd_req_t *req) {
    lichtAn = !lichtAn;
    digitalWrite(21, lichtAn ? HIGH : LOW);
    Serial.printf("[LICHT] %s\n", lichtAn ? "AN" : "AUS");
    
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, lichtAn ? "AN" : "AUS", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * Handler für die Abfrage der Stream-URL (/streamurl)
 * Liefert die absolute Adresse des MJPEG-Streams auf Port 81.
 */
esp_err_t streamurl_handler(httpd_req_t *req) {
    char url[64];
   snprintf(url, sizeof(url), "http://%s:81/stream", (apModus ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, url, strlen(url));
    return ESP_OK;
}

/**
 * Handler für den MJPEG-Videostream (/stream auf Port 81)
 * Holt fortlaufend Kamerabilder und sendet sie als Multipart-Chunk.
 */
esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part[64];
    
    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;
    
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    Serial.println("[STREAM] gestartet");
    
    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[STREAM] kein Frame");
            res = ESP_FAIL;
        } else {
            size_t hlen = snprintf(part, sizeof(part), STREAM_PART, fb->len);
            res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
            if (res == ESP_OK) res = httpd_resp_send_chunk(req, part, hlen);
            if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        if (res != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(40)); // Begrenzung auf ca. 25 FPS zur CPU-Entlastung
    }
    
    Serial.println("[STREAM] beendet");
    motorStop(); // Sicherheits-Stopp bei Verbindungsabriss
    return res;
}
