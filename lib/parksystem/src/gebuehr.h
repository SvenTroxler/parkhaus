#pragma once
// Berechnung der Parkgebuehr (US3, AK 3.1).
// CHF 2.00 pro angefangene Stunde, hoechstens CHF 20.00 pro 24 Stunden.
// Gezaehlt werden ganze Minuten; die erste Minute zaehlt bereits als Stunde.
//
//   0h01 -> 2.00   0h59 -> 2.00   1h00 -> 2.00   1h01 -> 4.00
//   9h59 -> 20.00  12h00 -> 20.00 24h00 -> 20.00 24h01 -> 22.00

#include <stdint.h>
#include "parameter.h"

inline uint32_t berechneGebuehrRappen(uint32_t parkdauerSekunden) {
  uint32_t minuten = parkdauerSekunden / 60;
  if (minuten == 0) minuten = 1;

  uint32_t tage = minuten / 1440;
  uint32_t rest = minuten % 1440;
  uint32_t stunden = (rest + 59) / 60;

  uint32_t restBetrag = stunden * TARIF_PRO_STUNDE;
  if (restBetrag > TARIF_MAX_PRO_TAG) restBetrag = TARIF_MAX_PRO_TAG;

  return tage * TARIF_MAX_PRO_TAG + restBetrag;
}
