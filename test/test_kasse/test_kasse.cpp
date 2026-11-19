// Unit-Tests der Kasse (Fahrzeugliste, Gebuehr, Zahlung).
// Abhaengigkeiten: IUhr -> FakeUhr, IProtokoll -> DummyProtokoll

#include <unity.h>
#include "kasse.h"
#include "../doubles/doubles.h"

static FakeUhr* uhr;
static DummyProtokoll protokoll;
static Kasse* kasse;

void setUp() {
  uhr = new FakeUhr();
  kasse = new Kasse(*uhr, protokoll);
}

void tearDown() {
  delete kasse;
  delete uhr;
}

static uint32_t gebuehrVon(const char* kz) {
  uint32_t g = 0;
  kasse->bezahlstatus(kz, &g);
  return g;
}

// TF-17 (Kasse): Startzustand 3 freie Plaetze
void test_TF17_startzustand_alle_plaetze_frei() {
  TEST_ASSERT_EQUAL_INT(KAPAZITAET, kasse->freiePlaetze());
}

// TF-01 / AK 1.2: Kennzeichen, Datum und Uhrzeit werden gespeichert
void test_TF01_einfahrt_speichert_kennzeichen_und_zeit() {
  uint32_t t0 = uhr->unixZeit();
  TEST_ASSERT_TRUE(Einfahrt::OK == kasse->einfahren("ZH 123456", false));

  const Fahrzeug* f = kasse->finde("ZH 123456");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_UINT32(t0, f->einfahrt);
  TEST_ASSERT_FALSE(f->bezahlt);
  TEST_ASSERT_FALSE(f->manuell);
  TEST_ASSERT_EQUAL_INT(KAPAZITAET - 1, kasse->freiePlaetze());
}

// TF-02: mehrere Fahrzeuge, jedes mit eigener Einfahrtszeit
void test_TF02_einfahrtszeiten_mehrerer_fahrzeuge() {
  uint32_t t0 = uhr->unixZeit();
  kasse->einfahren("ZH 123456", false);
  uhr->vorspulenMin(1);
  kasse->einfahren("LU 4521", false);
  uhr->vorspulenMin(1);
  kasse->einfahren("BE 98765", false);

  TEST_ASSERT_EQUAL_UINT32(t0,       kasse->finde("ZH 123456")->einfahrt);
  TEST_ASSERT_EQUAL_UINT32(t0 + 60,  kasse->finde("LU 4521")->einfahrt);
  TEST_ASSERT_EQUAL_UINT32(t0 + 120, kasse->finde("BE 98765")->einfahrt);
  TEST_ASSERT_EQUAL_INT(0, kasse->freiePlaetze());
}

// AK 6.3: manuell erfasste Fahrzeuge tragen einen Vermerk
void test_AK63_manuell_erfasst_wird_vermerkt() {
  kasse->einfahren("BE 98765", true);
  TEST_ASSERT_TRUE(kasse->finde("BE 98765")->manuell);
}

// TF-08: doppelte Einfahrt wird abgelehnt, Eintrag bleibt unveraendert
void test_TF08_doppelte_einfahrt_abgelehnt() {
  kasse->einfahren("ZH 123456", false);
  uint32_t t0 = kasse->finde("ZH 123456")->einfahrt;
  uhr->vorspulenMin(5);

  TEST_ASSERT_TRUE(Einfahrt::BEREITS_IM_PARKHAUS == kasse->einfahren("ZH 123456", false));
  TEST_ASSERT_EQUAL_UINT32(t0, kasse->finde("ZH 123456")->einfahrt);
  TEST_ASSERT_EQUAL_INT(KAPAZITAET - 1, kasse->freiePlaetze());
}

// TF-19 / AK 5.3: Parkhaus voll -> keine Einfahrt, nichts gespeichert
void test_TF19_parkhaus_voll() {
  kasse->einfahren("ZH 123456", false);
  kasse->einfahren("LU 4521", false);
  kasse->einfahren("BE 98765", false);

  TEST_ASSERT_TRUE(Einfahrt::VOLL == kasse->einfahren("SZ 7", false));
  TEST_ASSERT_NULL(kasse->finde("SZ 7"));
  TEST_ASSERT_EQUAL_INT(0, kasse->freiePlaetze());
}

// TF-09 / AK 3.1: Gebuehr wird aus der Parkdauer berechnet (Zeit per FakeUhr)
void test_TF09_gebuehr_aus_parkdauer() {
  kasse->einfahren("ZH 123456", false);
  uhr->vorspulenMin(1);
  TEST_ASSERT_EQUAL_UINT32(200, gebuehrVon("ZH 123456"));
  uhr->vorspulenMin(59);                 // 1 h 00
  TEST_ASSERT_EQUAL_UINT32(200, gebuehrVon("ZH 123456"));
  uhr->vorspulenMin(1);                  // 1 h 01
  TEST_ASSERT_EQUAL_UINT32(400, gebuehrVon("ZH 123456"));
  uhr->vorspulenMin(23 * 60);            // 24 h 01
  TEST_ASSERT_EQUAL_UINT32(2200, gebuehrVon("ZH 123456"));
}

