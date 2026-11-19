#include "kasse.h"

#include <stdio.h>
#include <string.h>
#include "gebuehr.h"

const char* grund(KassenErgebnis e) {
  switch (e) {
    case KassenErgebnis::OK:                     return "ok";
    case KassenErgebnis::UNGUELTIG:              return "ungueltig";
    case KassenErgebnis::UNBEKANNT:              return "unbekannt";
    case KassenErgebnis::BEREITS_BEZAHLT:        return "bereits_bezahlt";
    case KassenErgebnis::ZAHLART:                return "zahlart";
    case KassenErgebnis::ZAHLUNG_FEHLGESCHLAGEN: return "zahlung_fehlgeschlagen";
  }
  return "?";
}

Kasse::Kasse(IUhr& uhr, IProtokoll& protokoll) : uhr_(uhr), protokoll_(protokoll) {
  memset(plaetze_, 0, sizeof(plaetze_));
}

int Kasse::freiePlaetze() {
  int n = 0;
  for (auto& f : plaetze_) if (f.kz[0] == '\0') n++;
  return n;
}

const Fahrzeug* Kasse::finde(const char* kz) const {
  for (auto& f : plaetze_) if (f.kz[0] != '\0' && strcmp(f.kz, kz) == 0) return &f;
  return nullptr;
}

Fahrzeug* Kasse::findeVeraenderbar(const char* kz) {
  return const_cast<Fahrzeug*>(finde(kz));
}

uint32_t Kasse::gebuehr(const Fahrzeug& f) {
  if (f.bezahlt) return f.betrag;
  uint32_t t = uhr_.unixZeit();
  return berechneGebuehrRappen(t > f.einfahrt ? t - f.einfahrt : 0);
}

Einfahrt Kasse::einfahren(const char* kz, bool manuell) {
  if (finde(kz)) return Einfahrt::BEREITS_IM_PARKHAUS;          // TF-08
  for (auto& f : plaetze_) {
    if (f.kz[0] != '\0') continue;
    memset(&f, 0, sizeof(f));                                    // AK 1.2
    strncpy(f.kz, kz, KZ_LAENGE - 1);
    f.einfahrt = uhr_.unixZeit();
    f.manuell = manuell;
    return Einfahrt::OK;
  }
  return Einfahrt::VOLL;                                         // AK 5.3
}

Bezahlstatus Kasse::bezahlstatus(const char* kz, uint32_t* gebuehrRappen) {
  Fahrzeug* f = findeVeraenderbar(kz);
  if (!f) return Bezahlstatus::UNBEKANNT;                        // TF-15
  if (gebuehrRappen) *gebuehrRappen = gebuehr(*f);
  return f->bezahlt ? Bezahlstatus::BEZAHLT : Bezahlstatus::NICHT_BEZAHLT;
}

bool Kasse::ausfahren(const char* kz) {
  Fahrzeug* f = findeVeraenderbar(kz);
  if (!f) return false;
  f->kz[0] = '\0';
  return true;
}

KassenErgebnis Kasse::zahlen(const char* eingabeKz, const char* art, bool erfolgreich, uint32_t* betrag) {
  char kz[KZ_LAENGE];
  if (!normalisiereKennzeichen(eingabeKz, kz, sizeof(kz))) return KassenErgebnis::UNGUELTIG;
  Fahrzeug* f = findeVeraenderbar(kz);
  if (!f) return KassenErgebnis::UNBEKANNT;
  if (f->bezahlt) return KassenErgebnis::BEREITS_BEZAHLT;
  if (!art || (strcmp(art, "kreditkarte") != 0 && strcmp(art, "twint") != 0))
    return KassenErgebnis::ZAHLART;                              // AK 3.2

  // Simulierte Zahlung "fehlgeschlagen": Status bleibt "nicht bezahlt" (TF-12)
  if (!erfolgreich) return KassenErgebnis::ZAHLUNG_FEHLGESCHLAGEN;

  f->betrag = gebuehr(*f);
  f->bezahlt = true;                                             // AK 3.3
  if (betrag) *betrag = f->betrag;

  char text[64];
  snprintf(text, sizeof(text), "Bezahlt %s: %lu Rappen (%s)", kz, (unsigned long)f->betrag, art);
  protokoll_.schreibe(text);
  return KassenErgebnis::OK;
}

KassenErgebnis Kasse::setzeEinfahrtszeit(const char* eingabeKz, uint32_t minutenZurueck) {
  char kz[KZ_LAENGE];
  if (!normalisiereKennzeichen(eingabeKz, kz, sizeof(kz))) return KassenErgebnis::UNGUELTIG;
  Fahrzeug* f = findeVeraenderbar(kz);
  if (!f) return KassenErgebnis::UNBEKANNT;
  if (f->bezahlt) return KassenErgebnis::BEREITS_BEZAHLT;
  f->einfahrt = uhr_.unixZeit() - minutenZurueck * 60;
  return KassenErgebnis::OK;
}
