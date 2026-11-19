# Parksystem mit Raspberry Pi Pico W

## Gruppenmitglieder

- Lukas Traut
- Sven Troxler

## Beschreibung der Umsetzung

Das Parkhaus läuft auf **einem** Raspberry Pi Pico W (Arduino-Framework, PlatformIO, Core *earlephilhower*). Der Pico spannt ein eigenes WLAN auf, steuert beide Schranken (Servo SG90) und stellt zwei Weboberflächen bereit:

| Seite | Inhalt |
|---|---|
| http://192.168.42.1 | Schranken: Kamera-Simulation, Display (freie Plätze, Hinweise), Hilfe-Taste, Personal-Ansicht |
| http://192.168.42.1/kasse | Kasse: Fahrzeugliste, Parkgebühr, simulierte Zahlung, Testmodus |

**Ablauf**

1. **Einfahrt:** Ein Kennzeichen wird eingegeben (simuliert die Kamera). Ist es gültig und ein Platz frei, werden Kennzeichen und Einfahrtszeit gespeichert und die Schranke öffnet. Mit „Auto durchgefahren“ schliesst sie wieder.
2. **Bezahlen:** In der Kasse wird das Fahrzeug gewählt. Die Gebühr wird aus der Parkdauer berechnet (CHF 2.00 pro angefangene Stunde, max. CHF 20.00 pro 24 h). Die Zahlung mit Kreditkarte oder Twint wird simuliert (erfolgreich / fehlgeschlagen).
3. **Ausfahrt:** Die Schranke öffnet nur, wenn für das Kennzeichen bezahlt wurde, sonst zeigt das Display einen Hinweis.
4. **Nicht erkanntes Kennzeichen:** Bei leerer oder ungültiger Eingabe bleibt die Schranke zu und das Display zeigt „Kennzeichen nicht erkannt – bitte Hilfe-Taste drücken“. Die Hilfe-Taste benachrichtigt das Personal, das das Kennzeichen manuell erfasst. Danach läuft alles wie bei der automatischen Erkennung weiter.

**Hinweise**

- **Zeit:** Der Pico hat keine Echtzeituhr. Die Uhrzeit wird vom Browser bei jeder Anfrage mitgeschickt.
- **Daten:** Die Fahrzeugliste liegt nur im RAM. Ein Neustart ergibt ein leeres Parkhaus.
- **Testmodus** (Kasse): Einfahrtszeit eines Fahrzeugs zurücksetzen, damit sich lange Parkdauern testen lassen (TF-09, TF-20).

## Umgesetzte User Stories

| User Story | Umsetzung | Akzeptanzkriterien |
|---|---|---|
| **US1** Kennzeichen bei Einfahrt erkennen | Kamera-Simulation, Speicherung mit Datum/Uhrzeit | AK 1.1 ✅, AK 1.2 ✅, AK 1.3 ➖ (keine echte Kamera) |
| **US2** Einfahrtsschranke automatisch öffnen | Servo öffnet nach Erkennung, schliesst nach Durchfahrt | AK 2.1 ✅, AK 2.2 ✅, AK 2.3 ✅ |
| **US3** Parkgebühr bezahlen | Gebühr aus Parkdauer, simulierte Zahlung Kreditkarte/Twint, Status „bezahlt“ | AK 3.1 ✅, AK 3.2 ✅ (simuliert), AK 3.3 ✅ |
| **US4** Ausfahrtsschranke nur nach Bezahlung | Bezahlstatus wird bei der Ausfahrt geprüft | AK 4.1 ✅, AK 4.2 ✅, AK 4.3 ✅ |
| **US5** Freie Parkplätze anzeigen | Display bei der Einfahrt, „BESETZT“ bei vollem Parkhaus | AK 5.1 ✅, AK 5.2 ✅, AK 5.3 ✅ |
| **US6** Nicht erkanntes Kennzeichen behandeln | Hinweis, Hilfe-Taste, Personal-Ansicht, Vermerk „manuell erfasst“ | AK 6.1 ✅, AK 6.2 ✅, AK 6.3 ✅, AK 6.4 ✅ |

Zusätzlich: Eine doppelte Einfahrt desselben Kennzeichens wird abgelehnt (TF-08), ebenso eine Ausfahrt mit unbekanntem Kennzeichen (TF-15). TF-16 (Kasse nicht erreichbar) kann auf der Hardware nicht auftreten, da Kasse und Schranken auf demselben Pico laufen. Die Schranke behandelt den Fall trotzdem (sicherer Zustand: zu, Störungsmeldung) und er wird automatisiert getestet (siehe unten).

## Hardware

