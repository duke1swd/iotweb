#include <Arduino.h>
/*
 * This is the firmware for a wall mounted status box that
 * has two LEDs to display
 */
#include <Homie.h>

#define FIRMWARE_NAME     "Two LED Control"
#define FIRMWARE_VERSION  "0.1.0"

#define	N_LEDS	3			// There are 3, the internal and 2 external

/*
 * IO Pins
 */
const int PIN_LED = 2;
const int PIN_LED_1 = D5;
const int PIN_LED_2 = D6;
#define	LED_ON_VALUE	LOW		// pull the pin low to turn on the LED
#define	LED_OFF_VALUE	HIGH

/*
 * Per LED State Variables
 */
unsigned char on[N_LEDS];	// 0 = off, 1 = on, 2 = blinking, 3 = pausing
#define	OFF		0
#define	ON		1
#define	BLINKING	2
#define	PAUSING		3
#define	BLINK_ON_TIME	100	// in milliseconds
#define	BLINK_OFF_TIME	700
#define	PAUSE_TIME	2000

int ledpin[N_LEDS] = {PIN_LED, PIN_LED_1, PIN_LED_2};
unsigned char pwm_capable[N_LEDS] = {0, 1, 1};
unsigned char blinks[N_LEDS]; // if blinking, how many to do
unsigned char bcnt[N_LEDS]; // if blinking how many half phases left in string
int timeNext[N_LEDS]; // how many milliseconds until we do something
unsigned long lastMillis[N_LEDS];
unsigned char intensity[N_LEDS];
unsigned char intensity_override[N_LEDS];
unsigned char changed[N_LEDS];
unsigned char state[N_LEDS];

/*
 * We have only one node, and it is a range node for the set of LEDs we control.
 */

HomieNode ledNode("led", "simpleLedControl", "switch", true, 0, N_LEDS-1);

/*
 * The value coming in is an integer, represented as a string.
 * If the value is <= 0, LED is off.
 * If the value >= 10, LED is on.
 * Otherwise, emit that many blinks, pause, repeat.
 */
bool ledIntensityHandler(const HomieRange& range, const String& value) {
  int i;
  unsigned long numericValue;

  if (!range.isRange)
	  return false;				// If it isn't a range, then give up.

  if (range.index < 0 || range.index >= N_LEDS)
  	return false;				// If range is out of range ignore it.
  i = range.index;

  for (byte j = 0; j < value.length(); j++) {
    if (isDigit(value.charAt(j)) == false)
	    return false;			// If the value field isn't a number, ignore it.
  }

  numericValue = value.toInt();			// OK, what is the value?
  if (numericValue > 255)
  	return false;				// can't be negative, as we rejected input with '-' in it.
  
  if (numericValue != intensity[i]) {
  	intensity[i] = numericValue;
	changed[i] = 1;
	ledNode.setProperty("intensity").setRange(i).send(value);
  }

  return true;
}

/*
 * The value coming in is an integer, represented as a string.
 * If the value is <= 0, LED is off.
 * If the value >= 10, LED is on.
 * Otherwise, emit that many blinks, pause, repeat.
 */
bool ledIntensityOverrideHandler(const HomieRange& range, const String& value) {
  int i;
  unsigned long numericValue;

  if (!range.isRange)
	  return false;				// If it isn't a range, then give up.

  if (range.index < 0 || range.index >= N_LEDS)
  	return false;				// If range is out of range ignore it.
  i = range.index;

  for (byte j = 0; j < value.length(); j++) {
    if (isDigit(value.charAt(j)) == false)
	    return false;			// If the value field isn't a number, ignore it.
  }

  numericValue = value.toInt();			// OK, what is the value?
  if (numericValue > 255)
  	return false;				// can't be negative, as we rejected input with '-' in it.
  
  if (numericValue != intensity_override[i]) {
  	intensity_override[i] = numericValue;
	changed[i] = 1;
	ledNode.setProperty("intensity-override").setRange(i).send(value);
  }

  return true;
}

/*
 * The value coming in is an integer, represented as a string.
 * If the value is <= 0, LED is off.
 * If the value >= 10, LED is on.
 * Otherwise, emit that many blinks, pause, repeat.
 */
