#pragma once
// Fachliche Parameter des Parksystems (ohne Hardware-Bezug).
// Werden von der Logik und von den Tests verwendet.

// Parkhaus (Testkonzept Kap. 9: 3 Plaetze)
#define KAPAZITAET 3

// Tarif (Testkonzept Kap. 9): CHF 2.00 pro angefangene Stunde,
// hoechstens CHF 20.00 pro 24 Stunden. Betraege in Rappen.
#define TARIF_PRO_STUNDE  200
#define TARIF_MAX_PRO_TAG 2000

// Gesperrt-Meldungen (z.B. "nicht bezahlt") verschwinden nach dieser Zeit
#define GESPERRT_ANZEIGE_MS 15000UL
