#include <Arduino.h>
/*
 * Modified from Homie 2.0 documentation
 *
 * Use as a test case to prove that the setProperty method does
 * not call the handler for that property.  But it does send
 * out the MQTT message with the property's new value.
 */
#include <Homie.h>

#define FIRMWARE_NAME     "Simple LED Control"
#define FIRMWARE_VERSION  "0.1.3"

/*
 * IO Pins
 */
const int PIN_LED = 2;
#define	LED_ON_VALUE	LOW		// pull the pin low to turn on the LED
#define	LED_OFF_VALUE	HIGH

/*
 * Global State
 */
bool on;

HomieNode ledNode("led", "simpleLedControl", "switch");

bool ledOnHandler(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;

  on = (value == "true");
  digitalWrite(PIN_LED, on ? LED_ON_VALUE : LED_OFF_VALUE);
  ledNode.setProperty("on").send(value);
  Homie.getLogger() << "LED is " << (on ? "on" : "off") << endl;

  return true;
}

/*
 * This code called once to set up, but only after completely connected.
 */
void setupHandler() {
}

/*
 * This code is called once per loop(), but only
 * when connected to WiFi and MQTT broker
 */
void loopHandler() {
}

void setup() {
  Serial.begin(115200);
  Serial << endl << endl;
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);

  ledNode.advertise("on").settable(ledOnHandler);

  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  Homie.setup();
}

void loop() {
  Homie.loop();
}
