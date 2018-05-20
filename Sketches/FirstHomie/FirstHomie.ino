//
// Copied from the Homie v1.5 getting started page.
// Then modified to control the on-board LED, rather than a hypothetical external relay.
//
#include <Homie.h>

static const uint8_t PIN_LED0 = D4;
static const uint8_t PIN_LED1 = D3;
static const uint8_t PIN_LED2 = D8;

unsigned char blink_state;
int blink_last_time;
const int blink_time = 100; // milliseconds; half-period
const unsigned char blink_start = 11;  // set to 2*n+1 for n blinks

HomieNode ledNode("local-led", "switch");

bool ledOnHandler(String value) {
  if (value == "true") {
    //digitalWrite(PIN_LED0, LOW);
    Homie.setNodeProperty(ledNode, "on", "true"); // Update the state of the led
    //Serial.println("LED is on");
    blink_state = blink_start;
    blink_last_time = millis();
  } else if (value == "false") {
    //digitalWrite(PIN_LED0, HIGH);
    Homie.setNodeProperty(ledNode, "on", "false");
    //Serial.println("LED is off");
    blink_state = 0;
  } else {
    return false;
  }

  return true;
}

void setup() {
  pinMode(PIN_LED0, OUTPUT);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  digitalWrite(PIN_LED0, HIGH);
  digitalWrite(PIN_LED1, HIGH);
  digitalWrite(PIN_LED2, HIGH);
  blink_state = 0;
  //Serial.println("Hello World.");

  Homie.setFirmware("hello-world", "0.2.2");
  ledNode.subscribe("on", ledOnHandler);
  Homie.registerNode(ledNode);
  Homie.enableBuiltInLedIndicator(false);   // take control of the LED from Homie
  Homie.setup();
  Homie.setNodeProperty(ledNode, "on", "true"); // LED is on at startup
}

void loop() {
  Homie.loop();
  if ((blink_state & 1) == 0)
    digitalWrite(PIN_LED0, HIGH);  // turn it off
  else
    digitalWrite(PIN_LED0, LOW); // turn it on

  if (blink_state > 1 && millis() - blink_last_time > blink_time) {
    blink_last_time += blink_time;
    blink_state--;
  }
}
