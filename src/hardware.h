#pragma once
// Produktive Implementierungen der Schnittstellen (nur auf dem Pico).
// Hier - und nur hier - werden Hardware-Funktionen aufgerufen.

#include <Arduino.h>
#include <Servo.h>

#include "config.h"
#include "schnittstellen.h"

// Schranke: Servo SG90 an einem GPIO
class ServoAntrieb : public ISchrankenAntrieb {
 public:
  explicit ServoAntrieb(int pin) : pin_(pin) {}
  void begin() { servo_.attach(pin_); }
  void oeffnen() override { servo_.write(SERVO_WINKEL_OFFEN); }
  void schliessen() override { servo_.write(SERVO_WINKEL_ZU); }

 private:
  int pin_;
  Servo servo_;
};

// Uhr: Der Pico hat keine Echtzeituhr. Die Unix-Zeit wird vom Browser bei jeder
// Anfrage mitgeschickt (Parameter t) und mit millis() fortgeschrieben.
class PicoUhr : public IUhr {
 public:
  uint32_t unixZeit() override { return zeitBasis_ + millis() / 1000; }
  uint32_t laufzeitMs() override { return millis(); }

  void setzeUnixZeit(uint32_t t) {
    if (t < 1700000000UL) return;
    uint32_t neu = t - millis() / 1000;
    uint32_t diff = neu > zeitBasis_ ? neu - zeitBasis_ : zeitBasis_ - neu;
    if (!gesetzt_ || diff > 3) {
      zeitBasis_ = neu;
      gesetzt_ = true;
    }
  }

 private:
  uint32_t zeitBasis_ = 0;   // Unix-Zeit bei millis() == 0
  bool gesetzt_ = false;
};

// Protokoll: serieller Monitor
class SerialProtokoll : public IProtokoll {
 public:
  void schreibe(const char* text) override { Serial.println(text); }
};
