// Integrationstests: echte Kasse + echte Schranken, nur die Hardware ist ersetzt.
// Abhaengigkeiten:
//   ISchrankenAntrieb -> MockAntrieb (je einer fuer Ein- und Ausfahrt)
//   IKasse            -> SpyKasse um die echte Kasse (zeichnet Aufrufe auf)
//   IUhr              -> FakeUhr (Parkdauer vorspulen statt warten)
//   IProtokoll        -> DummyProtokoll

#include <unity.h>
#include "kasse.h"
#include "schranke.h"
#include "../doubles/doubles.h"

static FakeUhr* uhr;
static DummyProtokoll protokoll;
static Kasse* kasse;
static SpyKasse* spy;
static MockAntrieb* antriebEin;
static MockAntrieb* antriebAus;
static Schranke* ein;
static Schranke* aus;

void setUp() {
  uhr = new FakeUhr();
  kasse = new Kasse(*uhr, protokoll);
  spy = new SpyKasse(*kasse);
  antriebEin = new MockAntrieb();
  antriebAus = new MockAntrieb();
  ein = new Schranke("ein", true, *antriebEin, *spy, *uhr, protokoll);
  aus = new Schranke("aus", false, *antriebAus, *spy, *uhr, protokoll);
  ein->start();
  aus->start();
}

void tearDown() {
  delete ein; delete aus;
  delete antriebEin; delete antriebAus;
  delete spy; delete kasse; delete uhr;
}

static void einfahren(const char* kz) {
  TEST_ASSERT_TRUE_MESSAGE(ein->kamera(kz).ok, kz);
  ein->durchfahrt();
}

static void ausfahren(const char* kz) {
  TEST_ASSERT_TRUE_MESSAGE(aus->kamera(kz).ok, kz);
  aus->durchfahrt();
}

// TF-18 / AK 5.2: Anzeige freie Plaetze zeigt nacheinander 2, 1, 2
void test_TF18_anzeige_aktualisiert_sich() {
  TEST_ASSERT_EQUAL_INT(3, kasse->freiePlaetze());
  einfahren("ZH 123456");
  TEST_ASSERT_EQUAL_INT(2, kasse->freiePlaetze());
  einfahren("LU 4521");
  TEST_ASSERT_EQUAL_INT(1, kasse->freiePlaetze());
  kasse->zahlen("ZH 123456", "twint", true, nullptr);
  ausfahren("ZH 123456");
  TEST_ASSERT_EQUAL_INT(2, kasse->freiePlaetze());
}

// TF-19 / AK 5.3: voll -> SZ 7 abgewiesen; nach einer Ausfahrt kann SZ 7 einfahren
void test_TF19_parkhaus_voll() {
  einfahren("ZH 123456");
  einfahren("LU 4521");
  einfahren("BE 98765");
  TEST_ASSERT_EQUAL_STRING("BESETZT – Parkhaus voll", ein->meldung());

  TEST_ASSERT_FALSE(ein->kamera("SZ 7").ok);
  TEST_ASSERT_FALSE(antriebEin->istOffen);
  TEST_ASSERT_NULL(kasse->finde("SZ 7"));

  kasse->zahlen("LU 4521", "kreditkarte", true, nullptr);
  ausfahren("LU 4521");
  uhr->vorspulenSek(16);
  ein->tick();                                                 // Meldung "BESETZT" verschwindet

  TEST_ASSERT_TRUE(ein->kamera("SZ 7").ok);
  TEST_ASSERT_TRUE(antriebEin->istOffen);
  TEST_ASSERT_NOT_NULL(kasse->finde("SZ 7"));
}

// TF-12 / AK 4.3: fehlgeschlagene Zahlung -> Ausfahrt bleibt zu
void test_TF12_fehlgeschlagene_zahlung_ausfahrt_zu() {
  einfahren("ZH 123456");
  kasse->zahlen("ZH 123456", "kreditkarte", false, nullptr);
  TEST_ASSERT_FALSE(aus->kamera("ZH 123456").ok);
  TEST_ASSERT_FALSE(antriebAus->istOffen);
  TEST_ASSERT_TRUE(Bezahlstatus::NICHT_BEZAHLT == spy->letzterStatus);
  TEST_ASSERT_NOT_NULL(kasse->finde("ZH 123456"));             // Fahrzeug bleibt im Parkhaus
}

