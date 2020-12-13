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
const int PIN_LED_1 = D1;
const int PIN_LED_2 = D2;
#define	LED_ON_VALUE	LOW		// pull the pin low to turn on the LED
#define	LED_OFF_VALUE	HIGH

int ledpin[N_LEDS] = {PIN_LED, PIN_LED_1, PIN_LED_2};

/*
 * Global State
 */
int on[N_LEDS];	// 0 = off, 1 = on, 2 = blinking, 3 = pausing
#define	OFF		0
#define	ON		1
#define	BLINKING	2
#define	PAUSING		3
#define	BLINK_ON_TIME	100	// in milliseconds
#define	BLINK_OFF_TIME	700
#define	PAUSE_TIME	2000

int blinks[N_LEDS]; // if blinking, how many to do
int bcnt[N_LEDS]; // if blinking how many half phases left in string
int timeNext[N_LEDS]; // how many milliseconds until we do something

unsigned long lastMillis[N_LEDS];

HomieNode ledNode("led", "simpleLedControl", "switch", true, 0, N_LEDS-1);

/*
 * The value coming in is an integer, represented as a string.
 * If the value is <= 0, LED is off.
 * If the value >= 10, LED is on.
 * Otherwise, emit that many blinks, pause, repeat.
 */
bool ledOnHandler(const HomieRange& range, const String& value) {
  int i;
  if (!range.isRange)
	  return false;				// If it isn't a range, then give up.

  if (range.index < 0 || range.index >= N_LEDS)
  	return false;				// If range is out of range ignore it.
  i = range.index;

  for (byte j = 0; j < value.length(); j++) {
    if (isDigit(value.charAt(j)) == false)
	    return false;			// If the value field isn't a number, ignore it.
  }

  unsigned long numericValue = value.toInt();	// OK, what is the value?

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

  //digitalWrite(PIN_LED, on ? LED_ON_VALUE : LED_OFF_VALUE);
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
  }

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);

  ledNode.advertise("on").settable(ledOnHandler)
                         .setName("LED On")
			 .setDatatype("int");

  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  Homie.setup();
}

void loop() {
  unsigned long now;
  int i;

  Homie.loop();
  now = millis();
  for (i = 0; i < N_LEDS; i++) {
	switch (on[i]) {
	case ON:
		digitalWrite(ledpin[i], LED_ON_VALUE);
		break;
	case OFF:
		digitalWrite(ledpin[i], LED_OFF_VALUE);
		break;
	case BLINKING:
		if (bcnt[i] & 1) 
			digitalWrite(ledpin[i], LED_ON_VALUE);
		else
			digitalWrite(ledpin[i], LED_OFF_VALUE);
		break;
	case PAUSING:
		digitalWrite(ledpin[i], LED_OFF_VALUE);
		break;
	default:
		if (now & 0x400) 
			digitalWrite(ledpin[i], LED_ON_VALUE);
		else
			digitalWrite(ledpin[i], LED_OFF_VALUE);
		break;
	}
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
