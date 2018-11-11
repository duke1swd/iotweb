/*
 * Code to manage a WiFi controlled power outlet.
 * This code started life as an example fromthe Homie getting started pages.
 * 
 * Inputs:
 *  A push button labeled power
 * Outputs:
 *  An LED labeled power
 *  An LED labeled with a WiFi symbol
 *  A relay that controls two outlets.
 * Not that the WiFI LED and the Relay appear to be on the same pin.
 */
#include <Homie.h>

const int PIN_RELAY = 15;
const int PIN_LED = 2;
const int PIN_BUTTON = 13;

HomieNode outletNode("outlet", "switch");

bool outletOnHandler(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;

  bool on = (value == "true");
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
  outletNode.setProperty("on").send(value);
  Homie.getLogger() << "Outlet is " << (on ? "on" : "off") << endl;

  return true;
}

void setup() {
  Serial.begin(115200);
  Serial << endl << endl;
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_LED, LOW);

  Homie_setFirmware("outlet-control", "1.0.0");

  outletNode.advertise("on").settable(outletOnHandler);

  Homie.setup();
}

void loop() {
  Homie.loop();
  digitalWrite(PIN_LED, ((millis()/500)%2 == 0)? HIGH: LOW);  // blink 1 HZ, 50% cycle
}
