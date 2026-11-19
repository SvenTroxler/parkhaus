#pragma once
// Hardware-Konfiguration Parksystem (ein Raspberry Pi Pico W).
// Fachliche Parameter (Kapazitaet, Tarif) stehen in lib/parksystem/src/parameter.h.

#include "parameter.h"

// WLAN: Der Pico spannt selbst das Test-WLAN auf -> http://192.168.42.1
#define WLAN_SSID     "Parkhaus-Troxler-Traut"
#define WLAN_PASSWORT "parkhaus123"   // mind. 8 Zeichen

// Servos SG90: rot -> VBUS (Pin 40), braun -> GND, orange -> Signal
#define PIN_SERVO_EIN 8    // GP8 (Pin 11), GND an Pin 8
#define PIN_SERVO_AUS 9    // GP9 (Pin 12), GND an Pin 13

#define SERVO_WINKEL_ZU    0
#define SERVO_WINKEL_OFFEN 90
