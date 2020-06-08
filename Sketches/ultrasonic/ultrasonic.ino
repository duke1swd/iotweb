#include <ESP8266WiFi.h>

#define LED LED_BUILTIN // Note, LED goes on when this pin driven low.
#define LEDPERIOD 100  // in milliseconds
#define TOGGLE_PIN D5



unsigned long next_flip;
unsigned long on_time, off_time;
unsigned char state;

void setup() {
  String s;
  unsigned long k, p;

  pinMode(LED, OUTPUT);
  pinMode(TOGGLE_PIN, OUTPUT);

  Serial.begin(9600);
  do {
    Serial.println("Enter KHz");
    s = Serial.readStringUntil('\n');
    k = s.toInt();
  } while (k <= 0 || k > 60);
  Serial.print("Setting KHz to ");
  Serial.println(k);

  p = 1000 / k; // the period in microseconds
  on_time = p / 2;
  off_time = p - on_time;
  Serial.print("On time: ");
  Serial.println(on_time);
  Serial.print("  Off time: ");
  Serial.println(off_time);

  state = 0;
  next_flip = micros() + off_time;
}

void loop() {
  if (micros() > next_flip) {
    if (state) {
      state = 0;
      next_flip += off_time;
      digitalWrite(TOGGLE_PIN, LOW);
    } else {
      state = 1;
      next_flip += on_time;
      digitalWrite(TOGGLE_PIN, HIGH);
    }
  }
}
