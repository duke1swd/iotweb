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
#include <Bounce2.h>
#include <Homie.h>

#define FIRMWARE_NAME     "outlet-control-WiOn"
#define FIRMWARE_VERSION  "0.3.0"

/*
 * Reason codes.
 * These disclose why we are in the state we are in
 */
#define	REASON_BOOT	"boot"		// state hasn't been changed since boot, or is otherwise unknown.
#define	REASON_LOCAL	"local"		// state was last modified by local push button
#define	REASON_REMOTE	"remote"	// state was last modified by MQTT message
#define	REASON_TIME	"time"		// state was last modified by hitting a calendar event

/*
 * IO Pins
 */
const int PIN_RELAY = 15;
const int PIN_LED = 2;
const int PIN_BUTTON = 13;

/*
 * Stuff controlling / recording our state.
 */
static bool on;			// state of the relay
static const char * reason;	// one of the reason codes (above)
static bool rising;		// true when we've detected a button push but have not yet done anything with it.

static int led_can_change;	// False when we are not to blink LED.  Mostly because some non-standard blinking going on.

// only update properties when connected.
// code that may run when not connected sets these variables to tell the
// code that runs when we are connected to inform the world of a change
static boolean connected;
static boolean queued_reason;
static boolean queued_ton;
static boolean queued_toff;
static boolean queued_time_last_change;

// Schedule stuff
int	time_to_turn_on;	// set to zero if not in use
int	time_to_turn_off;	// set to zero if not in use
int	time_base;		// Current time is time_base + millis()/1000
int	time_last_change;	// When did we last change?

// Debounce the input button
Bounce debouncer = Bounce(); // Instantiate a Bounce object

HomieNode outletNode("outlet", "switch");

// The state node is an input node that tells the world the state of the relay,
// and the reason for that state.
HomieNode outletStateNode("outlet-state", "sensor");

/* convert integer to string */
static char *i_to_s(int i)
{
	static char buf[36];

	itoa(i, buf, 10);
	return buf;
}

// Handle changes to the "on" property.
bool outletOnHandler(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;

  bool desired = (value == "true");
  if (desired == on) return true;

  on = desired;
  outletStateNode.setProperty("state").send(on? "on": "off");

  reason = REASON_REMOTE;
  outletStateNode.setProperty("reason").send(reason);

  time_last_change = time_base + millis()/1000;
  outletStateNode.setProperty("time-last-change").send(i_to_s(time_last_change));

  Homie.getLogger() << "Outlet is " << (on ? "on" : "off") << endl;

  digitalWrite(PIN_RELAY, on ? HIGH : LOW);

  return true;
}

// Handle requests to schedule an turn-on time
//bool outletTOnHandler(const Homie