// TF-20: Gesamtablauf - 1 h 01 parken, CHF 4.00 mit Twint, Ausfahrt
void test_TF20_gesamtablauf() {
  einfahren("ZH 123456");
  TEST_ASSERT_EQUAL_INT(2, kasse->freiePlaetze());

  uhr->vorspulenMin(61);                                       // statt Testmodus/Warten
  uint32_t betrag = 0;
  TEST_ASSERT_TRUE(KassenErgebnis::OK == kasse->zahlen("ZH 123456", "twint", true, &betrag));
  TEST_ASSERT_EQUAL_UINT32(400, betrag);

  TEST_ASSERT_TRUE(aus->kamera("ZH 123456").ok);
  TEST_ASSERT_TRUE(antriebAus->istOffen);
  TEST_ASSERT_EQUAL_INT(3, kasse->freiePlaetze());
  aus->durchfahrt();
  TEST_ASSERT_FALSE(antriebAus->istOffen);
}

// TF-07: manuelle Erfassung bei der Ausfahrt, a) nicht bezahlt, b) bezahlt
void test_TF07_manuelle_erfassung_ausfahrt() {
  einfahren("BE 98765");

  // a) nicht bezahlt
  aus->kamera("");
  aus->hilfe();
  TEST_ASSERT_FALSE(aus->manuell("BE 98765").ok);
  TEST_ASSERT_FALSE(antriebAus->istOffen);
  aus->hilfeBeenden();

  // b) bezahlt
  kasse->zahlen("BE 98765", "kreditkarte", true, nullptr);
  aus->kamera("");
  aus->hilfe();
  TEST_ASSERT_TRUE(aus->manuell("BE 98765").ok);
  TEST_ASSERT_TRUE(antriebAus->istOffen);
}

// TF-21: Gesamtablauf mit nicht erkanntem Kennzeichen (Einfahrt und Ausfahrt manuell)
void test_TF21_gesamtablauf_nicht_erkannt() {
  TEST_ASSERT_FALSE(ein->kamera("").ok);
  ein->hilfe();
  TEST_ASSERT_TRUE(ein->manuell("LU 4521").ok);
  ein->durchfahrt();
  TEST_ASSERT_TRUE(spy->letztesManuell);
  TEST_ASSERT_TRUE(kasse->finde("LU 4521")->manuell);          // Vermerk "manuell erfasst"

  uhr->vorspulenMin(30);
  TEST_ASSERT_TRUE(KassenErgebnis::OK == kasse->zahlen("LU 4521", "kreditkarte", true, nullptr));

  TEST_ASSERT_FALSE(aus->kamera("").ok);
  aus->hilfe();
  int vorher = spy->anzahlBezahlstatus;
  TEST_ASSERT_TRUE(aus->manuell("LU 4521").ok);
  // Spy: die Zahlungspruefung wurde auch bei manueller Erfassung durchgefuehrt
  TEST_ASSERT_EQUAL_INT(vorher + 1, spy->anzahlBezahlstatus);
  TEST_ASSERT_TRUE(Bezahlstatus::BEZAHLT == spy->letzterStatus);
  TEST_ASSERT_EQUAL_INT(1, spy->anzahlAusfahren);
  TEST_ASSERT_EQUAL_INT(3, kasse->freiePlaetze());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_TF18_anzeige_aktualisiert_sich);
  RUN_TEST(test_TF19_parkhaus_voll);
  RUN_TEST(test_TF12_fehlgeschlagene_zahlung_ausfahrt_zu);
  RUN_TEST(test_TF20_gesamtablauf);
  RUN_TEST(test_TF07_manuelle_erfassung_ausfahrt);
  RUN_TEST(test_TF21_gesamtablauf_nicht_erkannt);
  return UNITY_END();
}
