// ==============================================================================
// Datei: web_interface.h
// Projekt: ESP32-CAM-BOT-MODULE
// Beschreibung: Zentrale Header-Brücke. Deklariert die Schnittstellen der 
//               ausgelagerten Webserver-Komponenten für das Hauptprogramm.
// ==============================================================================

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Weitverweis auf das HTML-Datenfeld mit erzwungener C-Namensgebung
    extern const char INDEX_HTML[] PROGMEM;

#ifdef __cplusplus
}
#endif

// ── Deklaration der HTTP-Handler (web_handlers.cpp) ──────────────────────────
esp_err_t index_handler(httpd_req_t *req);
esp_err_t ctrl_handler(httpd_req_t *req);
esp_err_t cal_handler(httpd_req_t *req);
esp_err_t licht_handler(httpd_req_t *req);
esp_err_t streamurl_handler(httpd_req_t *req);
esp_err_t stream_handler(httpd_req_t *req);

// ── Deklaration der Server-Startfunktion (web_server.cpp) ────────────────────
/**
 * Initialisiert und startet die beiden asynchronen HTTP-Server-Instanzen
 * für die Fahrzeugsteuerung (Port 80) und den Videostream (Port 81).
 */
void serverStart();

#endif // WEB_INTERFACE_H