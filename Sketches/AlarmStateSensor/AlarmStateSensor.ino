//
// This is the firmware for the alarm state sensor.
// Primary job is to sense the state of the alarm:
//    off
//    stay -- alarm is on but people or dogs are home
//    away -- alarm is on, including interior motion sensor
// The state of the alarm is reflected as a persistent message
//  in MQTT.  This can then be used for various purposes.
//
// For version 0.2 we reflect the raw alarm state.  This is two bits,
// sent as a number 0-3.  The MSB is alarm output 17, the LSB
// is alarm output 18
//
// Version 0.3 adds debug mode, to help us debug the LED hardware
// and the alarm software, it simply copies the inputs to the LED outputs.
//  Hold Pin D6 low at power up to enable debug
//  Hold an input pin low to light up the corresponding LED
//

#include <Homie.h>

#define FIRMWARE_NAME     "alarm-state"
#define FIRMWARE_VERSION  "0.4.1"

// Note: all of these LEDs are on when LOW, off when HIGH
static const uint8_t PIN_LED0 = D4; // the WeMos blue LED
static const uint8_t PIN_LED1 = D1; // a white status LED
static const uint8_t PIN_LED2 = D7; // another white status LED


// These input pins are driven low by an open collector on the alarm.
// Active LOW, by default.  All inputs have external pullup resistors
// The alarm programming has to be figured out.  When it is, I'll
// update this comment with what the pins actually mean.

static const uint8_t PIN_INPUT17 = D2; // output #17 from alarm panel
static const uint8_t PIN_INPUT18 = D5; // output #18 from alarm panel
static const uint8_t PIN_DEBUG = D6;  // high for normal mode, low for debug mode.

// Blink control.
// State variables for the built-in LED
unsigned char blink_state;
long blink_last_time;
const long blink_time = 100; // milliseconds; half-period
const unsigned char blink_start = 11;  // set to 2*n+1 for n blinks
bool blinking;
unsigned char alarm_status;
unsigned char cooked_alarm_status;

/*
 * Stuff for handling decode the alarm state
 */

// Possible states of the P18 decoder
#define	S_idle_low	0
#define	S_h1		1
#define	S_h2		2
#define	S_h3		3
#define	S_h4		4
#define	S_two_sec	5
#define	S_high		6
#define	S_l1		7
#define	S_l2		8
#define	S_l3		9
#define	N_STATES	10

// Possible cooked states
#define	P_Disarmed	0
#define	P_Off		1
#define	P_High		2
#define	P_Pulse		3
#define	P_Two_Sec	4

// If P17 is on, these are the alarm state names
const char *cooked_alarm_states[] = {
	"disarmed",
	"armed-stay",
	"alarmed-burglar",
	"alarmed-fire",
	"armed-away",
};

/*
 * P18 decoder state machine.  Input is [current_state][i] where
 	i = 0 when p18 is 0
	i = 1 when p18 is 1
	i = 2 when timeout happens
 */
const unsigned char p18_state_table[N_STATES][3] = {
	{S_idle_low, S_h1, S_idle_low},		// S_idle_low
	{S_h2, S_h1, S_h4},			// S_h1
	{S_h2, S_h3, S_idle_low},		// S_h2
	{S_h2, S_h3, S_h4},			// S_h3
	{S_two_sec, S_h4, S_high},		// S_h4
	{S_two_sec, S_h1, S_two_sec},		// S_two_sec
	{S_l1, S_high, S_high},			// S_high
	{S_l1, S_l2, S_idle_low},		// S_l1
	{S_h2, S_l2, S_l3},			// S_l2
	{S_two_sec, S_l3, S_high},		// S_l3
};

// P18 state output values
const unsigned char p18_state_output[N_STATES] = {
	P_Off,
	P_Two_Sec,
	P_Pulse,
	P_Pulse,
	P_Two_Sec,
	P_Two_Sec,
	P_High,
	P_High,
	P_High,
	P_High,
};


bool debug_mode;
long last_debug_report_time;

// The LED is an output node, provides external control of the blue LED on the Wemos D1
HomieNode lightNode("led", "switch");   // ID is "led", which is unique within this device.  Type is "switch"

// The ALARM is an input node, which tells the world the state of the alarm panel.
HomieNode alarmStateNode("alarm-state", "sensor");

//
// When you turn the LED on, it blinks for awhile, then turns off.
bool lightOnHandler(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;

  bool on = (value == "true");
  if (on) {
    blink_state = blink_start;
    blink_last_time = millis();
    blinking = true;
    digitalWrite(PIN_LED1, LOW); // turn on
    digitalWrite(PIN_LED2, LOW); // turn on
  } else {
    blink_state = 0;
    blinking = false;
    digitalWrite(PIN_LED1, HIGH); // turn off
    digitalWrite(PIN_LED2, HIGH); // turn off
  }
  lightNode.setProperty("on").send(value);
  Homie.getLogger() << "Alarm State Sensor LED set " << (on ? "on" : "off") << endl;

  return true;
}

