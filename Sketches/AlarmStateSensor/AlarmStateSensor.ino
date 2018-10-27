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
//

#include <Homie.h>

#define FIRMWARE_NAME     "alarm-state"
#define FIRMWARE_VERSION  "0.3.0"

// Note: all of these LEDs are on when LOW, off when HIGH
static const uint8_t PIN_LED0 = D4; // the WeMos blue LED
static const uint8_t PIN_LED1 = D3; // a white status LED
static const uint8_t PIN_LED2 = D8; // another white status LED


// These input pins are driven low by an open collector on the alarm.
// Active LOW, by default.  All inputs have external pullup resistors
// The alarm programming has to be figured out.  When it is, I'll
// update this comment with what the pins actually mean.

static const uint8_t PIN_INPUT17 = D0; // output #17 from alarm panel
static const uint8_t PIN_INPUT18 = D5; // output #18 from alarm panel
static const uint8_t PIN_DEBUG = D6;  // high for normal mode, low for debug mode.

// Blink control.
// State variables for the built-in LED
unsigned char blink_state;
int blink_last_time;
const int blink_time = 100; // milliseconds; half-period
const unsigned char blink_start = 11;  // set to 2*n+1 for n blinks
bool blinking;
unsigned char alarm_status;

bool debug_mode;

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
  // turn off all LEDs
  digitalWrite(PIN_LED0, HIGH);
  digitalWrite(PIN_LED1, HIGH);
  digitalWrite(PIN_LED2, HIGH);
}

//
// Read the alarm pins
// If they have change, tell the world.
//
void sensor() {
  unsigned char p17, p18;
  unsigned char s;

  p17 = !digitalRead(PIN_INPUT17);
  p18 = !digitalRead(PIN_INPUT18);
  s = 0;
  if (p17)
    s = 2;
  if (p18)
    s++;

  if (s != alarm_status) {
    alarm_status = s;
    alarmStateNode.setProperty("rawstate").send(String(alarm_status));
    alarmStateNode.setProperty("state").send("unknown");
  }
}

// This code handles turning on, blinking, and turning off the on-board LED.
void blinkHandler() {
  
  if (blinking && blink_state == 0) {
      lightNode.setProperty("on").send("false");
      blinking = false;
      digitalWrite(PIN_LED1, HIGH); // turn off
      digitalWrite(PIN_LED2, HIGH); // turn off
  }
  
  if ((blink_state & 1) == 0)
    digitalWrite(PIN_LED0, HIGH);  // turn it off
  else
    digitalWrite(PIN_LED0, LOW); // turn it on

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
  digitalWrite(PIN_LED0, LOW);
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);
  delay(2000);

  // set up the serial port
  Serial.begin(115200);
  Serial.println("Alarm State Sensor\n");

  // If we are commanded to debug, the skip _all_ the HOMIE stuff
  debug_mode = false;
  if (!digitalRead(PIN_DEBUG)) {
    debug_mode = true;
    return;
  }

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  // register the LED's control function
  lightNode.advertise("on").settable(lightOnHandler);
  alarmStateNode.advertise("state");
  alarmStateNode.advertiseRange("rawstate", 0, 3);
  
  Homie.setup();
}


void loop() {
  if (debug_mode) {
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
  Homie.loop();
}
