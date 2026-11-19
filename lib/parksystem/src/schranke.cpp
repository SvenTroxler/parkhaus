#include "schranke.h"

#include <stdio.h>
#include <string.h>
#include "parameter.h"

static const char* const ZUSTAENDE[] = {"bereit", "nicht_erkannt", "hilfe", "offen", "gesperrt"};

const char* zustandText(Zustand z) { return ZUSTAENDE[static_cast<int>(z)]; }

Schranke::Schranke(const char* id, bool einfahrt, ISchrankenAntrieb& antrieb, IKasse& kasse,
                   IUhr& uhr, IProtokoll& protokoll)
    : id_(id), einfahrt_(einfahrt), antrieb_(antrieb), kasse_(kasse), uhr_(uhr), protokoll_(protokoll) {}

void Schranke::start() { melde(Zustand::BEREIT, "", "info"); }

void Schranke::melde(Zustand z, const char* text, const char* typ) {
  zustand_ = z;
  if (text != meldung_) snprintf(meldung_, sizeof(meldung_), "%s", text);
  typ_ = typ;
  seit_ = uhr_.laufzeitMs();
  if (z == Zustand::OFFEN) antrieb_.oeffnen(); else antrieb_.schliessen();

  char log[200];
  snprintf(log, sizeof(log), "[%s] %s: %s", id_, zustandText(z), meldung_);
  protokoll_.schreibe(log);
}

const char* Schranke::meldung() {
  if (zustand_ != Zustand::BEREIT) return meldung_;
  if (!einfahrt_) return "Bitte vorfahren";
  int frei = kasse_.freiePlaetze();
  if (frei < 0) return "Störung – bitte Hilfe-Taste drücken";
  if (frei == 0) return "BESETZT – Parkhaus voll";                           // AK 5.3
  return "Willkommen – bitte vorfahren";
}

const char* Schranke::typ() const { return zustand_ == Zustand::BEREIT ? "info" : typ_; }

static void chf(char* buf, size_t n, uint32_t rappen) {
  snprintf(buf, n, "CHF %lu.%02lu", (unsigned long)(rappen / 100), (unsigned long)(rappen % 100));
}

// Kennzeichen von der Kamera (manuell=false) oder vom Personal (manuell=true)
// verarbeiten. Gibt false zurueck, wenn das Kennzeichen ungueltig ist.
bool Schranke::verarbeite(const char* eingabe, bool manuell) {
  char kz[KZ_LAENGE];
  if (!normalisiereKennzeichen(eingabe, kz, sizeof(kz))) {
    // AK 2.2 / AK 6.1: Schranke bleibt zu, Hinweis auf die Hilfe-Taste
    if (!manuell) melde(Zustand::NICHT_ERKANNT, "Kennzeichen nicht erkannt – bitte Hilfe-Taste drücken", "fehler");
    return false;
  }
  strncpy(kz_, kz, sizeof(kz_) - 1);
  char text[sizeof(meldung_)];

  if (einfahrt_) {
    switch (kasse_.einfahren(kz, manuell)) {
      case Einfahrt::OK:                                                     // AK 1.2, 2.1
        snprintf(text, sizeof(text), "%s – bitte einfahren", kz);
        melde(Zustand::OFFEN, text, "ok");
        break;
      case Einfahrt::BEREITS_IM_PARKHAUS:                                    // TF-08
        snprintf(text, sizeof(text), "%s ist bereits im Parkhaus – bitte Hilfe-Taste drücken", kz);
        melde(Zustand::GESPERRT, text, "fehler");
        break;
      case Einfahrt::VOLL:                                                   // AK 5.3
        melde(Zustand::GESPERRT, "BESETZT – Parkhaus voll", "fehler");
        break;
      case Einfahrt::NICHT_ERREICHBAR:                                       // sicherer Zustand
        melde(Zustand::GESPERRT, "Störung: Kasse nicht erreichbar – bitte Hilfe-Taste drücken", "fehler");
        break;
    }
    return true;
  }

  uint32_t gebuehr = 0;
  switch (kasse_.bezahlstatus(kz, &gebuehr)) {                               // AK 4.1
    case Bezahlstatus::UNBEKANNT:                                            // TF-15
      snprintf(text, sizeof(text), "Keine Zahlung für %s vorhanden – bitte Hilfe-Taste drücken", kz);
      melde(Zustand::GESPERRT, text, "fehler");
      break;
    case Bezahlstatus::NICHT_BEZAHLT: {                                      // AK 4.3
      char betrag[24];
      chf(betrag, sizeof(betrag), gebuehr);
      snprintf(text, sizeof(text), "Parkgebühr für %s nicht bezahlt (%s) – bitte an der Kasse bezahlen", kz, betrag);
      melde(Zustand::GESPERRT, text, "fehler");
      break;
    }
    case Bezahlstatus::BEZAHLT:                                              // AK 4.2
      kasse_.ausfahren(kz);
      snprintf(text, sizeof(text), "%s – gute Fahrt!", kz);
      melde(Zustand::OFFEN, text, "ok");
      break;
    case Bezahlstatus::NICHT_ERREICHBAR:                                     // TF-16
      melde(Zustand::GESPERRT, "Störung: Kasse nicht erreichbar – bitte Hilfe-Taste drücken", "fehler");
      break;
  }
  return true;
}

Antwort Schranke::kamera(const char* eingabe) {
  if (zustand_ == Zustand::OFFEN) return {false, "Schranke ist offen – zuerst durchfahren"};
  verarbeite(eingabe, false);
  return {zustand_ == Zustand::OFFEN, meldung_};
}

Antwort Schranke::hilfe() {
  if (zustand_ != Zustand::OFFEN && zustand_ != Zustand::HILFE)
    melde(Zustand::HILFE, "Personal wurde benachrichtigt – bitte warten", "info");
  return {zustand_ == Zustand::HILFE, meldung_};
}

Antwort Schranke::manuell(const char* eingabe) {
  if (zustand_ == Zustand::OFFEN) return {false, "Schranke ist bereits offen"};
  if (!verarbeite(eingabe, true)) return {false, "Ungültiges Kennzeichen – Format z.B. ZH 123456"};
  return {zustand_ == Zustand::OFFEN, meldung_};
}

Antwort Schranke::hilfeBeenden() {
  if (zustand_ != Zustand::OFFEN) melde(Zustand::BEREIT, "", "info");
  return {true, ""};
}

Antwort Schranke::durchfahrt() {
  bool warOffen = zustand_ == Zustand::OFFEN;
  if (warOffen) melde(Zustand::BEREIT, "", "info");
  return {warOffen, warOffen ? "Schranke geschlossen" : "Schranke ist nicht offen"};
}

void Schranke::tick() {
  if (zustand_ == Zustand::GESPERRT && uhr_.laufzeitMs() - seit_ > GESPERRT_ANZEIGE_MS)
    melde(Zustand::BEREIT, "", "info");
}
