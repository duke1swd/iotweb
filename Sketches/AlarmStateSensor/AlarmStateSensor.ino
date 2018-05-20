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

#include <Homie.h>

// Note: all of these LEDs are on when LOW, off when HIGH
static const uint8_t PIN_LED0 = D4; // the WeMos blue LED
static const uint8_t PIN_LED1 = D3; // a white status LED
static const uint8_t PIN_LED2 = D8; // another white status LED

// These input pins are driven low by an open collector on the alarm.
// Active LOW, by default.
// The alarm programming has to be figured out.  When it is, I'll
// update this comment with what the pins actually mean.

static const uint8_t PIN_INPUT17 = D0; // output #17 from alarm panel
static const uint8_t PIN_INPUT18 = D5; // output #18 from alarm panel

// Blink control.
// State variables for the built-in LED
unsigned char blink_state;
int blink_last_time;
const int blink_time = 100; // milliseconds; half-period
const unsigned char blink_start = 11;  // set to 2*n+1 for n blinks
bool blinking;
unsigned char alarm_status;

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
  pinMode(PIN_INPUT17, INPUT_PULLUP);
  pinMode(PIN_INPUT18, INPUT_PULLUP);

  // set up the serial port
  Serial.begin(115200);
  Serial.println("Alarm State Sensor\n");

  Homie_setFirmware("alarm-state", "0.2.0");
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  // register the LED's control function
  lightNode.advertise("on").settable(lightOnHandler);
  alarmStateNode.advertise("state");
  alarmStateNode.advertiseRange("rawstate", 0, 3);
  
  Homie.setup();
}


void loop() {
  Homie.loop();
}
