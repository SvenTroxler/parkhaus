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

Zusätzlich: Eine doppelte Einfahrt desselben Kennzeichens wird abgelehnt (TF-08), ebenso eine Ausfahrt mit unbekanntem Kennzeichen (TF-15). TF-16 (Kasse nicht erreichbar) entfällt, da Kasse und Schranken auf demselben Pico laufen.

## Hardware

| Servo | Signal (orange) | VCC (rot) | GND (braun) |
|---|---|---|---|
| Schranke 1 (Einfahrt) | GP8 (Pin 11) | VBUS (Pin 40) | GND (Pin 8) |
| Schranke 2 (Ausfahrt) | GP9 (Pin 12) | VBUS (Pin 40) | GND (Pin 13) |

## Installation und Start

1. Pico im BOOTSEL-Modus anschliessen, `upload_port` in `platformio.ini` ggf. anpassen, dann `pio run -t upload`.
2. Laptops mit dem WLAN `Parkhaus-Test` (Passwort `parkhaus123`) verbinden.
3. Laptop A: http://192.168.42.1 (Schranken), Laptop B: http://192.168.42.1/kasse (Kasse).
