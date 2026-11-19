// Parksystem - ein Raspberry Pi Pico W
// Steuert Ein- und Ausfahrtsschranke (Servos), simuliert die Kameras ueber eine
// Weboberflaeche, fuehrt die Fahrzeugliste, berechnet die Parkgebuehr und nimmt
// (simulierte) Zahlungen entgegen.
// Sven Troxler & Lukas Traut
//
// Diese Datei verbindet nur noch Hardware und Weboberflaeche mit der Logik
// (lib/parksystem). Die Logik selbst ruft keine Hardware-Funktionen auf.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "config.h"
#include "hardware.h"
#include "kasse.h"
#include "schranke.h"
#include "webseite.h"

// ---------------------------------------------------------------------------
// Abhaengigkeiten erzeugen und der Logik uebergeben (Dependency Injection)
// ---------------------------------------------------------------------------
WebServer server(80);

PicoUhr uhr;
SerialProtokoll protokoll;
ServoAntrieb antriebEin(PIN_SERVO_EIN);
ServoAntrieb antriebAus(PIN_SERVO_AUS);

Kasse kasse(uhr, protokoll);
Schranke seiten[2] = {
  Schranke("ein", true,  antriebEin, kasse, uhr, protokoll),
  Schranke("aus", false, antriebAus, kasse, uhr, protokoll),
};

// ---------------------------------------------------------------------------
// Hilfsfunktionen Web
// ---------------------------------------------------------------------------
void zeitAusAnfrage() {
  if (server.hasArg("t")) uhr.setzeUnixZeit(strtoul(server.arg("t").c_str(), nullptr, 10));
}

void sendeJson(const String& json) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void antworte(const Antwort& a) {
  String t = a.text;
  t.replace("\"", "'");
  sendeJson(String("{\"ok\":") + (a.ok ? "true" : "false") + ",\"text\":\"" + t + "\"}");
}

void sendeFehler(const char* grund) {
  sendeJson(String("{\"ok\":false,\"grund\":\"") + grund + "\"}");
}

Schranke* seiteAusAnfrage() {
  String id = server.arg("seite");
  for (auto& s : seiten) if (id == s.id()) return &s;
  return nullptr;
}

String seiteJson(Schranke& s) {
  String meldung = s.meldung();
  meldung.replace("\"", "'");
  String j = "{\"zustand\":\"";
  j += zustandText(s.zustand());
  j += "\",\"meldung\":\"";  j += meldung;
  j += "\",\"typ\":\"";      j += s.typ();
  j += "\",\"kz\":\"";       j += s.kz();
  j += "\",\"offen\":";      j += s.zustand() == Zustand::OFFEN ? "true" : "false";
  j += ",\"hilfe\":";        j += s.zustand() == Zustand::HILFE ? "true" : "false";
  j += "}";
  return j;
}

// Schranken-Aktion ausfuehren: Seite suchen, Aktion aufrufen, Antwort senden
template <typename F>
void schrankenAktion(F aktion) {
  zeitAusAnfrage();
  Schranke* s = seiteAusAnfrage();
  if (!s) return antworte({false, "Seite fehlt"});
  antworte(aktion(*s));
}

// ---------------------------------------------------------------------------
// Web-Schnittstelle Schranken
// ---------------------------------------------------------------------------
// GET /api/status
void handleStatus() {
  zeitAusAnfrage();
  String j = "{\"frei\":";
  j += kasse.freiePlaetze();
  j += ",\"kapazitaet\":";  j += KAPAZITAET;
  j += ",\"ein\":";         j += seiteJson(seiten[0]);
  j += ",\"aus\":";         j += seiteJson(seiten[1]);
  j += "}";
  sendeJson(j);
}

