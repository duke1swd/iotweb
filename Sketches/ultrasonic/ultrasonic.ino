#include <ESP8266WiFi.h>

#define LED LED_BUILTIN // Note, LED goes on when this pin driven low.
#define LEDPERIOD 100  // in milliseconds
#define TOGGLE_PIN D5


#define OSC_FLIP  35    // flip state every 25 uSec to generate 20KHz 50% duty cycle wave

unsigned long next_flip;
unsigned char state;

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(TOGGLE_PIN, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("Hello World!");
  
  state = 0;
  next_flip = micros() + OSC_FLIP;
}

void loop() {
  if (micros() > next_flip) {
    next_flip += OSC_FLIP;
    if (state) {
      state = 0;
      digitalWrite(TOGGLE_PIN, LOW);
    } else {
      state = 1;
      digitalWrite(TOGGLE_PIN, HIGH);
    }
  }
}
