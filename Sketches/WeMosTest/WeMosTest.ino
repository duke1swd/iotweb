#include <ESP8266WiFi.h>

#define LED LED_BUILTIN // Note, LED goes on when this pin driven low.
#define LEDHZ 100

void setup() {
  pinMode(LED, OUTPUT);
}

void loop() {
  if (millis() / (LEDHZ / 2) % 2 == 1)
    digitalWrite(LED, LOW);
  else
    digitalWrite(LED, HIGH);
}
