#pragma once
// Steuerung einer Schranke (Einfahrt oder Ausfahrt) inkl. Display-Meldung (US2, US4, US5, US6).
// Alle Abhaengigkeiten (Antrieb, Kasse, Uhr, Protokoll) werden im Konstruktor uebergeben.

#include <stdint.h>
#include "schnittstellen.h"
#include "kennzeichen.h"

// Display-Meldungen gemaess Testkonzept V1.1 (erwartete Ergebnisse im Testprotokoll)
#define MELDUNG_NICHT_ERKANNT "Kennzeichen nicht erkannt – bitte Hilfe-Taste drücken"  // TF-05
#define MELDUNG_NICHT_BEZAHLT "Bitte zuerst an der Kasse bezahlen"                     // TF-07a, TF-14
#define MELDUNG_UNBEKANNT     "Fahrzeug unbekannt – bitte Hilfe-Taste drücken"         // TF-15
#define MELDUNG_STOERUNG      "Störung – Kasse nicht erreichbar"                       // TF-16
#define MELDUNG_BESETZT       "BESETZT – Parkhaus voll"                                // TF-19

enum class Zustand { BEREIT, NICHT_ERKANNT, HILFE, OFFEN, GESPERRT };
const char* zustandText(Zustand z);   // "bereit", "nicht_erkannt", ... (Web-Schnittstelle)

// Antwort auf eine Aktion an der Schranke
struct Antwort {
  bool ok;
  const char* text;   // gueltig bis zur naechsten Aktion an derselben Schranke
};

class Schranke {
 public:
  Schranke(const char* id, bool einfahrt, ISchrankenAntrieb& antrieb, IKasse& kasse,
           IUhr& uhr, IProtokoll& protokoll);

  void start();                         // Grundstellung: zu, bereit

  // Aktionen (produktiv aus der Weboberflaeche)
  Antwort kamera(const char* eingabe);  // Kamera-Simulation, "" = nicht erkannt
  Antwort hilfe();                      // AK 6.2: Hilfe-Taste
  Antwort manuell(const char* eingabe); // AK 6.2-6.4: Personal erfasst Kennzeichen
  Antwort hilfeBeenden();
  Antwort durchfahrt();                 // AK 2.3: Auto ist durchgefahren
  void tick();                          // regelmaessig aufrufen (loop)

  // Zustand fuer Display / Weboberflaeche
  const char* id() const { return id_; }
  bool istEinfahrt() const { return einfahrt_; }
  Zustand zustand() const { return zustand_; }
  bool offen() const { return zustand_ == Zustand::OFFEN; }
  const char* meldung();                // im Zustand BEREIT die Standardmeldung (AK 5.1)
  const char* typ() const;              // "info", "ok", "fehler"
  const char* kz() const { return kz_; }

 private:
  void melde(Zustand z, const char* text, const char* typ);
  bool verarbeite(const char* eingabe, bool manuell);

  const char* id_;
  bool einfahrt_;
  ISchrankenAntrieb& antrieb_;
  IKasse& kasse_;
  IUhr& uhr_;
  IProtokoll& protokoll_;

  Zustand zustand_ = Zustand::BEREIT;
  char meldung_[160] = {0};
  const char* typ_ = "info";
  char kz_[KZ_LAENGE] = {0};            // letztes Kennzeichen
  uint32_t seit_ = 0;                   // laufzeitMs() beim letzten Zustandswechsel
};