bool ledOnHandler(const HomieRange& range, const String& value) {
  int i;
  unsigned long numericValue;

  if (!range.isRange)
	  return false;				// If it isn't a range, then give up.

  if (range.index < 0 || range.index >= N_LEDS)
  	return false;				// If range is out of range ignore it.
  i = range.index;

  for (byte j = 0; j < value.length(); j++) {
    if (isDigit(value.charAt(j)) == false)
	    return false;			// If the value field isn't a number, ignore it.
  }

  numericValue = value.toInt();			// OK, what is the value?

  timeNext[i] = 0;
  if (numericValue <= 0) {
  	on[i] = OFF;
  } else if (numericValue >= 10) {
   	on[i] = ON;
  } else {
  	on[i] = BLINKING;
	blinks[i] = numericValue;
	bcnt[i] = blinks[i] * 2 - 1;
	timeNext[i] = BLINK_ON_TIME;
  }

  ledNode.setProperty("on").setRange(i).send(value);
  Homie.getLogger() << "LED is " << (on[i] ? "on" : "off") << endl;
  switch (on[i]) {
	case OFF:
	  Homie.getLogger() << "LED is off\n";
	  break;
	case ON:
	  Homie.getLogger() << "LED is on\n";
	  break;
	default:
	  Homie.getLogger() << "LED is blinking\n";
  }

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
  int i;
  unsigned long now;

  Serial.begin(115200);
  Serial << endl << endl;

  now = millis();
  for (i = 0; i < N_LEDS; i++) {
	pinMode(ledpin[i], OUTPUT);
	digitalWrite(ledpin[i], LED_OFF_VALUE);
	on[i] = OFF;
	lastMillis[i] = now;
	intensity[i] = 255;
	intensity_override[i] = 0;
	changed[i] = 0;
	state[i] = 0;
  }

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);

  ledNode.advertise("on").settable(ledOnHandler)
                        .setName("LED On")
			.setDatatype("int");
  ledNode.advertise("intensity").settable(ledIntensityHandler)
  			.setName("LED Intensity")
			.setDatatype("int");
  ledNode.advertise("intensity-override").settable(ledIntensityOverrideHandler)
  			.setName("LED Intensity Override")
			.setDatatype("int");

  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  Homie.setup();
}

/*
 * Turn on the LED at the right intensity.  If the led is
 * already on and the intensity has not changed, do not make
 * another call to analogWrite().

 * Note that since driving the pin low turns on the LED we
 * need to use 255-i as intensity i.

 * If the LED is not on a PWM capable pin, just set it on.
 */
void ledOn(int i) {
	unsigned char c_intensity;

	c_intensity = intensity[i];
	if (c_intensity < intensity_override[i])
		c_intensity = intensity_override[i];
	if (c_intensity < 1)
		c_intensity = 1;

	if (c_intensity == 255 || !pwm_capable[i]) 
		digitalWrite(ledpin[i], LED_ON_VALUE);
	else if (state[i] == 0 || changed[i])
		analogWrite(ledpin[i], 4*(255-c_intensity));

	state[i] = 1;
}

void ledOff(int i) {
	digitalWrite(ledpin[i], LED_OFF_VALUE);
	state[i] = 0;
}

void loop() {
  unsigned long now;
  int i;

  Homie.loop();
  now = millis();
  for (i = 0; i < N_LEDS; i++) {
	switch (on[i]) {
	case ON:
		ledOn(i);
		break;
	case OFF:
		ledOff(i);
		break;
	case BLINKING:
		if (bcnt[i] & 1) 
			ledOn(i);
		else
			ledOff(i);
		break;
	case PAUSING:
		ledOff(i);
		break;
	default:
		if (now & 0x400) 
			ledOn(i);
		else
			ledOff(i);
		break;
	}
	changed[i] = 0;
	if (timeNext[i] > 0 && lastMillis[i] != now) {
		timeNext[i]--;
		lastMillis[i] = now;
	}
	if (timeNext[i] == 0) switch (on[i]) {
		case BLINKING:
			bcnt[i]--;
			if (bcnt[i] == 0) {
				on[i] = PAUSING;
				timeNext[i] = PAUSE_TIME;
			} else if (bcnt[i] & 1)
				timeNext[i] = BLINK_ON_TIME;
			else
				timeNext[i] = BLINK_OFF_TIME;
			break;
		case PAUSING:
			bcnt[i] = blinks[i] * 2 - 1;
			on[i] = BLINKING;
			timeNext[i] = BLINK_ON_TIME;
			break;
	}
  }
}
