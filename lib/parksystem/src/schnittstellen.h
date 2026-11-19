#pragma once
// Schnittstellen zwischen der Logik des Parksystems und ihrer Umgebung.
//
// Die Logik (Kasse, Schranke) ruft keine Hardware-Funktionen direkt auf.
// Sie bekommt alle Abhaengigkeiten ueber den Konstruktor uebergeben
// (Dependency Injection):
//
//   Produktiv (src/)          Schnittstelle        Test (test/doubles/)
//   ServoAntrieb       ---->  ISchrankenAntrieb <-- MockAntrieb
//   PicoUhr            ---->  IUhr              <-- FakeUhr
//   SerialProtokoll    ---->  IProtokoll        <-- DummyProtokoll
//   Kasse (lib)        ---->  IKasse            <-- StubKasse, MockKasse, SpyKasse

#include <stdint.h>

// Antrieb einer Schranke (produktiv: Servo SG90 an einem GPIO).
class ISchrankenAntrieb {
 public:
  virtual ~ISchrankenAntrieb() {}
  virtual void oeffnen() = 0;
  virtual void schliessen() = 0;
};

// Zeitquelle. Der Pico hat keine Echtzeituhr: produktiv kommt die Uhrzeit vom
// Browser, die Laufzeit von millis().
class IUhr {
 public:
  virtual ~IUhr() {}
  virtual uint32_t unixZeit() = 0;      // Sekunden seit 1.1.1970 (Ein-/Ausfahrt, Gebuehr)
  virtual uint32_t laufzeitMs() = 0;    // Millisekunden seit Start (Anzeigedauer)
};

// Protokoll / Log (produktiv: serieller Monitor).
class IProtokoll {
 public:
  virtual ~IProtokoll() {}
  virtual void schreibe(const char* text) = 0;
};

// Ergebnis einer Einfahrt bei der Kasse
enum class Einfahrt { OK, BEREITS_IM_PARKHAUS, VOLL, NICHT_ERREICHBAR };

// Antwort der Kasse auf die Frage "wurde bezahlt?" (AK 4.1)
enum class Bezahlstatus { UNBEKANNT, NICHT_BEZAHLT, BEZAHLT, NICHT_ERREICHBAR };

// Kasse aus Sicht der Schranken: Fahrzeugliste und Bezahlstatus.
// Produktiv laeuft die Kasse auf demselben Pico (direkter Aufruf). Ueber diese
// Schnittstelle koennte sie genauso gut auf einem zweiten Pico (HTTP) laufen.
class IKasse {
 public:
  virtual ~IKasse() {}
  virtual int freiePlaetze() = 0;                                  // -1 = nicht erreichbar
  virtual Einfahrt einfahren(const char* kz, bool manuell) = 0;    // kz ist normalisiert
  virtual Bezahlstatus bezahlstatus(const char* kz, uint32_t* gebuehrRappen) = 0;
  virtual bool ausfahren(const char* kz) = 0;                      // Fahrzeug austragen
};
