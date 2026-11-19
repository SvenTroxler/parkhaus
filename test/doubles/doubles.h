#pragma once
// Test Doubles fuer die Schnittstellen aus lib/parksystem/src/schnittstellen.h.
// Werden von allen Testprogrammen in test/test_*/ verwendet.

#include <stdint.h>
#include <string.h>
#include "schnittstellen.h"

// ---------------------------------------------------------------------------
// Dummy: Platzhalter, fuer den Testfall irrelevant.
// Das Protokoll (serieller Monitor) hat keinen Einfluss auf das Verhalten.
// ---------------------------------------------------------------------------
class DummyProtokoll : public IProtokoll {
 public:
  void schreibe(const char*) override {}
};

// ---------------------------------------------------------------------------
// Fake: vereinfachte, aber funktionierende Implementierung.
// Eine Uhr, die nur weiterlaeuft, wenn der Test sie vorspult. Damit lassen sich
// Parkdauern von Stunden/Tagen in Mikrosekunden und deterministisch testen.
// ---------------------------------------------------------------------------
class FakeUhr : public IUhr {
 public:
  explicit FakeUhr(uint32_t startUnixZeit = 1767225600UL /* 1.1.2026 00:00 UTC */)
      : start_(startUnixZeit) {}

  uint32_t unixZeit() override { return start_ + ms_ / 1000; }
  uint32_t laufzeitMs() override { return (uint32_t)ms_; }

  void vorspulenMs(uint64_t ms) { ms_ += ms; }
  void vorspulenSek(uint64_t s) { ms_ += s * 1000; }
  void vorspulenMin(uint64_t m) { ms_ += m * 60000; }

 private:
  uint32_t start_;
  uint64_t ms_ = 0;
};

// ---------------------------------------------------------------------------
// Mock: Stub mit "Erinnerungsvermoegen".
// Merkt sich, wie oft die Schranke geoeffnet/geschlossen wurde und in welcher
// Stellung sie zuletzt ist - ersetzt das Beobachten des Servos.
// ---------------------------------------------------------------------------
class MockAntrieb : public ISchrankenAntrieb {
 public:
  void oeffnen() override { anzahlOeffnen++; istOffen = true; }
  void schliessen() override { anzahlSchliessen++; istOffen = false; }
  void zuruecksetzen() { anzahlOeffnen = anzahlSchliessen = 0; }

  int anzahlOeffnen = 0;
  int anzahlSchliessen = 0;
  bool istOffen = false;
};

// ---------------------------------------------------------------------------
// Stub: hart kodierte Antworten, keine Logik, ignoriert die Parameter.
// Damit lassen sich Zustaende der Kasse erzwingen, die mit der echten Kasse
// schwierig oder gar nicht herstellbar sind (z.B. "nicht erreichbar", TF-16).
// ---------------------------------------------------------------------------
class StubKasse : public IKasse {
 public:
  int freiePlaetze() override { return frei; }
  Einfahrt einfahren(const char*, bool) override { return einfahrt; }
  Bezahlstatus bezahlstatus(const char*, uint32_t* g) override {
    if (g) *g = gebuehr;
    return status;
  }
  bool ausfahren(const char*) override { return true; }

  // Vorgegebene Antworten (im Test vor dem Aufruf setzen)
  int frei = 3;
  Einfahrt einfahrt = Einfahrt::OK;
  Bezahlstatus status = Bezahlstatus::BEZAHLT;
  uint32_t gebuehr = 0;
};

// ---------------------------------------------------------------------------
// Mock: wie der Stub, merkt sich aber zusaetzlich alle Aufrufe und Parameter.
// So laesst sich pruefen, OB und WIE die Schranke die Kasse benutzt
// (z.B. "bei ungueltigem Kennzeichen wird nichts gespeichert").
// ---------------------------------------------------------------------------
class MockKasse : public StubKasse {
 public:
  Einfahrt einfahren(const char* kz, bool manuell) override {
    anzahlEinfahren++;
    kopiere(letztesKz, kz);
    letztesManuell = manuell;
    return StubKasse::einfahren(kz, manuell);
  }
  Bezahlstatus bezahlstatus(const char* kz, uint32_t* g) override {
    anzahlBezahlstatus++;
    kopiere(letztesKz, kz);
    return StubKasse::bezahlstatus(kz, g);
  }
  bool ausfahren(const char* kz) override {
    anzahlAusfahren++;
    kopiere(letztesKz, kz);
    return true;
  }

  int anzahlEinfahren = 0;
  int anzahlBezahlstatus = 0;
  int anzahlAusfahren = 0;
  char letztesKz[16] = {0};
  bool letztesManuell = false;

 private:
  static void kopiere(char* ziel, const char* quelle) {
    strncpy(ziel, quelle ? quelle : "", 15);
    ziel[15] = '\0';
  }
};

// ---------------------------------------------------------------------------
// Spy: umschliesst die ECHTE Kasse und zeichnet die Aufrufe auf.
// Der Test bleibt ein Integrationstest (echte Fahrzeugliste, echte Gebuehr),
// zusaetzlich kann geprueft werden, dass die Schranke die Kasse gefragt hat.
// ---------------------------------------------------------------------------
class SpyKasse : public IKasse {
 public:
  explicit SpyKasse(IKasse& echt) : echt_(echt) {}

  int freiePlaetze() override { return echt_.freiePlaetze(); }
  Einfahrt einfahren(const char* kz, bool manuell) override {
    anzahlEinfahren++;
    letztesManuell = manuell;
    return echt_.einfahren(kz, manuell);
  }
  Bezahlstatus bezahlstatus(const char* kz, uint32_t* g) override {
    anzahlBezahlstatus++;
    letzterStatus = echt_.bezahlstatus(kz, g);
    return letzterStatus;
  }
  bool ausfahren(const char* kz) override {
    anzahlAusfahren++;
    return echt_.ausfahren(kz);
  }

  int anzahlEinfahren = 0;
  int anzahlBezahlstatus = 0;
  int anzahlAusfahren = 0;
  bool letztesManuell = false;
  Bezahlstatus letzterStatus = Bezahlstatus::UNBEKANNT;

 private:
  IKasse& echt_;
};