/*
 * This code is called once, after Homie is in normal mode and we are connected
 * to the MQTT broker.
 */
void setupHandler() {
  blink_state = 0;
  alarm_status = 0xff;
  cooked_alarm_status = 0xff;
  // turn off all LEDs
  digitalWrite(PIN_LED0, LOW);
  digitalWrite(PIN_LED1, HIGH);
  digitalWrite(PIN_LED2, HIGH);
}

//
// P18 state machine stuff
//
unsigned char p18_current_state;
long p18_state_enter_time;		// set to zero for no pending timeout
const long p18_timeout = 1100;		// timeouts occur after 1.1 seconds

void p18_reset()
{
	p18_current_state = S_idle_low;
	p18_state_enter_time = 0;
}

void p18_machine(unsigned char p18)
{
	long now;

	if (p18)
		p18 = 1;
	now = millis();
	if (p18_state_enter_time > 0 &&
	    now - p18_state_enter_time >= p18_timeout) {
	    	p18 = 2;
		p18_state_enter_time = 0;
	}
	p18_current_state = p18_state_table[p18_current_state][p18];
}

//
// Read the alarm pins
// If they have change, tell the world.
//
void sensor() {
  unsigned char p17, p18;
  unsigned char s, c;

  p17 = !digitalRead(PIN_INPUT17);
  p18 = !digitalRead(PIN_INPUT18);

  // Send the raw alarm pin status to Homie
  s = 0;
  if (p17)
    s = 2;
  if (p18)
    s++;

  if (s != alarm_status) {
    alarm_status = s;
    alarmStateNode.setProperty("rawstate").send(String(alarm_status));
  }

  // Calculate the cooked alarm status and send it to Homie
  if (!p17) {
  	p18_reset();
	c = P_Disarmed;
  } else {
  	p18_machine(p18);
	c = p18_state_output[p18_current_state];
  }

  if (c != cooked_alarm_status) {
    alarmStateNode.setProperty("state").send(cooked_alarm_states[c]);
    cooked_alarm_status = c;
  }
}

// This code handles turning on, blinking, and turning off the on-board LED.
void blinkHandler() {
  
  if (blinking && blink_state == 0) {
      lightNode.setProperty("on").send("false");
      lightNode.setProperty("on/set").send("false");
      blinking = false;
      digitalWrite(PIN_LED1, HIGH); // turn off
      digitalWrite(PIN_LED2, HIGH); // turn off
  }
  
  if ((blink_state & 1) == 0)
    digitalWrite(PIN_LED0, LOW);  // turn it off
  else
    digitalWrite(PIN_LED0, HIGH); // turn it on

  if (blink_state > 0 && millis() - blink_last_time > blink_time) {
    blink_last_time += blink_time;
    blink_state--;
  } 
}

//
// This code is called once per loop(), but only
// when connected to WiFi and MQTT broker
//
void loopHandler() {
  sensor();
  blinkHandler();
}

void setup() {
  // Set up the I/O pins
  pinMode(PIN_LED0, OUTPUT);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_INPUT17, INPUT);
  pinMode(PIN_INPUT18, INPUT);
  pinMode(PIN_DEBUG, INPUT);

  // turn on all LEDs for at least 2 seconds
  // once we enter normal operation setupHandler() will turn these back off
  digitalWrite(PIN_LED0, HIGH);
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);
  delay(2000);
  digitalWrite(PIN_LED0, LOW);

  // set up the serial port
  Serial.begin(74880);
  Serial.println("Alarm State Sensor");
  Serial.println(FIRMWARE_VERSION);

  // If we are commanded to debug, the skip _all_ the HOMIE stuff
  debug_mode = false;
  if (!digitalRead(PIN_DEBUG)) {
    Serial.println("Entering debug mode");
    debug_mode = true;
    last_debug_report_time = 0;
    return;
  }
  Serial.println("Entering normal mode");

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  // register the LED's control function
  lightNode.advertise("on").settable(lightOnHandler);
  alarmStateNode.advertise("state");
  alarmStateNode.advertiseRange("rawstate", 0, 3);
  Homie.disableLedFeedback(); // we want to control the LED
  
  Homie.setup();
}


void loop() {
  int v;
  
  /*
   * DEBUG MODE
   */
  if (debug_mode) {
    if (millis() - last_debug_report_time > 1000) {
      last_debug_report_time = millis();
      v = digitalRead(PIN_INPUT17);
      Serial.print(v);
      v = digitalRead(PIN_INPUT18);
      Serial.println(v);
    }
    // in debug mode if you ground one of the input lines we turn on
    // the corresponding LED
    if (digitalRead(PIN_INPUT17))
      digitalWrite(PIN_LED1, HIGH);
    else
      digitalWrite(PIN_LED1, LOW);
    if (digitalRead(PIN_INPUT18))
      digitalWrite(PIN_LED2, HIGH);
    else
      digitalWrite(PIN_LED2, LOW);
    return;
  }

  /*
   * NORMAL MODE
   */
  Homie.loop();
}
