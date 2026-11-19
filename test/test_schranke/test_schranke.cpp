// Unit-Tests der Schrankensteuerung - isoliert von der Kasse.
// Abhaengigkeiten:
//   ISchrankenAntrieb -> MockAntrieb   (wurde geoeffnet/geschlossen?)
//   IKasse            -> MockKasse     (Antworten vorgegeben, Aufrufe gezaehlt)
//   IUhr              -> FakeUhr       (Anzeigedauer 15 s ohne Warten)
//   IProtokoll        -> DummyProtokoll

#include <unity.h>
#include <string.h>
#include "parameter.h"
#include "schranke.h"
#include "../doubles/doubles.h"

static MockAntrieb* antrieb;
static MockKasse* kasse;
static FakeUhr* uhr;
static DummyProtokoll protokoll;
static Schranke* ein;
static Schranke* aus;

void setUp() {
  antrieb = new MockAntrieb();
  kasse = new MockKasse();
  uhr = new FakeUhr();
  ein = new Schranke("ein", true, *antrieb, *kasse, *uhr, protokoll);
  aus = new Schranke("aus", false, *antrieb, *kasse, *uhr, protokoll);
  ein->start();
  aus->start();
  antrieb->zuruecksetzen();
}

void tearDown() {
  delete ein;
  delete aus;
  delete uhr;
  delete kasse;
  delete antrieb;
}

#define TEST_ASSERT_ENTHAELT(teil, text) \
  TEST_ASSERT_NOT_NULL_MESSAGE(strstr((text), (teil)), (text))

// Grundstellung: Schranke zu, Zustand bereit
void test_start_schranke_zu() {
  TEST_ASSERT_FALSE(antrieb->istOffen);
  TEST_ASSERT_TRUE(Zustand::BEREIT == ein->zustand());
  TEST_ASSERT_EQUAL_STRING("bereit", zustandText(ein->zustand()));
}

// TF-17 / AK 5.1 und AK 5.3: Standardmeldung der Einfahrt abhaengig von freien Plaetzen
void test_TF17_TF19_standardmeldung_einfahrt() {
  kasse->frei = 3;
  TEST_ASSERT_EQUAL_STRING("Willkommen – bitte vorfahren", ein->meldung());
  kasse->frei = 0;
  TEST_ASSERT_EQUAL_STRING("BESETZT – Parkhaus voll", ein->meldung());
  TEST_ASSERT_EQUAL_STRING("Bitte vorfahren", aus->meldung());
}

// TF-01 / TF-03 / AK 2.1: gueltiges Kennzeichen -> Kasse speichert, Schranke oeffnet sofort
void test_TF01_TF03_einfahrt_gueltig_oeffnet() {
  Antwort a = ein->kamera("zh 123456");

  TEST_ASSERT_TRUE(a.ok);
  TEST_ASSERT_EQUAL_INT(1, kasse->anzahlEinfahren);
  TEST_ASSERT_EQUAL_STRING("ZH 123456", kasse->letztesKz);     // normalisiert uebergeben
  TEST_ASSERT_FALSE(kasse->letztesManuell);
  TEST_ASSERT_EQUAL_INT(1, antrieb->anzahlOeffnen);            // im selben Aufruf geoeffnet
  TEST_ASSERT_TRUE(antrieb->istOffen);
  TEST_ASSERT_TRUE(Zustand::OFFEN == ein->zustand());
  TEST_ASSERT_EQUAL_STRING("ZH 123456 – bitte einfahren", ein->meldung());
}

// TF-04 / AK 2.3: Schranke schliesst erst nach der Durchfahrt
void test_TF04_schliesst_nach_durchfahrt() {
  ein->kamera("ZH 123456");
  uhr->vorspulenSek(60);
  ein->tick();
  TEST_ASSERT_TRUE(antrieb->istOffen);                         // vorher schliesst sie nicht

  Antwort a = ein->durchfahrt();
  TEST_ASSERT_TRUE(a.ok);
  TEST_ASSERT_FALSE(antrieb->istOffen);
  TEST_ASSERT_TRUE(Zustand::BEREIT == ein->zustand());

  TEST_ASSERT_FALSE(ein->durchfahrt().ok);                     // zweite Durchfahrt: nicht offen
}

