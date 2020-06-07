#include <ESP8266WiFi.h>

#define LED LED_BUILTIN // Note, LED goes on when this pin driven low.
#define LEDPERIOD 1000  // in milliseconds

void setup() {
  pinMode(LED, OUTPUT);
}

void loop() {
  if (millis() / (LEDPERIOD / 2) % 2 == 1)
    digitalWrite(LED, LOW);
  else
    digitalWrite(LED, HIGH);
}
