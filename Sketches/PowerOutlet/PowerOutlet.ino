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
 *   Blinks 250 ms on at 1/5 HZ when not connected
 */
#include <Bounce2.h>
#include <Homie.h>

#define FIRMWARE_NAME     "outlet-control-WiOn"
#define FIRMWARE_VERSION  "1.0.2"

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
static bool buttonState;	// set true by pressing button.  Set false by external entity.

static long led_can_change;	// False when we are not to blink LED.  Mostly because some non-standard blinking going on.

// only update properties when connected.
// code that may run when not connected sets these variables to tell the
// code that runs when we are connected to inform the world of a change
static boolean connected;
static boolean queued_reason;
static boolean queued_ton;
static boolean queued_toff;
static boolean queued_time_last_change;
static boolean queued_remote_set;
static boolean desired_remote_set;

// Schedule stuff
long	time_to_turn_on;	// set to zero if not in use
long	time_to_turn_off;	// set to zero if not in use
long	time_base;		// Current time is time_base + millis()/1000
long	queued_time_base;	// push set of time base out of ISR
long	time_last_change;	// When did we last change?

// Debounce the input button
Bounce debouncer = Bounce(); // Instantiate a Bounce object

// This node is controls the relay
HomieNode outletNode("outlet", "outlet", "relay");

// This node watches the push button
HomieNode buttonNode("button", "button", "button");

/* convert integer to string */
static char *l_to_s(long i)
{
	static char buf[36];

	itoa(i, buf, 10);
	return buf;
}

/****
 *
 * Message Handlers
 *
 * NOTE: the message handlers are called asynchronously from the TCP/IP upcal.
 * We assume this means they may be called from an interrupt at any time.
 * For this reason their function is mostly to record stuff for later
 * processing in the loop code.
 *
 ****/

// Broadcast handler.  Useful for time base.
bool broadcastHandler(const String& level, const String& value) {
  // Only broadcast we know about it IOTtime.
  if (level == "IOTtime") {
	long t = value.toInt();

	if (t < 0) return false;

	queued_time_base = t - millis()/1000;
	return true;
  }
  return false;
}

// Handle "on" property messages
bool outletOnHandler(const HomieRange& range, const String& value) {

  // If we don't understand the value then we didn't handle the message
  if (value != "true" && value != "false") return false;

  bool desired = (value == "true");
  if (desired != on) {
	  desired_remote_set = desired;
	  queued_remote_set = true;
  }

  // Message handled
  return true;
}

// Handle "time-on" messages
bool outletTOnHandler(const HomieRange& range, const String& value) {
  long t = value.toInt();

  if (t < 0) return false;

  time_to_turn_on = t;
  queued_ton = true;
  return true;
}

// Handle "time-off" messages
bool outletTOffHandler(const HomieRange& range, const String& value) {
  long t = value.toInt();

  if (t < 0) return false;

  time_to_turn_off = t;
  queued_toff = true;
  return true;
}

// Nothing to do when someone clears this state.
bool buttonSetHandler(const HomieRange& range, const String& value) {
  // If we don't understand the value then we didn't handle the message
  if (value != "true" && value != "false") return false;

  buttonState = (value == "true");

  return true;
}



/*
 * This code called once to set up, but only after completely connected.
 */
void setupHandler() {
  // turn on blue LED for 2 seconds
  digitalWrite(PIN_LED, HIGH);
  led_can_change = millis() + 2000;
  connected = true;
}

void setup() {
  unsigned char i;

  void loopHandler();
  Serial.begin(115200);
  for (i = 0; i < 4; i++) {
	Serial.println("WiOn Outlet Control");
	Serial.println(FIRMWARE_VERSION);
	Serial << endl << endl;
	delay(1000);
  }

  connected = false;

  debouncer.attach(PIN_BUTTON, INPUT); // Attach the debouncer to the pin
  debouncer.interval(25); // Use a debounce interval of 25 milliseconds

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  time_base = 0;
  queued_time_base = 0;
  on = false;
  buttonState = false;
  reason = REASON_BOOT;
  queued_reason = true;
  time_to_turn_on = 0;
  queued_ton = true;
  time_to_turn_off = 0;
  queued_toff = true;
  time_last_change = 0;
  queued_time_last_change = true;
  queued_remote_set = false;
  digitalWrite(PIN_LED, LOW);

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  outletNode.advertise("on").settable(outletOnHandler);
  outletNode.advertise("reason");
  outletNode.advertise("time-on").settable(outletTOnHandler);
  outletNode.advertise("time-off").settable(outletTOffHandler);
  outletNode.advertise("time-last-change");

  buttonNode.advertise("button").settable(buttonSetHandler);

  Homie.setBroadcastHandler(broadcastHandler);

  Homie.disableLedFeedback(); // allow this code to handle LED


  Serial.println("Calling Homie.setup");
  Homie.setup();
}

/*
 * This code is called once per loop(), but only
 * when connected to WiFi and MQTT broker
 * All queued state changes are relayed to Homie
 *
 */
void loopHandler() {

  // If we are here, then by definition we are connected.
  connected = true;

  // If we need to update the time base, block interrupts and do it here.
  time_base = queued_time_base;

  // If the base level code made changes, send that
  // info to Homie
  if (queued_reason) {
    queued_reason = false;
    outletNode.setProperty("on").send(on? "true": "false");
    outletNode.setProperty("reason").send(reason);
  }

  if (queued_ton) {
    queued_ton = false;
    outletNode.setProperty("time-on").send(l_to_s(time_to_turn_on));
  }

  if (queued_toff) {
    queued_toff = false;
    outletNode.setProperty("time-off").send(l_to_s(time_to_turn_off));
  }

  if (queued_time_last_change) {
    queued_time_last_change = false;
    if (time_base > 0)
      outletNode.setProperty("time-last-change").send(l_to_s(time_last_change));
  }

  // Handle local button press
  if (rising) {
    rising = false;
    if (!buttonState) {
      buttonState = true;
      buttonNode.setProperty("button").send("true");
    }
  }
}

/*
 * This code runs repeatedly, whether connected to not.
 *
 * This code handles all scheduled events.
 * When not connected,it handles button press locally.
 * Notifications to the Homie system are queued to
 * be handled when connected.
 */
void loop() {
  long now;
  long t = millis();

  now = time_base + t/1000;

  // Process commands we received through Homie
  if (queued_remote_set) {
	queued_remote_set = false;
  	on = desired_remote_set;
	reason = REASON_REMOTE;
	queued_reason = true;
	queued_time_last_change = true;
	time_last_change = now;
  }

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
  // Note that if we get two rising edges before
  // Homie does anything with it, we assume we are not connected.
  debouncer.update(); // Update the Bounce instance
  if (debouncer.rose()) {
	if (!connected || rising) {
		rising = false;
		on = !on;
		reason = REASON_LOCAL;
		queued_reason = true;
		time_last_change = now;
		queued_time_last_change = true;
	} else
		rising = true;
  }

  // Push any local-mode changes in relay state to the hardware
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);

  // This section controls blinking our current state on the LED.
  if (t > led_can_change) {
    // blink 1 HZ, 5% cycle when connected, 0.2HZ 5% when not
    if (!connected)
    	t /= 5;
    digitalWrite(PIN_LED, ((t/50)%20 != 0)? HIGH: LOW); 
  }

  // Set connected = false here.  If we get to the 
  // Homie loop handler it will be set to true.
  connected = false;
  Homie.loop();
}