// Waehrend die Schranke offen ist, wird kein weiteres Kennzeichen angenommen
void test_kamera_bei_offener_schranke_abgelehnt() {
  ein->kamera("ZH 123456");
  Antwort a = ein->kamera("LU 4521");
  TEST_ASSERT_FALSE(a.ok);
  TEST_ASSERT_EQUAL_INT(1, kasse->anzahlEinfahren);
}

// TF-05 / AK 2.2, 6.1, 5.2: ungueltige Eingaben -> Schranke zu, Hinweis, nichts gespeichert
void test_TF05_einfahrt_ungueltig_bleibt_zu() {
  const char* eingaben[] = {"", "123456", "ZH", "XX 12!3"};
  for (const char* e : eingaben) {
    Antwort a = ein->kamera(e);
    TEST_ASSERT_FALSE_MESSAGE(a.ok, e);
    TEST_ASSERT_TRUE(Zustand::NICHT_ERKANNT == ein->zustand());
    TEST_ASSERT_EQUAL_STRING("Kennzeichen nicht erkannt – bitte Hilfe-Taste drücken", ein->meldung());
    TEST_ASSERT_EQUAL_STRING("fehler", ein->typ());
  }
  TEST_ASSERT_EQUAL_INT(0, antrieb->anzahlOeffnen);
  TEST_ASSERT_EQUAL_INT(0, kasse->anzahlEinfahren);            // kein Fahrzeug gespeichert
}

// TF-06 / AK 6.2, 6.3: Hilfe-Taste, Personal erfasst manuell -> Schranke oeffnet
void test_TF06_manuelle_erfassung_einfahrt() {
  ein->kamera("");
  Antwort h = ein->hilfe();
  TEST_ASSERT_TRUE(h.ok);
  TEST_ASSERT_TRUE(Zustand::HILFE == ein->zustand());          // Personal wird benachrichtigt

  Antwort a = ein->manuell("BE 98765");
  TEST_ASSERT_TRUE(a.ok);
  TEST_ASSERT_TRUE(kasse->letztesManuell);                     // Vermerk "manuell erfasst"
  TEST_ASSERT_EQUAL_STRING("BE 98765", kasse->letztesKz);
  TEST_ASSERT_TRUE(antrieb->istOffen);
}

// Personal gibt ein ungueltiges Kennzeichen ein -> Zustand HILFE bleibt
void test_manuell_ungueltig_bleibt_in_hilfe() {
  ein->kamera("");
  ein->hilfe();
  Antwort a = ein->manuell("123");
  TEST_ASSERT_FALSE(a.ok);
  TEST_ASSERT_EQUAL_STRING("Ungültiges Kennzeichen – Format z.B. ZH 123456", a.text);
  TEST_ASSERT_TRUE(Zustand::HILFE == ein->zustand());
  TEST_ASSERT_EQUAL_INT(0, kasse->anzahlEinfahren);
}

// Hilfe beenden -> zurueck in die Grundstellung
void test_hilfe_beenden() {
  ein->kamera("");
  ein->hilfe();
  ein->hilfeBeenden();
  TEST_ASSERT_TRUE(Zustand::BEREIT == ein->zustand());
  TEST_ASSERT_FALSE(antrieb->istOffen);
}

// TF-08: Kasse meldet "bereits im Parkhaus" -> Schranke bleibt zu
void test_TF08_doppelte_einfahrt_bleibt_zu() {
  kasse->einfahrt = Einfahrt::BEREITS_IM_PARKHAUS;
  Antwort a = ein->kamera("ZH 123456");
  TEST_ASSERT_FALSE(a.ok);
  TEST_ASSERT_TRUE(Zustand::GESPERRT == ein->zustand());
  TEST_ASSERT_ENTHAELT("bereits im Parkhaus", ein->meldung());
  TEST_ASSERT_EQUAL_INT(0, antrieb->anzahlOeffnen);
}

// TF-19 / AK 5.3: Kasse meldet "voll" -> Schranke bleibt zu, Meldung BESETZT
void test_TF19_voll_bleibt_zu() {
  kasse->einfahrt = Einfahrt::VOLL;
  Antwort a = ein->kamera("SZ 7");
  TEST_ASSERT_FALSE(a.ok);
  TEST_ASSERT_EQUAL_STRING("BESETZT – Parkhaus voll", ein->meldung());
  TEST_ASSERT_FALSE(antrieb->istOffen);
}

