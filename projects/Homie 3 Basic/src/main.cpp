#include <Arduino.h>
/*
 * Playpen for learning Homie stuff
 * Starting with version 0.2 this is based on the Homie 3 code.
 */
#include <Homie.h>

#define FIRMWARE_NAME     "Simple LED Control"
#define FIRMWARE_VERSION  "0.2.0"

/*
 * IO Pins
 */
const int PIN_LED = 2;
#define	LED_ON_VALUE	LOW		// pull the pin low to turn on the LED
#define	LED_OFF_VALUE	HIGH

/*
 * Global State
 */
int on;	// 0 = off, 1 = on, 2 = blinking, 3 = pausing
#define	OFF		0
#define	ON		1
#define	BLINKING	2
#define	PAUSING		3
#define	BLINK_ON_TIME	100	// in milliseconds
#define	BLINK_OFF_TIME	700
#define	PAUSE_TIME	2000

int blinks; // if blinking, how many to do
int bcnt; // if blinking how many half phases left in string
int timeNext; // how many milliseconds until we do something

unsigned long lastMillis;

HomieNode ledNode("led", "simpleLedControl", "switch");

/*
 * The value coming in is an integer, represented as a string.
 * If the value is <= 0, LED is off.
 * If the value >= 10, LED is on.
 * Otherwise, emit that many blinks, pause, repeat.
 */
bool ledOnHandler(const HomieRange& range, const String& value) {
  for (byte j = 0; j < value.length(); j++) {
    if (isDigit(value.charAt(j)) == false) return false;
  }

  const unsigned long numericValue = value.toInt();

  timeNext = 0;
  if (numericValue <= 0) {
  	on = OFF;
  } else if (numericValue >= 10) {
   	on = ON;
  } else {
  	on = BLINKING;
	blinks = numericValue;
	bcnt = blinks * 2 - 1;
	timeNext = BLINK_ON_TIME;
  }

  //digitalWrite(PIN_LED, on ? LED_ON_VALUE : LED_OFF_VALUE);
  ledNode.setProperty("on").send(value);
  Homie.getLogger() << "LED is " << (on ? "on" : "off") << endl;
  switch (on) {
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
  Serial.begin(115200);
  Serial << endl << endl;
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LED_OFF_VALUE);
  on = OFF;

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);

  ledNode.advertise("on").settable(ledOnHandler)
                         .setName("LED On")
			 .setDatatype("int");

  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  Homie.setup();

  lastMillis = millis();
}

void loop() {
  unsigned long now;

  Homie.loop();
  now = millis();
  switch (on) {
  case ON:
	digitalWrite(PIN_LED, LED_ON_VALUE);
	break;
  case OFF:
	digitalWrite(PIN_LED, LED_OFF_VALUE);
	break;
  case BLINKING:
	if (bcnt & 1) 
		digitalWrite(PIN_LED, LED_ON_VALUE);
	else
		digitalWrite(PIN_LED, LED_OFF_VALUE);
	break;
  case PAUSING:
	digitalWrite(PIN_LED, LED_OFF_VALUE);
	break;
  default:
	if (now & 0x400) 
		digitalWrite(PIN_LED, LED_ON_VALUE);
	else
		digitalWrite(PIN_LED, LED_OFF_VALUE);
	break;
  }
  if (timeNext > 0 && lastMillis != now) {
  	timeNext--;
	lastMillis = now;
  }
  if (timeNext == 0) switch (on) {
  	case BLINKING:
		bcnt--;
		if (bcnt == 0) {
			on = PAUSING;
			timeNext = PAUSE_TIME;
		} else if (bcnt & 1)
			timeNext = BLINK_ON_TIME;
		else
			timeNext = BLINK_OFF_TIME;
		break;
	case PAUSING:
		bcnt = blinks * 2 - 1;
		on = BLINKING;
		timeNext = BLINK_ON_TIME;
		break;
  }
}
