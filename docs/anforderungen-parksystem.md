# A01-A03 – Implementierung Parkhaus mit Schranke
 
## Gruppenmitglieder
 
- Sven Troxler
- Lukas Traut
 
## Kurze Beschreibung der Umsetzung
 
Das Parksystem läuft auf zwei Raspberry Pi Pico (Arduino), die per UART (GP0/GP1 gekreuzt, GND–GND) verbunden sind. Die Schranken sind je ein SG90-Servo an GP8.
 
- **Pico 1 – Einfahrt** (`einfahrt/`): erfasst das Kennzeichen, gibt bei Bedarf ein Papierticket aus, steuert die Einfahrtsschranke und zeigt die freien Plätze an.
- **Pico 2 – Ausfahrt + Kassenautomat** (`ausfahrt_kasse/`): verwaltet alle Parkvorgänge, berechnet die Parkgebühr, nimmt die Bezahlung entgegen und öffnet die Ausfahrtsschranke nur nach vollständiger Bezahlung.
 
Kamera, Induktionsschleife, Zahlungsterminal und Anzeigen werden über den Seriellen Monitor simuliert (115200 Baud, `?` zeigt alle Befehle). Weil der Pico keine Echtzeituhr hat, läuft auf Pico 2 eine simulierte Uhr (10× schneller, mit `zeit +<min>` vorspulbar).
 
Standardtarif: CHF 2.00 pro angefangene Stunde, Tagesmaximum CHF 25.00, 15 Min gratis, 15 Min Freifahrt nach dem Bezahlen.
 
## Umgesetzte User Stories
 
| Story | Status |
|---|---|
| US-01 Einfahrt mit Kennzeichenerfassung | ✅ (Kennzeichenerkennung simuliert) |
| US-02 Ersatzweg bei nicht erkanntem Kennzeichen | ✅ |
| US-03 Automatische Schrankenöffnung bei Einfahrt | ✅ |
| US-04 Ausfahrt nur nach Bezahlung | ✅ |
| US-05 Bezahlen am Kassenautomaten | ✅ (Zahlungsterminal simuliert) |
| US-06 Tarifberechnung | ✅ |
| US-07 Freifahrt-Fenster nach Bezahlung | ✅ |
| US-08 Dauerparker mit Abonnement | ✅ |
| US-09 Belegungsanzeige und Vollsperrung | ✅ |
| US-10 Sicherheit der Schranke | ✅ (ausser Stromausfall) |
| US-11 Gegensprechanlage / Hilferuf | teilweise (Hilferuf + Fernöffnung, keine Sprechverbindung) |
| US-12 Betriebs-Dashboard und Störungsmeldung | teilweise (Protokoll nur im RAM) |
| US-13 Auswertung und Umsatzreport | teilweise (Tagesreport + CSV-Export) |
| US-14 Datenschutzkonforme Speicherung | teilweise (Löschfrist, Auskunft, Zugriffsprotokoll) |
| US-15 Weiterbetrieb bei Netzwerkausfall | simuliert |