// TF-13 / AK 4.1, 4.2: bezahlt -> Kasse wird gefragt, Fahrzeug ausgetragen, Schranke oeffnet
void test_TF13_ausfahrt_bezahlt_oeffnet() {
  kasse->status = Bezahlstatus::BEZAHLT;
  Antwort a = aus->kamera("ZH 123456");
  TEST_ASSERT_TRUE(a.ok);
  TEST_ASSERT_EQUAL_INT(1, kasse->anzahlBezahlstatus);
  TEST_ASSERT_EQUAL_INT(1, kasse->anzahlAusfahren);
  TEST_ASSERT_EQUAL_STRING("ZH 123456", kasse->letztesKz);
  TEST_ASSERT_TRUE(antrieb->istOffen);
  TEST_ASSERT_EQUAL_STRING("ZH 123456 – gute Fahrt!", aus->meldung());
}

// TF-14 / AK 4.3: nicht bezahlt -> zu, Display "Bitte zuerst an der Kasse bezahlen"
void test_TF14_ausfahrt_nicht_bezahlt_bleibt_zu() {
  kasse->status = Bezahlstatus::NICHT_BEZAHLT;
  kasse->gebuehr = 400;
  Antwort a = aus->kamera("ZH 123456");
  TEST_ASSERT_FALSE(a.ok);
  TEST_ASSERT_EQUAL_STRING("Bitte zuerst an der Kasse bezahlen", aus->meldung());
  TEST_ASSERT_EQUAL_INT(0, kasse->anzahlAusfahren);
  TEST_ASSERT_EQUAL_INT(0, antrieb->anzahlOeffnen);
}

// TF-15: unbekanntes Kennzeichen -> zu, Display "Fahrzeug unbekannt" (nicht "bitte bezahlen", F-04)
void test_TF15_ausfahrt_unbekannt_bleibt_zu() {
  kasse->status = Bezahlstatus::UNBEKANNT;
  Antwort a = aus->kamera("SZ 7");
  TEST_ASSERT_FALSE(a.ok);
  TEST_ASSERT_EQUAL_STRING("Fahrzeug unbekannt – bitte Hilfe-Taste drücken", aus->meldung());
  TEST_ASSERT_FALSE(antrieb->istOffen);
}

// TF-16: Kasse nicht erreichbar -> sicherer Zustand (zu) und Stoerungsmeldung.
// Mit echter Hardware nicht testbar (Kasse laeuft auf demselben Pico), mit dem Stub schon.
void test_TF16_kasse_nicht_erreichbar_bleibt_zu() {
  kasse->status = Bezahlstatus::NICHT_ERREICHBAR;
  Antwort a = aus->kamera("ZH 123456");
  TEST_ASSERT_FALSE(a.ok);
  TEST_ASSERT_EQUAL_STRING("Störung – Kasse nicht erreichbar", aus->meldung());
  TEST_ASSERT_FALSE(antrieb->istOffen);

  // Kasse wieder erreichbar -> naechster Versuch oeffnet
  kasse->status = Bezahlstatus::BEZAHLT;
  TEST_ASSERT_TRUE(aus->kamera("ZH 123456").ok);
  TEST_ASSERT_TRUE(antrieb->istOffen);
}

// TF-07a / AK 6.4, 4.3: manuelle Erfassung an der Ausfahrt, nicht bezahlt -> zu
void test_TF07a_manuell_ausfahrt_nicht_bezahlt() {
  kasse->status = Bezahlstatus::NICHT_BEZAHLT;
  aus->kamera("");
  aus->hilfe();
  TEST_ASSERT_FALSE(aus->manuell("BE 98765").ok);
  TEST_ASSERT_EQUAL_INT(1, kasse->anzahlBezahlstatus);         // Zahlung wurde geprueft
  TEST_ASSERT_EQUAL_STRING("Bitte zuerst an der Kasse bezahlen", aus->meldung());
  TEST_ASSERT_FALSE(antrieb->istOffen);
}

