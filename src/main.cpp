// Parksystem - ein Raspberry Pi Pico W
// Steuert Ein- und Ausfahrtsschranke (Servos), simuliert die Kameras ueber eine
// Weboberflaeche, fuehrt die Fahrzeugliste, berechnet die Parkgebuehr und nimmt
// (simulierte) Zahlungen entgegen.
// Sven Troxler & Lukas Traut

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>

#include "config.h"
#include "gebuehr.h"
#include "kennzeichen.h"
#include "webseite.h"

WebServer server(80);

// ---------------------------------------------------------------------------
// Uhrzeit: Der Pico hat keine Echtzeituhr. Die Zeit wird vom Browser bei
// jeder Anfrage mitgeschickt (Parameter t = Unix-Zeit).
// ---------------------------------------------------------------------------
uint32_t zeitBasis = 0;   // Unix-Zeit bei millis() == 0
bool zeitGesetzt = false;

uint32_t jetzt() { return zeitBasis + millis() / 1000; }

void zeitAusAnfrage() {
  if (!server.hasArg("t")) return;
  uint32_t t = strtoul(server.arg("t").c_str(), nullptr, 10);
  if (t < 1700000000UL) return;
  uint32_t neu = t - millis() / 1000;
  if (!zeitGesetzt || (neu > zeitBasis ? neu - zeitBasis : zeitBasis - neu) > 3) {
    zeitBasis = neu;
    zeitGesetzt = true;
  }
}

// ---------------------------------------------------------------------------
// Fahrzeugliste (ein Eintrag pro Parkplatz, leeres Kennzeichen = frei)
// ---------------------------------------------------------------------------
struct Fahrzeug {
  char kz[KZ_LAENGE];
  uint32_t einfahrt;       // Unix-Zeit (Sekunden)
  uint32_t betrag;         // bezahlter Betrag in Rappen
  bool manuell;            // AK 6.3: "manuell erfasst"
  bool bezahlt;
};

Fahrzeug fahrzeuge[KAPAZITAET];

int freiePlaetze() {
  int n = 0;
  for (auto& f : fahrzeuge) if (f.kz[0] == '\0') n++;
  return n;
}

Fahrzeug* finde(const char* kz) {
  for (auto& f : fahrzeuge) if (f.kz[0] != '\0' && strcmp(f.kz, kz) == 0) return &f;
  return nullptr;
}

Fahrzeug* freierPlatz() {
  for (auto& f : fahrzeuge) if (f.kz[0] == '\0') return &f;
  return nullptr;
}

uint32_t gebuehr(const Fahrzeug& f) {
  if (f.bezahlt) return f.betrag;
  uint32_t t = jetzt();
  return berechneGebuehrRappen(t > f.einfahrt ? t - f.einfahrt : 0);
}

// ---------------------------------------------------------------------------
// Schranken und Display
// ---------------------------------------------------------------------------
enum Zustand { BEREIT, NICHT_ERKANNT, HILFE, OFFEN, GESPERRT };
static const char* ZUSTAENDE[] = {"bereit", "nicht_erkannt", "hilfe", "offen", "gesperrt"};

struct Seite {
  const char* id;
  bool einfahrt;
  int pinServo;
  Servo servo;
  Zustand zustand;
  String meldung;
  String typ;                 // "info", "ok", "fehler"
  String kz;                  // letztes Kennzeichen
  unsigned long seit;         // millis() beim letzten Zustandswechsel
};

Seite seiten[2] = {
  {"ein", true,  PIN_SERVO_EIN},
  {"aus", false, PIN_SERVO_AUS},
};

// Gesperrt-Meldungen (z.B. "nicht bezahlt") verschwinden nach dieser Zeit
static const unsigned long GESPERRT_ANZEIGE_MS = 15000;

void melde(Seite& s, Zustand z, const String& text, const char* typ) {
  s.zustand = z;
  s.meldung = text;
  s.typ = typ;
  s.seit = millis();
  s.servo.write(z == OFFEN ? SERVO_WINKEL_OFFEN : SERVO_WINKEL_ZU);
  Serial.printf("[%s] %s: %s\n", s.id, ZUSTAENDE[z], text.c_str());
}

// Meldung im Ruhezustand (AK 5.1: freie Plaetze bei der Einfahrt)
String standardMeldung(const Seite& s) {
  if (!s.einfahrt) return "Bitte vorfahren";
  if (freiePlaetze() == 0) return "BESETZT – Parkhaus voll";
  return "Willkommen – bitte vorfahren";
}

String chf(long rappen) {
  char buf[16];
  snprintf(buf, sizeof(buf), "CHF %ld.%02ld", rappen / 100, rappen % 100);
  return buf;
}