| Servo | Signal (orange) | VCC (rot) | GND (braun) |
|---|---|---|---|
| Schranke 1 (Einfahrt) | GP8 (Pin 11) | VBUS (Pin 40) | GND (Pin 8) |
| Schranke 2 (Ausfahrt) | GP9 (Pin 12) | VBUS (Pin 40) | GND (Pin 13) |

## Installation und Start

1. Pico im BOOTSEL-Modus anschliessen, `upload_port` in `platformio.ini` ggf. anpassen, dann `pio run -e pico -t upload`.
2. Laptops mit dem WLAN `Parkhaus-Troxler-Traut` (Passwort `parkhaus123`) verbinden.
3. Laptop A: http://192.168.42.1 (Schranken), Laptop B: http://192.168.42.1/kasse (Kasse).

---

# Automatisierte Tests mit Test Doubles (A02-01)

Branch: `testdoubles`

## Aufbau

Die Logik des Parksystems wurde aus `main.cpp` herausgelöst und liegt jetzt in der Bibliothek `lib/parksystem`. Sie ruft **keine Hardware-Funktionen** mehr auf (kein `Servo`, `millis()`, `Serial`, `WiFi`, `WebServer`, kein `Arduino.h`). Alle Abhängigkeiten sind als Schnittstellen definiert ([`schnittstellen.h`](lib/parksystem/src/schnittstellen.h)) und werden über den Konstruktor übergeben (Dependency Injection).

```
                    ┌───────────── lib/parksystem (Logik, ohne Hardware) ─────────────┐
 Weboberfläche ───► │  Schranke ("ein"/"aus") ──IKasse──► Kasse                       │
 (main.cpp)         │     │            │                    │                         │
                    │ ISchrankenAntrieb IUhr, IProtokoll   IUhr, IProtokoll           │
                    └─────┼────────────┼────────────────────┼─────────────────────────┘
        Pico (src/hardware.h):   ServoAntrieb   PicoUhr   SerialProtokoll
        Test (test/doubles):     MockAntrieb    FakeUhr   DummyProtokoll   (+ Stub/Mock/SpyKasse)
```

| Datei | Inhalt |
|---|---|
| `lib/parksystem/src/schnittstellen.h` | Schnittstellen `ISchrankenAntrieb`, `IUhr`, `IProtokoll`, `IKasse` |
| `lib/parksystem/src/kasse.*` | Fahrzeugliste, Gebühr, Zahlung, Testmodus (implementiert `IKasse`) |
| `lib/parksystem/src/schranke.*` | Zustandsautomat einer Schranke inkl. Display-Meldung |
| `lib/parksystem/src/gebuehr.h`, `kennzeichen.h` | Tarifberechnung, Prüfung/Normalisierung Kennzeichen |
| `src/hardware.h` | Produktive Implementierungen: `ServoAntrieb`, `PicoUhr`, `SerialProtokoll` |
| `src/main.cpp` | Erzeugt die Objekte, verbindet sie und übersetzt HTTP-Anfragen in Aufrufe der Logik |
| `test/doubles/doubles.h` | Alle Test Doubles |
| `test/test_*/` | Je ein Testprogramm (Unity) |

Die Web-Schnittstelle (`/api/...`) und das Verhalten auf dem Pico sind unverändert.

## Identifizierte Abhängigkeiten