// TF-07b / AK 6.4, 4.2: manuelle Erfassung an der Ausfahrt, bezahlt -> Zahlung geprueft, offen
void test_TF07b_manuell_ausfahrt_bezahlt() {
  kasse->status = Bezahlstatus::BEZAHLT;
  aus->kamera("");
  aus->hilfe();
  TEST_ASSERT_TRUE(aus->manuell("BE 98765").ok);
  TEST_ASSERT_EQUAL_INT(1, kasse->anzahlBezahlstatus);
  TEST_ASSERT_EQUAL_INT(1, kasse->anzahlAusfahren);
  TEST_ASSERT_TRUE(antrieb->istOffen);
}

// TF-22 / F-03: "ZH 0" und "ZH 1000000" oeffnen die Schranke nicht, "ZH 1" und "ZH 999999" schon
void test_TF22_grenzwerte_kennzeichen_an_der_schranke() {
  TEST_ASSERT_FALSE(ein->kamera("ZH 0").ok);
  TEST_ASSERT_FALSE(ein->kamera("ZH 1000000").ok);
  TEST_ASSERT_EQUAL_INT(0, kasse->anzahlEinfahren);
  TEST_ASSERT_EQUAL_INT(0, antrieb->anzahlOeffnen);

  TEST_ASSERT_TRUE(ein->kamera("ZH 1").ok);
  ein->durchfahrt();
  TEST_ASSERT_TRUE(ein->kamera("ZH 999999").ok);
  TEST_ASSERT_EQUAL_INT(2, kasse->anzahlEinfahren);
}

// Gesperrt-Meldung verschwindet nach 15 s (FakeUhr statt Warten)
void test_gesperrt_meldung_verschwindet_nach_15s() {
  kasse->status = Bezahlstatus::UNBEKANNT;
  aus->kamera("SZ 7");
  uhr->vorspulenMs(GESPERRT_ANZEIGE_MS);
  aus->tick();
  TEST_ASSERT_TRUE(Zustand::GESPERRT == aus->zustand());       // genau 15 s: noch sichtbar
  uhr->vorspulenMs(1);
  aus->tick();
  TEST_ASSERT_TRUE(Zustand::BEREIT == aus->zustand());
  TEST_ASSERT_EQUAL_STRING("Bitte vorfahren", aus->meldung());
}

// Hinweis "nicht erkannt" bleibt stehen, bis Hilfe kommt (wird nicht automatisch geloescht)
void test_nicht_erkannt_bleibt_stehen() {
  ein->kamera("");
  uhr->vorspulenSek(60);
  ein->tick();
  TEST_ASSERT_TRUE(Zustand::NICHT_ERKANNT == ein->zustand());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_start_schranke_zu);
  RUN_TEST(test_TF17_TF19_standardmeldung_einfahrt);
  RUN_TEST(test_TF01_TF03_einfahrt_gueltig_oeffnet);
  RUN_TEST(test_TF04_schliesst_nach_durchfahrt);
  RUN_TEST(test_kamera_bei_offener_schranke_abgelehnt);
  RUN_TEST(test_TF05_einfahrt_ungueltig_bleibt_zu);
  RUN_TEST(test_TF06_manuelle_erfassung_einfahrt);
  RUN_TEST(test_manuell_ungueltig_bleibt_in_hilfe);
  RUN_TEST(test_hilfe_beenden);
  RUN_TEST(test_TF08_doppelte_einfahrt_bleibt_zu);
  RUN_TEST(test_TF19_voll_bleibt_zu);
  RUN_TEST(test_TF13_ausfahrt_bezahlt_oeffnet);
  RUN_TEST(test_TF14_ausfahrt_nicht_bezahlt_bleibt_zu);
  RUN_TEST(test_TF15_ausfahrt_unbekannt_bleibt_zu);
  RUN_TEST(test_TF16_kasse_nicht_erreichbar_bleibt_zu);
  RUN_TEST(test_TF07a_manuell_ausfahrt_nicht_bezahlt);
  RUN_TEST(test_TF07b_manuell_ausfahrt_bezahlt);
  RUN_TEST(test_TF22_grenzwerte_kennzeichen_an_der_schranke);
  RUN_TEST(test_gesperrt_meldung_verschwindet_nach_15s);
  RUN_TEST(test_nicht_erkannt_bleibt_stehen);
  return UNITY_END();
}
