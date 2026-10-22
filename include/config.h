#pragma once
// Konfiguration Parksystem (ein Raspberry Pi Pico W)

#include <Arduino.h>

// WLAN: Der Pico spannt selbst das Test-WLAN auf -> http://192.168.42.1
#define WLAN_SSID     "Parkhaus-Troxler-Traut"
#define WLAN_PASSWORT "parkhaus123"   // mind. 8 Zeichen

// Parkhaus (Testkonzept Kap. 9: 3 Plaetze)
#define KAPAZITAET 3

// Tarif (Testkonzept Kap. 9): CHF 2.00 pro angefangene Stunde,
// hoechstens CHF 20.00 pro 24 Stunden. Betraege in Rappen.
#define TARIF_PRO_STUNDE  200
#define TARIF_MAX_PRO_TAG 2000

// Servos SG90: rot -> VBUS (Pin 40), braun -> GND, orange -> Signal
#define PIN_SERVO_EIN 8    // GP8 (Pin 11), GND an Pin 8
#define PIN_SERVO_AUS 9    // GP9 (Pin 12), GND an Pin 13

#define SERVO_WINKEL_ZU    0
#define SERVO_WINKEL_OFFEN 90