| Abhängigkeit | Warum im automatisierten Test ungeeignet? | Was braucht die Logik wirklich? | Test Double | Begründung |
|---|---|---|---|---|
| **Schranke** (Servo SG90 an GP8/GP9, `Servo.write()`) | Nur auf dem Pico vorhanden; Ergebnis muss man von Auge beobachten (aufwändig) | „öffnen“ und „schliessen“ | **Mock** `MockAntrieb` | Der Test muss prüfen, *ob* und *wie oft* geöffnet/geschlossen wurde und in welcher Stellung die Schranke ist. Genau das ist ein Mock (Stub mit Erinnerungsvermögen). |
| **Zeit** (`millis()`, Uhrzeit vom Browser) | Langsam (Parkdauer von 1 h 01 oder 24 h 01 abwarten, 15 s Anzeigedauer) und nicht deterministisch | aktuelle Unix-Zeit (Ein-/Ausfahrt, Gebühr) und Laufzeit in ms (Anzeigedauer) | **Fake** `FakeUhr` | Eine vereinfachte, funktionierende Uhr, die nur läuft, wenn der Test sie vorspult (`vorspulenMin(61)`). Stunden und Tage dauern so Mikrosekunden, und jeder Testlauf ist gleich. |
| **Kasse** aus Sicht der Schranke (Abfrage „bezahlt?“; bei zwei Picos wäre das die WLAN/HTTP-Kommunikation) | Zustände wie „bezahlt“ oder „unbekannt“ müssen erst aufwändig hergestellt werden; „Kasse nicht erreichbar“ lässt sich gar nicht herstellen | `freiePlaetze`, `einfahren`, `bezahlstatus`, `ausfahren` | **Stub** `StubKasse` | Für Schranken-Tests reichen fest vorgegebene Antworten (z.B. `status = NICHT_ERREICHBAR` für TF-16, `einfahrt = VOLL` für TF-19). |
| | | | **Mock** `MockKasse` | Zusätzlich wird gezählt, ob die Schranke die Kasse aufruft und mit welchem Kennzeichen (z.B. „bei ungültigem Kennzeichen wird nichts gespeichert“, „normalisiertes Kennzeichen wird übergeben“, „manuell“-Vermerk). |
| | | | **Spy** `SpyKasse` | In den Ablauf-Tests läuft die **echte** Kasse. Der Spy umschliesst sie und zeichnet die Aufrufe auf. Damit bleibt es ein Integrationstest, und zusätzlich lässt sich nachweisen, dass auch bei manueller Erfassung die Zahlung geprüft wurde (TF-21). |
| **Protokoll** (`Serial.printf`, serieller Monitor) | Nur auf dem Pico; hat keinen Einfluss auf das Verhalten | nichts (nur Ausgabe) | **Dummy** `DummyProtokoll` | Die Logik verlangt ein Protokoll-Objekt, für die Tests ist es aber irrelevant. Ein leerer Platzhalter genügt. |
| **Kamera** bzw. deren Simulation über die Weboberfläche | Browser, WLAN und Klicks sind nötig (aufwändig, langsam) | nur das erkannte Kennzeichen als Text (`""` = nicht erkannt) | keines nötig | Die Weboberfläche ruft nur noch `schranke.kamera("ZH 123456")` auf. Der Test ruft dieselbe Methode direkt auf und übernimmt so die Rolle der Kamera. Dasselbe gilt für Hilfe-Taste, Personal-Eingabe und „Auto durchgefahren“. |
| **Zahlungsterminal** (Kreditkarte/Twint, simuliert über die Weboberfläche) | wie Kamera | Zahlart und Ergebnis „erfolgreich/fehlgeschlagen“ | keines nötig | Wird als Parameter an `kasse.zahlen(kz, art, erfolgreich, ...)` übergeben. |
| **Speicherung** der Fahrzeuge | Liegt nur im RAM (Array in `Kasse`), ist schnell und deterministisch | – | keines nötig | Die echte Speicherung wird in den Tests verwendet. Würde sie später im Flash (LittleFS) liegen, käme eine Schnittstelle mit einem **Fake** (z.B. Array statt Datei) dazu. |

## Automatisierte Testfälle

Tests ausführen (ohne Pico, auf dem PC):

```
pio test -e native
```

Die Tests laufen auf dem PC (`env:native`) mit dem Test-Framework Unity. Dafür braucht es einen GCC für den PC (Windows: MinGW-w64, z.B. über MSYS2, im `PATH`). Die Firmware wird weiterhin mit `pio run -e pico` gebaut.

| Testprogramm | Was wird getestet | Test Doubles | Anzahl |
|---|---|---|---|
| `test_regeln` | Gebühr und Kennzeichen-Regeln (reine Funktionen) | keine | 5 |
| `test_kasse` | Kasse isoliert | FakeUhr, DummyProtokoll | 14 |
| `test_schranke` | Schranke isoliert, ohne echte Kasse | MockAntrieb, MockKasse (Stub-Antworten), FakeUhr, DummyProtokoll | 18 |
| `test_ablauf` | Echte Kasse + echte Schranken (Integration), nur Hardware ersetzt | SpyKasse, MockAntrieb, FakeUhr, DummyProtokoll | 6 |

Bezug zum Testkonzept (A01-02-10):

