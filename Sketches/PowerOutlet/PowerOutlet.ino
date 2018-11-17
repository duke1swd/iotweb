/*
 * Code to manage a WiFi controlled power outlet.
 * 
 * Inputs:
 *  A push button labeled power
 * Outputs:
 *  A Blue LED under software control
 *  A relay that controls two outlets.
 *  A Red LED that is hard wired to show the relay state.
 *
 * This version does not yet do anything with the button.
 *   Future versions will allow local control of output
 *   with the push button
 *
 * The Blue LED has these modes:
 *   Powers up off.
 *   Turns on for 2 seconds when we connect to the WiFi and to the MQTT broker
 *   Blinks 50 ms on at 1 HZ when connected.
 *   Future versions will have a slower blink when not connected.
 */
#include <Homie.h>

#define FIRMWARE_NAME     "outlet-control-WiOn"
#define FIRMWARE_VERSION  "0.2.0"

/*
 * Reason codes.
 * These disclose why we are in the state we are in
 */
#define	REASON_BOOT	"boot"		// state hasn't been changed since boot, or is otherwise unknown.
#define	REASON_LOCAL	"local"		// state was last modified by local push button
#define	REASON_REMOTE	"remote"	// state was last modified by MQTT message
#define	REASON_TIME	"time"		// state was last modified by hitting a calendar event

const int PIN_RELAY = 15;
const int PIN_LED = 2;
const int PIN_BUTTON = 13;

static bool on;			// state of the relay
static const char * reason;	// one of the reason codes (above)

HomieNode outletNode("outlet", "switch");

// The state node is an input node that tells the world the state of the relay,
// and the reason for that state.  Future version will also have timing info
HomieNode outletStateNode("outlet-state", "sensor");

bool outletOnHandler(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;

  bool desired = (value == "true");
  if (desired == on) return true;

  on = desired;
  reason = REASON_REMOTE;
  outletStateNode.setProperty("state").send(on? "on": "off");
  outletStateNode.setProperty("reason").send(reason);
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
  outletNode.setProperty("on").send(value);
  Homie.getLogger() << "Outlet is " << (on ? "on" : "off") << endl;

  return true;
}

static int led_can_change;

// only send reasons when connected.
static boolean queued_reason;

/*
 * This code called once to set up, but only after completely connected.
 */
void setupHandler() {
  // turn on blue LED for 2 seconds
  digitalWrite(PIN_LED, HIGH);
  led_can_change = millis() + 2000;
}

void setup() {
  void loopHandler();
  Serial.begin(115200);
  Serial.println("WiOn Outlet Control");
  Serial.println(FIRMWARE_VERSION);
  Serial << endl << endl;

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  on = false;
  reason = REASON_BOOT;
  queued_reason = true;
  digitalWrite(PIN_LED, LOW);

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  outletNode.advertise("on").settable(outletOnHandler);
  outletNode.advertise("state");
  outletNode.advertise("reason");

  Homie.setup();
}

/*
 * This code is called once per loop(), but only
 * when connected to WiFi and MQTT broker
 */
void loopHandler() {
  int t = millis();

  if (t > led_can_change)
    digitalWrite(PIN_LED, ((t/50)%20 == 0)? HIGH: LOW);  // blink 1 HZ, 5% cycle

  if (queued_reason) {
    queued_reason = false;
    outletStateNode.setProperty("state").send(on? "on": "off");
    outletStateNode.setProperty("reason").send(reason);
  }
}

void loop() {
  Homie.loop();
}
