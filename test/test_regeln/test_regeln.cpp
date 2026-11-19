// Unit-Tests der reinen Rechenregeln (keine Abhaengigkeiten -> keine Test Doubles noetig).
// TF-09 (Gebuehr, Grenzwerte), Testdaten Kennzeichen aus Testkonzept Kap. 9 (TF-05)
// und Grenzwerte Kennzeichen (TF-22, Testkonzept V1.1).

#include <unity.h>
#include "gebuehr.h"
#include "kennzeichen.h"

void setUp() {}
void tearDown() {}

static uint32_t minuten(uint32_t h, uint32_t m) { return (h * 60 + m) * 60; }

// TF-09: Tabelle "Gebuehren-Testdaten" aus dem Testkonzept
void test_TF09_gebuehr_grenzwerte() {
  TEST_ASSERT_EQUAL_UINT32(200,  berechneGebuehrRappen(minuten(0, 1)));    // erste angefangene Stunde
  TEST_ASSERT_EQUAL_UINT32(200,  berechneGebuehrRappen(minuten(0, 59)));   // knapp unter Stundengrenze
  TEST_ASSERT_EQUAL_UINT32(200,  berechneGebuehrRappen(minuten(1, 0)));    // genau auf Stundengrenze
  TEST_ASSERT_EQUAL_UINT32(400,  berechneGebuehrRappen(minuten(1, 1)));    // knapp ueber Stundengrenze
  TEST_ASSERT_EQUAL_UINT32(2000, berechneGebuehrRappen(minuten(9, 59)));   // Maximum erreicht
  TEST_ASSERT_EQUAL_UINT32(2000, berechneGebuehrRappen(minuten(12, 0)));   // Tagesmaximum greift
  TEST_ASSERT_EQUAL_UINT32(2000, berechneGebuehrRappen(minuten(24, 0)));   // genau 1 Tag
  TEST_ASSERT_EQUAL_UINT32(2200, berechneGebuehrRappen(minuten(24, 1)));   // neuer Tag
}

void test_gebuehr_unter_einer_minute_zaehlt_als_erste_stunde() {
  TEST_ASSERT_EQUAL_UINT32(200, berechneGebuehrRappen(0));
  TEST_ASSERT_EQUAL_UINT32(200, berechneGebuehrRappen(59));
}

void test_gebuehr_mehrere_tage() {
  TEST_ASSERT_EQUAL_UINT32(4000, berechneGebuehrRappen(minuten(48, 0)));
  TEST_ASSERT_EQUAL_UINT32(4400, berechneGebuehrRappen(minuten(49, 1)));
}

static void pruefeGueltig(const char* eingabe, const char* erwartet) {
  char kz[KZ_LAENGE];
  TEST_ASSERT_TRUE_MESSAGE(normalisiereKennzeichen(eingabe, kz, sizeof(kz)), eingabe);
  TEST_ASSERT_EQUAL_STRING(erwartet, kz);
}

static void pruefeUngueltig(const char* eingabe) {
  char kz[KZ_LAENGE];
  TEST_ASSERT_FALSE_MESSAGE(normalisiereKennzeichen(eingabe, kz, sizeof(kz)), eingabe);
}

// Testkonzept Kap. 9: gueltige Kennzeichen
void test_kennzeichen_gueltig() {
  pruefeGueltig("ZH 123456", "ZH 123456");
  pruefeGueltig("LU 4521", "LU 4521");
  pruefeGueltig("BSZ 7", "BSZ 7");
  pruefeGueltig("zh123456", "ZH 123456");      // Kleinbuchstaben, ohne Leerzeichen
  pruefeGueltig("  sz   7 ", "SZ 7");
}

// Testkonzept Kap. 9 / TF-05: ungueltige Eingaben
void test_TF05_kennzeichen_ungueltig() {
  pruefeUngueltig("");                // leeres Feld
  pruefeUngueltig("123456");          // Kanton fehlt
  pruefeUngueltig("ZH");              // Nummer fehlt
  pruefeUngueltig("XX 12!3");         // Sonderzeichen
  pruefeUngueltig("Z 1");             // nur ein Buchstabe
  pruefeUngueltig("ZHZH 1");          // zu viele Buchstaben
  pruefeUngueltig("ZH 1234567");      // zu viele Ziffern
  pruefeUngueltig("ZH 12 A");         // Buchstabe nach Ziffer
  pruefeUngueltig(nullptr);
}

// TF-22 (Testkonzept V1.1): Grenzwerte der Nummer 1..999999 (Fehler F-03)
void test_TF22_kennzeichen_grenzwerte() {
  pruefeGueltig("ZH 1", "ZH 1");              // unterer Grenzwert
  pruefeGueltig("ZH 999999", "ZH 999999");    // oberer Grenzwert
  pruefeUngueltig("ZH 0");                    // knapp unter dem unteren Grenzwert
  pruefeUngueltig("ZH 1000000");              // knapp ueber dem oberen Grenzwert
  pruefeUngueltig("ZH 007");                  // fuehrende Null
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_TF09_gebuehr_grenzwerte);
  RUN_TEST(test_gebuehr_unter_einer_minute_zaehlt_als_erste_stunde);
  RUN_TEST(test_gebuehr_mehrere_tage);
  RUN_TEST(test_kennzeichen_gueltig);
  RUN_TEST(test_TF05_kennzeichen_ungueltig);
  RUN_TEST(test_TF22_kennzeichen_grenzwerte);
  return UNITY_END();
}