// ---------------------------------------------------------------------------
// Web-Schnittstelle Kasse
// ---------------------------------------------------------------------------
// GET /api/fahrzeuge
void handleFahrzeuge() {
  zeitAusAnfrage();
  String s = "{\"jetzt\":";
  s += kasse.jetzt();
  s += ",\"kapazitaet\":";   s += KAPAZITAET;
  s += ",\"frei\":";         s += kasse.freiePlaetze();
  s += ",\"fahrzeuge\":[";
  bool erstes = true;
  for (int i = 0; i < KAPAZITAET; i++) {
    const Fahrzeug& f = kasse.platz(i);
    if (f.kz[0] == '\0') continue;
    if (!erstes) s += ",";
    s += "{\"kz\":\"";        s += f.kz;
    s += "\",\"ein\":";       s += f.einfahrt;
    s += ",\"manuell\":";     s += f.manuell ? "true" : "false";
    s += ",\"bezahlt\":";     s += f.bezahlt ? "true" : "false";
    s += ",\"gebuehr\":";     s += kasse.gebuehr(f);
    s += "}";
    erstes = false;
  }
  s += "]}";
  sendeJson(s);
}

// POST /api/zahlen?kz=..&art=kreditkarte|twint&ergebnis=ok|fehler  (US3)
void handleZahlen() {
  zeitAusAnfrage();
  uint32_t betrag = 0;
  KassenErgebnis e = kasse.zahlen(server.arg("kz").c_str(), server.arg("art").c_str(),
                                  server.arg("ergebnis") == "ok", &betrag);
  if (e != KassenErgebnis::OK) return sendeFehler(grund(e));
  sendeJson(String("{\"ok\":true,\"betrag\":") + betrag + "}");
}

// POST /api/test/einfahrtszeit?kz=..&minuten=..  (Testmodus, TF-09, TF-20)
void handleTestEinfahrtszeit() {
  zeitAusAnfrage();
  uint32_t minuten = strtoul(server.arg("minuten").c_str(), nullptr, 10);
  KassenErgebnis e = kasse.setzeEinfahrtszeit(server.arg("kz").c_str(), minuten);
  if (e != KassenErgebnis::OK) return sendeFehler(grund(e));
  sendeJson("{\"ok\":true}");
}

// ---------------------------------------------------------------------------
// Setup / Loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  antriebEin.begin();
  antriebAus.begin();
  for (auto& s : seiten) s.start();   // Schranke zu

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WLAN_SSID, WLAN_PASSWORT);

  server.on("/", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", SCHRANKEN_SEITE); });
  server.on("/kasse", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", KASSE_SEITE); });
  server.on("/api/status", handleStatus);
  // POST /api/kamera?seite=ein|aus&kz=..   (Kamera-Simulation, leer = nicht erkannt)
  server.on("/api/kamera", []() { schrankenAktion([](Schranke& s) { return s.kamera(server.arg("kz").c_str()); }); });
  // POST /api/hilfe?seite=..   (AK 6.2: Hilfe-Taste benachrichtigt das Personal)
  server.on("/api/hilfe", []() { schrankenAktion([](Schranke& s) { return s.hilfe(); }); });
  // POST /api/manuell?seite=..&kz=..   (Personal erfasst Kennzeichen, AK 6.2-6.4)
  server.on("/api/manuell", []() { schrankenAktion([](Schranke& s) { return s.manuell(server.arg("kz").c_str()); }); });
  // POST /api/hilfe_beenden?seite=..
  server.on("/api/hilfe_beenden", []() { schrankenAktion([](Schranke& s) { return s.hilfeBeenden(); }); });
  // POST /api/durchfahrt?seite=..   (AK 2.3: Schranke schliesst nach der Durchfahrt)
  server.on("/api/durchfahrt", []() { schrankenAktion([](Schranke& s) { return s.durchfahrt(); }); });
  server.on("/api/fahrzeuge", handleFahrzeuge);
  server.on("/api/zahlen", handleZahlen);
  server.on("/api/test/einfahrtszeit", handleTestEinfahrtszeit);
  server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();

  Serial.printf("Parkhaus bereit: WLAN \"%s\", http://%s\n", WLAN_SSID, WiFi.softAPIP().toString().c_str());
}

void loop() {
  server.handleClient();
  for (auto& s : seiten) s.tick();
}
