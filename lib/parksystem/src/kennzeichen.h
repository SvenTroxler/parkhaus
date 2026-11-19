#pragma once
// Pruefung und Vereinheitlichung von Kennzeichen (US1, US6).
//
// Gueltig: 2-3 Buchstaben (Kanton) + Zahl 1-999999 (ohne fuehrende Null),
// Leerzeichen dazwischen erlaubt.
//   "zh123456" -> "ZH 123456", "SZ 7" -> "SZ 7", "ZH 1" und "ZH 999999" (TF-22)
// Ungueltig: "" , "123456" (Kanton fehlt), "ZH" (Nummer fehlt), "XX 12!3" (Sonderzeichen),
//   "ZH 0", "ZH 007" (Zahl beginnt mit 0), "ZH 1000000" (zu gross)  (TF-22, Fehler F-03)

#include <stddef.h>

static const size_t KZ_LAENGE = 12;  // inkl. '\0', z.B. "BSZ 123456"

inline bool normalisiereKennzeichen(const char* eingabe, char* ausgabe, size_t groesse) {
  char buchstaben[4] = {0};
  char ziffern[7] = {0};
  size_t nb = 0, nz = 0;

  for (const char* p = eingabe; p && *p; p++) {
    char c = *p;
    if (c == ' ') continue;
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';

    if (c >= 'A' && c <= 'Z') {
      if (nz > 0 || nb >= 3) return false;   // Buchstabe nach Ziffer / zu lang
      buchstaben[nb++] = c;
    } else if (c >= '0' && c <= '9') {
      if (nz >= 6) return false;              // groesser als 999999
      if (nz == 0 && c == '0') return false;  // "ZH 0", fuehrende Null (F-03)
      ziffern[nz++] = c;
    } else {
      return false;                           // Sonderzeichen
    }
  }

  if (nb < 2 || nz < 1) return false;
  if (groesse < nb + nz + 2) return false;

  size_t i = 0;
  for (size_t k = 0; k < nb; k++) ausgabe[i++] = buchstaben[k];
  ausgabe[i++] = ' ';
  for (size_t k = 0; k < nz; k++) ausgabe[i++] = ziffern[k];
  ausgabe[i] = '\0';
  return true;
}