// Kennzeichen von der Kamera (manuell=false) oder vom Personal (manuell=true)
// verarbeiten. Gibt false zurueck, wenn das Kennzeichen ungueltig ist.
bool verarbeiteKennzeichen(Seite& s, const String& eingabe, bool manuell) {
  char kz[KZ_LAENGE];
  if (!normalisiereKennzeichen(eingabe.c_str(), kz, sizeof(kz))) {
    // AK 2.2 / AK 6.1: Schranke bleibt zu, Hinweis auf die Hilfe-Taste
    if (!manuell) melde(s, NICHT_ERKANNT, "Kennzeichen nicht erkannt – bitte Hilfe-Taste drücken", "fehler");
    return false;
  }
  s.kz = kz;
  String k = kz;
  Fahrzeug* f = finde(kz);

  if (s.einfahrt) {
    if (f) {                                                          // TF-08
      melde(s, GESPERRT, k + " ist bereits im Parkhaus – bitte Hilfe-Taste drücken", "fehler");
    } else if (freiePlaetze() == 0) {                                 // AK 5.3
      melde(s, GESPERRT, "BESETZT – Parkhaus voll", "fehler");
    } else {                                                          // AK 1.2, 2.1
      f = freierPlatz();
      memset(f, 0, sizeof(*f));
      strncpy(f->kz, kz, KZ_LAENGE - 1);
      f->einfahrt = jetzt();
      f->manuell = manuell;
      melde(s, OFFEN, k + " – bitte einfahren", "ok");
    }
  } else {
    if (!f) {                                                         // TF-15
      melde(s, GESPERRT, "Keine Zahlung für " + k + " vorhanden – bitte Hilfe-Taste drücken", "fehler");
    } else if (!f->bezahlt) {                                         // AK 4.3
      melde(s, GESPERRT, "Parkgebühr für " + k + " nicht bezahlt (" + chf(gebuehr(*f)) +
            ") – bitte an der Kasse bezahlen", "fehler");
    } else {                                                          // AK 4.2
      f->kz[0] = '\0';
      melde(s, OFFEN, k + " – gute Fahrt!", "ok");
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Web-Schnittstelle Schranken
// ---------------------------------------------------------------------------
void sendeJson(const String& json) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void antworte(bool ok, const String& text) {
  String t = text;
  t.replace("\"", "'");
  sendeJson(String("{\"ok\":") + (ok ? "true" : "false") + ",\"text\":\"" + t + "\"}");
}

Seite* seiteAusAnfrage() {
  String id = server.arg("seite");
  for (auto& s : seiten) if (id == s.id) return &s;
  return nullptr;
}

String seiteJson(const Seite& s) {
  String meldung = s.zustand == BEREIT ? standardMeldung(s) : s.meldung;
  String typ = s.zustand == BEREIT ? "info" : s.typ;
  meldung.replace("\"", "'");
  String j = "{\"zustand\":\"";
  j += ZUSTAENDE[s.zustand];
  j += "\",\"meldung\":\"";  j += meldung;
  j += "\",\"typ\":\"";      j += typ;
  j += "\",\"kz\":\"";       j += s.kz;
  j += "\",\"offen\":";      j += s.zustand == OFFEN ? "true" : "false";
  j += ",\"hilfe\":";        j += s.zustand == HILFE ? "true" : "false";
  j += "}";
  return j;
}

// GET /api/status
void handleStatus() {
  zeitAusAnfrage();
  String j = "{\"frei\":";
  j += freiePlaetze();
  j += ",\"kapazitaet\":";  j += KAPAZITAET;
  j += ",\"ein\":";         j += seiteJson(seiten[0]);
  j += ",\"aus\":";         j += seiteJson(seiten[1]);
  j += "}";
  sendeJson(j);
}

// POST /api/kamera?seite=ein|aus&kz=..   (Kamera-Simulation, leer = nicht erkannt)
void handleKamera() {
  zeitAusAnfrage();
  Seite* s = seiteAusAnfrage();
  if (!s) return antworte(false, "Seite fehlt");
  if (s->zustand == OFFEN) return antworte(false, "Schranke ist offen – zuerst durchfahren");
  verarbeiteKennzeichen(*s, server.arg("kz"), false);
  antworte(s->zustand == OFFEN, s->meldung);
}

// POST /api/hilfe?seite=..   (AK 6.2: Hilfe-Taste benachrichtigt das Personal)
void handleHilfe() {
  Seite* s = seiteAusAnfrage();
  if (!s) return antworte(false, "Seite fehlt");
  if (s->zustand != OFFEN && s->zustand != HILFE)
    melde(*s, HILFE, "Personal wurde benachrichtigt – bitte warten", "info");
  antworte(s->zustand == HILFE, s->meldung);
}

// POST /api/manuell?seite=..&kz=..   (Personal erfasst Kennzeichen, AK 6.2-6.4)
void handleManuell() {
  zeitAusAnfrage();
  Seite* s = seiteAusAnfrage();
  if (!s) return antworte(false, "Seite fehlt");
  if (s->zustand == OFFEN) return antworte(false, "Schranke ist bereits offen");
  if (!verarbeiteKennzeichen(*s, server.arg("kz"), true))
    return antworte(false, "Ungültiges Kennzeichen – Format z.B. ZH 123456");
  antworte(s->zustand == OFFEN, s->meldung);
}

// POST /api/hilfe_beenden?seite=..
void handleHilfeBeenden() {
  Seite* s = seiteAusAnfrage();
  if (!s) return antworte(false, "Seite fehlt");
  if (s->zustand != OFFEN) melde(*s, BEREIT, "", "info");
  antworte(true, "");
}

// POST /api/durchfahrt?seite=..   (AK 2.3: Schranke schliesst nach der Durchfahrt)
void handleDurchfahrt() {
  Seite* s = seiteAusAnfrage();
  if (!s) return antworte(false, "Seite fehlt");
  bool warOffen = s->zustand == OFFEN;
  if (warOffen) melde(*s, BEREIT, "", "info");
  antworte(warOffen, warOffen ? "Schranke geschlossen" : "Schranke ist nicht offen");
}

// ---------------------------------------------------------------------------
// Web-Schnittstelle Kasse
// ---------------------------------------------------------------------------
void sendeFehler(const char* grund) {
  sendeJson(String("{\"ok\":false,\"grund\":\"") + grund + "\"}");
}

// GET /api/fahrzeuge
void handleFahrzeuge() {
  zeitAusAnfrage();
  String s = "{\"jetzt\":";
  s += jetzt();
  s += ",\"kapazitaet\":";   s += KAPAZITAET;
  s += ",\"frei\":";         s += freiePlaetze();
  s += ",\"fahrzeuge\":[";
  bool erstes = true;
  for (auto& f : fahrzeuge) {
    if (f.kz[0] == '\0') continue;
    if (!erstes) s += ",";
    s += "{\"kz\":\"";        s += f.kz;
    s += "\",\"ein\":";       s += f.einfahrt;
    s += ",\"manuell\":";     s += f.manuell ? "true" : "false";
    s += ",\"bezahlt\":";     s += f.bezahlt ? "true" : "false";
    s += ",\"gebuehr\":";     s += gebuehr(f);
    s += "}";
    erstes = false;
  }
  s += "]}";
  sendeJson(s);
}

// POST /api/zahlen?kz=..&art=kreditkarte|twint&ergebnis=ok|fehler  (US3)
void handleZahlen() {
  zeitAusAnfrage();
  char kz[KZ_LAENGE];
  if (!normalisiereKennzeichen(server.arg("kz").c_str(), kz, KZ_LAENGE)) return sendeFehler("ungueltig");
  Fahrzeug* f = finde(kz);
  if (!f) return sendeFehler("unbekannt");
  if (f->bezahlt) return sendeFehler("bereits_bezahlt");

  String art = server.arg("art");
  if (art != "kreditkarte" && art != "twint") return sendeFehler("zahlart");

  // Simulierte Zahlung "fehlgeschlagen": Status bleibt "nicht bezahlt" (TF-12)
  if (server.arg("ergebnis") != "ok") return sendeFehler("zahlung_fehlgeschlagen");

  f->betrag = gebuehr(*f);
  f->bezahlt = true;                                                // AK 3.3

  Serial.printf("Bezahlt %s: %lu Rappen (%s)\n", kz, (unsigned long)f->betrag, art.c_str());
  sendeJson(String("{\"ok\":true,\"betrag\":") + f->betrag + "}");
}

// POST /api/test/einfahrtszeit?kz=..&minuten=..  (Testmodus, TF-09, TF-20)
void handleTestEinfahrtszeit() {
  zeitAusAnfrage();
  char kz[KZ_LAENGE];
  if (!normalisiereKennzeichen(server.arg("kz").c_str(), kz, KZ_LAENGE)) return sendeFehler("ungueltig");
  Fahrzeug* f = finde(kz);
  if (!f) return sendeFehler("unbekannt");
  if (f->bezahlt) return sendeFehler("bereits_bezahlt");

  uint32_t minuten = strtoul(server.arg("minuten").c_str(), nullptr, 10);
  f->einfahrt = jetzt() - minuten * 60;
  sendeJson("{\"ok\":true}");
}

// ---------------------------------------------------------------------------
// Setup / Loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  for (auto& s : seiten) {
    s.servo.attach(s.pinServo);
    melde(s, BEREIT, "", "info");   // Schranke zu
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WLAN_SSID, WLAN_PASSWORT);

  server.on("/", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", SCHRANKEN_SEITE); });
  server.on("/kasse", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", KASSE_SEITE); });
  server.on("/api/status", handleStatus);
  server.on("/api/kamera", handleKamera);
  server.on("/api/hilfe", handleHilfe);
  server.on("/api/manuell", handleManuell);
  server.on("/api/hilfe_beenden", handleHilfeBeenden);
  server.on("/api/durchfahrt", handleDurchfahrt);
  server.on("/api/fahrzeuge", handleFahrzeuge);
  server.on("/api/zahlen", handleZahlen);
  server.on("/api/test/einfahrtszeit", handleTestEinfahrtszeit);
  server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();

  Serial.printf("Parkhaus bereit: WLAN \"%s\", http://%s\n", WLAN_SSID, WiFi.softAPIP().toString().c_str());
}

void loop() {
  server.handleClient();

  for (auto& s : seiten)
    if (s.zustand == GESPERRT && millis() - s.seit > GESPERRT_ANZEIGE_MS) melde(s, BEREIT, "", "info");
}