void onHomieEvent(const HomieEvent& event) {
  switch(event.type) {
    case HomieEventType::STANDALONE_MODE:
      // Do whatever you want when standalone mode is started
      connected = false;
      break;
    case HomieEventType::CONFIGURATION_MODE:
      // Do whatever you want when configuration mode is started
      connected = false;
      break;
    case HomieEventType::NORMAL_MODE:
      // Do whatever you want when normal mode is started
      connected = false;
      break;
    case HomieEventType::OTA_STARTED:
      // Do whatever you want when OTA is started
      connected = false;
      break;
    case HomieEventType::OTA_PROGRESS:
      // Do whatever you want when OTA is in progress

      // You can use event.sizeDone and event.sizeTotal
      connected = false;
      break;
    case HomieEventType::OTA_FAILED:
      // Do whatever you want when OTA is failed
      connected = true;
      break;
    case HomieEventType::OTA_SUCCESSFUL:
      // Do whatever you want when OTA is successful
      connected = true;
      break;
    case HomieEventType::ABOUT_TO_RESET:
      // Do whatever you want when the device is about to reset
      connected = false;
      break;
    case HomieEventType::WIFI_CONNECTED:
      // Do whatever you want when Wi-Fi is connected in normal mode

      // You can use event.ip, event.gateway, event.mask
      break;
    case HomieEventType::WIFI_DISCONNECTED:
      // Do whatever you want when Wi-Fi is disconnected in normal mode

      // You can use event.wifiReason
      break;
    case HomieEventType::MQTT_READY:
      // Do whatever you want when MQTT is connected in normal mode
      connected = true;
      break;
    case HomieEventType::MQTT_DISCONNECTED:
      // Do whatever you want when MQTT is disconnected in normal mode

      // You can use event.mqttReason
      connected = false;
      break;
    case HomieEventType::MQTT_PACKET_ACKNOWLEDGED:
      // Do whatever you want when an MQTT packet with QoS > 0 is acknowledged by the broker

      // You can use event.packetId
      break;
    case HomieEventType::READY_TO_SLEEP:
      // After you've called `prepareToSleep()`, the event is triggered when MQTT is disconnected
      break;
  }
}

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

  connected = false;

  debouncer.attach(PIN_BUTTON, INPUT); // Attach the debouncer to the pin
  debouncer.interval(25); // Use a debounce interval of 25 milliseconds

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  on = false;
  reason = REASON_BOOT;
  queued_reason = true;
  queued_ton = true;
  queued_toff = true;
  time_last_change = 0;
  queued_time_last_change = true;
  digitalWrite(PIN_LED, LOW);

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
  Homie.onEvent(onHomieEvent); 

  outletNode.advertise("on").settable(outletOnHandler);
  outletNode.advertise("state");
  outletNode.advertise("reason");
  outletNode.advertise("time-on").settable(outletTOnHandler);
  outletNode.advertise("time-off").settable(outletTOffHandler);
  outletNode.advertise("time-last-change");

  Homie.setup();
}

/*
 * This code is called once per loop(), but only
 * when connected to WiFi and MQTT broker
 */
void loopHandler() {

  if (rising) {
    rising = false;
  }

  if (queued_reason) {
    queued_reason = false;
    outletStateNode.setProperty("state").send(on? "on": "off");
    outletStateNode.setProperty("reason").send(reason);
  }

  if (queued_ton) {
    queued_ton = false;
    outletStateNode.setProperty("time-on").send("");
  }

  if (queued_toff) {
    queued_toff = false;
    outletStateNode.setProperty("time-off").send("");
  }

  if (queued_time_last_change) {
    queued_time_last_change = false;
    outletStateNode.setProperty("time-last-change").send(i_to_s(time_last_change));
  }
}

void loop() {
  int now;

  now = time_base + millis()/1000;

  // Process schedule events
  if (on &&
	time_to_turn_off &&
	time_base &&
	now > time_to_turn_off) {
		on = false;
		reason = REASON_TIME;
		time_to_turn_off = 0;
		time_last_change = now;
		queued_reason = true;
		queued_toff = true;
		queued_time_last_change = true;
  }

  if (!on &&
	time_to_turn_on &&
	time_base &&
	now > time_to_turn_on) {
		on = true;
		reason = REASON_TIME;
		time_to_turn_on = 0;
		time_last_change = now;
		queued_reason = true;
		queued_ton = true;
		queued_time_last_change = true;
  }

  // Process push button
  debouncer.update(); // Update the Bounce instance
  if (debouncer.rose()) {
	if (!connected) {
		on = !on;
		reason = REASON_LOCAL;
		queued_reason = true;
		time_last_change = time_base + millis()/1000;
		queued_time_last_change = true;
	} else
		rising = true;
  }

  // Push any local-mode changes in relay state to the hardware
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
  int t = millis();

  if (t > led_can_change) {
    // blink 1 HZ, 5% cycle when connected, 0.2HZ 5% when not
    if (!connected)
    	t /= 5;
    digitalWrite(PIN_LED, ((t/50)%20 == 0)? HIGH: LOW); 
  }

  Homie.loop();
}
