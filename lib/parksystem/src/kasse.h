#pragma once
// Kasse: Fahrzeugliste, Parkgebuehr und (simulierte) Zahlung (US1, US3, US5).
// Haengt nur von IUhr und IProtokoll ab und ist damit ohne Hardware testbar.

#include <stdint.h>
#include "schnittstellen.h"
#include "parameter.h"
#include "kennzeichen.h"

struct Fahrzeug {
  char kz[KZ_LAENGE];      // leer = Platz frei
  uint32_t einfahrt;       // Unix-Zeit (Sekunden)
  uint32_t betrag;         // bezahlter Betrag in Rappen
  bool manuell;            // AK 6.3: "manuell erfasst"
  bool bezahlt;
};

// Ergebnis einer Kassen-Aktion. Die Texte (grund()) sind Teil der Web-Schnittstelle.
enum class KassenErgebnis { OK, UNGUELTIG, UNBEKANNT, BEREITS_BEZAHLT, ZAHLART, ZAHLUNG_FEHLGESCHLAGEN };
const char* grund(KassenErgebnis e);

class Kasse : public IKasse {
 public:
  Kasse(IUhr& uhr, IProtokoll& protokoll);

  // IKasse (wird von den Schranken verwendet)
  int freiePlaetze() override;
  Einfahrt einfahren(const char* kz, bool manuell) override;
  Bezahlstatus bezahlstatus(const char* kz, uint32_t* gebuehrRappen) override;
  bool ausfahren(const char* kz) override;

  // Kasse (Weboberflaeche /kasse)
  // art: "kreditkarte" oder "twint", erfolgreich: Ergebnis des (simulierten) Terminals
  KassenErgebnis zahlen(const char* eingabeKz, const char* art, bool erfolgreich, uint32_t* betrag);
  // Testmodus (TF-09, TF-20): Einfahrtszeit auf "jetzt - minuten" setzen
  KassenErgebnis setzeEinfahrtszeit(const char* eingabeKz, uint32_t minutenZurueck);

  uint32_t gebuehr(const Fahrzeug& f);
  const Fahrzeug* finde(const char* kz) const;
  const Fahrzeug& platz(int i) const { return plaetze_[i]; }   // 0 .. KAPAZITAET-1
  uint32_t jetzt() { return uhr_.unixZeit(); }

 private:
  Fahrzeug* findeVeraenderbar(const char* kz);

  IUhr& uhr_;
  IProtokoll& protokoll_;
  Fahrzeug plaetze_[KAPAZITAET];
};