| TF | Testfall | Automatisiert in | Bemerkung |
|---|---|---|---|
| TF-01 | Einfahrt mit gültigem Kennzeichen registrieren | `test_kasse`, `test_schranke` | Kennzeichen + Zeit gespeichert, Kasse wird mit normalisiertem Kennzeichen aufgerufen |
| TF-02 | Einfahrtszeiten mehrerer Fahrzeuge | `test_kasse` | FakeUhr statt 2 Min. warten |
| TF-03 | Einfahrtsschranke öffnet innerhalb 2 s | `test_schranke` (teilweise) | Geprüft wird, dass `oeffnen()` im selben Aufruf erfolgt. Die echte Zeit (Browser → WLAN → Servo) nicht. |
| TF-04 | Schranke schliesst nach Durchfahrt | `test_schranke`, `test_ablauf` | inkl. „schliesst vorher nicht“ |
| TF-05 | Einfahrt ohne gültiges Kennzeichen | `test_regeln`, `test_schranke` | alle ungültigen Eingaben aus Kap. 9; Schranke zu, Meldung, nichts gespeichert |
| TF-06 | Manuelle Erfassung bei Einfahrt | `test_schranke`, `test_kasse` | Hilfe-Zustand, Vermerk „manuell erfasst“ |
| TF-07 | Manuelle Erfassung bei Ausfahrt | `test_schranke`, `test_ablauf` | Variante a (nicht bezahlt) und b (bezahlt) |
| TF-08 | Doppelte Einfahrt | `test_kasse`, `test_schranke` | |
| TF-09 | Parkgebühr (Grenzwerte) | `test_regeln`, `test_kasse` | alle 8 Zeilen der Gebühren-Testdaten |
| TF-10 | Zahlung mit Kreditkarte | `test_kasse` | |
| TF-11 | Zahlung mit Twint | `test_kasse` | |
| TF-12 | Fehlgeschlagene Zahlung | `test_kasse`, `test_ablauf` | Status bleibt offen, Ausfahrt bleibt zu |
| TF-13 | Ausfahrt nach erfolgter Zahlung | `test_schranke`, `test_ablauf` | Kasse wird gefragt, Fahrzeug ausgetragen |
| TF-14 | Ausfahrt ohne Zahlung | `test_schranke` | inkl. Betrag im Hinweis |
| TF-15 | Ausfahrt mit unbekanntem Kennzeichen | `test_schranke`, `test_kasse` | |
| TF-16 | Kasse nicht erreichbar | `test_schranke` | nur mit dem Stub möglich (`NICHT_ERREICHBAR`) |
| TF-17 | Anzeige freie Plätze im Startzustand | `test_kasse`, `test_schranke` | |
| TF-18 | Anzeige aktualisiert sich | `test_ablauf` | 2 → 1 → 2 |
| TF-19 | Parkhaus voll | `test_kasse`, `test_schranke`, `test_ablauf` | |
| TF-20 | Gesamtablauf | `test_ablauf` | 1 h 01 → CHF 4.00, Twint |
| TF-21 | Gesamtablauf mit nicht erkanntem Kennzeichen | `test_ablauf` | Spy beweist die Zahlungsprüfung bei manueller Ausfahrt |

Zusätzlich getestet: Gesperrt-Meldung verschwindet nach genau 15 s, „nicht erkannt“ bleibt bis zur Hilfe stehen, bezahlter Betrag steigt nicht mehr, ungültige Zahlart, Testmodus.

## Fazit

Die Logik (Gebühr, Fahrzeugliste, Zahlung, Schranken-Zustände) ist jetzt vollständig automatisiert und in Sekunden testbar, ohne Firmware hochzuladen oder auf Parkzeit zu warten. Dank Stub ist sogar TF-16 testbar, das auf der Hardware gar nicht herstellbar ist.

Nicht (oder nur teilweise) automatisieren lassen sich:

- **TF-03 (2 Sekunden):** Die Zeit hängt von Browser, WLAN, Webserver und der mechanischen Bewegung des Servos ab. Der Test zeigt nur, dass die Logik den Befehl sofort gibt. Die echte Reaktionszeit muss mit Stoppuhr gemessen werden.
- **Bewegt sich die Schranke wirklich?** Der Mock zeigt nur, dass `oeffnen()` aufgerufen wurde. Ob der Servo richtig verdrahtet ist und den richtigen Winkel fährt, sieht man nur an der Hardware.
- **AK 1.3 (Kamera bei Dunkelheit/Wetter):** Es gibt keine echte Kamera. Das ist ausserhalb des Testumfangs.
- **Weboberfläche / Display (Teile von TF-05, TF-06, TF-17, TF-18):** Ob die Meldung im Browser „sofort“ bzw. innerhalb 5 s erscheint und ob das Personal die Benachrichtigung sieht, hängt vom JavaScript und vom Polling ab. Getestet ist nur der Text, den die Logik liefert.
- **WLAN, Webserver, Uhrzeit vom Browser (`PicoUhr`), JSON-Ausgabe in `main.cpp`:** Das sind die Adapter zur Hardware. Sie sind bewusst dünn gehalten und werden weiterhin manuell (Smoke-Test, Testprotokoll) geprüft.
- **TF-16 auf der echten Hardware:** Es gibt keine Verbindung zwischen zwei Picos, die man trennen könnte. Getestet ist nur die Reaktion der Logik.