// TF-09 / TF-20: Testmodus setzt die Einfahrtszeit zurueck
void test_testmodus_einfahrtszeit_setzen() {
  kasse->einfahren("ZH 123456", false);
  TEST_ASSERT_TRUE(KassenErgebnis::OK == kasse->setzeEinfahrtszeit("zh123456", 61));
  TEST_ASSERT_EQUAL_UINT32(400, gebuehrVon("ZH 123456"));
  TEST_ASSERT_TRUE(KassenErgebnis::UNBEKANNT == kasse->setzeEinfahrtszeit("SZ 7", 10));
  TEST_ASSERT_TRUE(KassenErgebnis::UNGUELTIG == kasse->setzeEinfahrtszeit("123", 10));
}

// TF-10: Zahlung mit Kreditkarte -> Status bezahlt
void test_TF10_zahlung_kreditkarte() {
  kasse->einfahren("ZH 123456", false);
  uhr->vorspulenMin(61);
  uint32_t betrag = 0;
  TEST_ASSERT_TRUE(KassenErgebnis::OK == kasse->zahlen("ZH 123456", "kreditkarte", true, &betrag));
  TEST_ASSERT_EQUAL_UINT32(400, betrag);
  TEST_ASSERT_TRUE(kasse->finde("ZH 123456")->bezahlt);
  TEST_ASSERT_TRUE(Bezahlstatus::BEZAHLT == kasse->bezahlstatus("ZH 123456", nullptr));
}

// TF-11: Zahlung mit Twint -> Status bezahlt
void test_TF11_zahlung_twint() {
  kasse->einfahren("LU 4521", false);
  TEST_ASSERT_TRUE(KassenErgebnis::OK == kasse->zahlen("LU 4521", "twint", true, nullptr));
  TEST_ASSERT_TRUE(Bezahlstatus::BEZAHLT == kasse->bezahlstatus("LU 4521", nullptr));
}

// TF-12: fehlgeschlagene Zahlung -> Status bleibt "nicht bezahlt"
void test_TF12_zahlung_fehlgeschlagen() {
  kasse->einfahren("ZH 123456", false);
  TEST_ASSERT_TRUE(KassenErgebnis::ZAHLUNG_FEHLGESCHLAGEN == kasse->zahlen("ZH 123456", "twint", false, nullptr));
  TEST_ASSERT_FALSE(kasse->finde("ZH 123456")->bezahlt);
  TEST_ASSERT_TRUE(Bezahlstatus::NICHT_BEZAHLT == kasse->bezahlstatus("ZH 123456", nullptr));
}

// AK 3.2: nur Kreditkarte und Twint; Fehlerfaelle der Kasse
void test_zahlung_fehlerfaelle() {
  kasse->einfahren("ZH 123456", false);
  TEST_ASSERT_TRUE(KassenErgebnis::ZAHLART == kasse->zahlen("ZH 123456", "bar", true, nullptr));
  TEST_ASSERT_TRUE(KassenErgebnis::UNGUELTIG == kasse->zahlen("", "twint", true, nullptr));
  TEST_ASSERT_TRUE(KassenErgebnis::UNBEKANNT == kasse->zahlen("SZ 7", "twint", true, nullptr));
  kasse->zahlen("ZH 123456", "twint", true, nullptr);
  TEST_ASSERT_TRUE(KassenErgebnis::BEREITS_BEZAHLT == kasse->zahlen("ZH 123456", "twint", true, nullptr));
  TEST_ASSERT_EQUAL_STRING("zahlung_fehlgeschlagen", grund(KassenErgebnis::ZAHLUNG_FEHLGESCHLAGEN));
}

// Nach dem Bezahlen steigt die Gebuehr nicht mehr, auch wenn Zeit vergeht
void test_bezahlter_betrag_bleibt_fix() {
  kasse->einfahren("ZH 123456", false);
  kasse->zahlen("ZH 123456", "twint", true, nullptr);
  uhr->vorspulenMin(5 * 60);
  TEST_ASSERT_EQUAL_UINT32(200, gebuehrVon("ZH 123456"));
}

// TF-13 / TF-15: Ausfahrt austragen, unbekanntes Kennzeichen
void test_TF13_TF15_ausfahren() {
  kasse->einfahren("ZH 123456", false);
  TEST_ASSERT_TRUE(Bezahlstatus::UNBEKANNT == kasse->bezahlstatus("SZ 7", nullptr));
  TEST_ASSERT_FALSE(kasse->ausfahren("SZ 7"));
  TEST_ASSERT_TRUE(kasse->ausfahren("ZH 123456"));
  TEST_ASSERT_NULL(kasse->finde("ZH 123456"));
  TEST_ASSERT_EQUAL_INT(KAPAZITAET, kasse->freiePlaetze());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_TF17_startzustand_alle_plaetze_frei);
  RUN_TEST(test_TF01_einfahrt_speichert_kennzeichen_und_zeit);
  RUN_TEST(test_TF02_einfahrtszeiten_mehrerer_fahrzeuge);
  RUN_TEST(test_AK63_manuell_erfasst_wird_vermerkt);
  RUN_TEST(test_TF08_doppelte_einfahrt_abgelehnt);
  RUN_TEST(test_TF19_parkhaus_voll);
  RUN_TEST(test_TF09_gebuehr_aus_parkdauer);
  RUN_TEST(test_testmodus_einfahrtszeit_setzen);
  RUN_TEST(test_TF10_zahlung_kreditkarte);
  RUN_TEST(test_TF11_zahlung_twint);
  RUN_TEST(test_TF12_zahlung_fehlgeschlagen);
  RUN_TEST(test_zahlung_fehlerfaelle);
  RUN_TEST(test_bezahlter_betrag_bleibt_fix);
  RUN_TEST(test_TF13_TF15_ausfahren);
  return UNITY_END();
}
