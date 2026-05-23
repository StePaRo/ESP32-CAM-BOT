// ==============================================================================
// Datei: web_server.cpp
// Projekt: ESP32-CAM-BOT-MODULE
// Beschreibung: Übernimmt die Konfiguration und den System-Start der beiden
//               Webserver-Instanzen (Port 80 und Port 81).
// ==============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include "esp_http_server.h"

// ── Externe Vorab-Deklarationen der Handler aus web_handlers.cpp ──────────────
esp_err_t index_handler(httpd_req_t *req);
esp_err_t ctrl_handler(httpd_req_t *req);
esp_err_t cal_handler(httpd_req_t *req);
esp_err_t licht_handler(httpd_req_t *req);
esp_err_t streamurl_handler(httpd_req_t *req);
esp_err_t stream_handler(httpd_req_t *req);

// ── Server starten ───────────────────────────────────────────────────────────
void serverStart() {
    // Externe Server-Handles aus der Hauptdatei referenzieren
    extern httpd_handle_t ctrl_httpd;
    extern httpd_handle_t stream_httpd;

    // ── Port 80: Steuerung & Tuning ───────────────────────────────────────────
    {
        httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
        cfg.server_port = 80;
        cfg.ctrl_port = 32768;
        cfg.stack_size = 4096;
        cfg.max_uri_handlers = 6;
        cfg.core_id = 1; // Bindung an Core 1 für saubere Task-Trennung
        
        httpd_uri_t u_index     = { "/",          HTTP_GET, index_handler,     NULL };
        httpd_uri_t u_ctrl      = { "/ctrl",      HTTP_GET, ctrl_handler,      NULL };
        httpd_uri_t u_cal       = { "/cal",       HTTP_GET, cal_handler,       NULL };
        httpd_uri_t u_licht     = { "/licht",     HTTP_GET, licht_handler,     NULL };
        httpd_uri_t u_streamurl = { "/streamurl", HTTP_GET, streamurl_handler, NULL };
        
        if (httpd_start(&ctrl_httpd, &cfg) == ESP_OK) {
            httpd_register_uri_handler(ctrl_httpd, &u_index);
            httpd_register_uri_handler(ctrl_httpd, &u_ctrl);
            httpd_register_uri_handler(ctrl_httpd, &u_cal);
            httpd_register_uri_handler(ctrl_httpd, &u_licht);
            httpd_register_uri_handler(ctrl_httpd, &u_streamurl);
            Serial.printf("[HTTP] Steuerung http://%s\n", WiFi.localIP().toString().c_str());
        }
    }

    // ── Port 81: High-Speed Video-Stream ──────────────────────────────────────
    {
        httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
        cfg.server_port = 81;
        cfg.ctrl_port = 32769;
        cfg.stack_size = 8192;
        cfg.max_uri_handlers = 2;
        
        // Zwingt den datenintensiven Video-Task auf Core 1 (weg vom WiFi-Stack auf Core 0)
        cfg.core_id = 1; 
        
        httpd_uri_t u_stream = { "/stream", HTTP_GET, stream_handler, NULL };
        
        if (httpd_start(&stream_httpd, &cfg) == ESP_OK) {
            httpd_register_uri_handler(stream_httpd, &u_stream);
            Serial.printf("[HTTP] Stream http://%s:81/stream\n", WiFi.localIP().toString().c_str());
        }
    }
}