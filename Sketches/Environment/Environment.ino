/*
 * Code to manage a WiFi controlled environment sensor
 * 
 * Inputs:
 *  Ambient LUX, temp, and humidity
 * Outputs:
 *  None
 *
 * Implements 3 homie sensors
 */
#include <Homie.h>

#define FIRMWARE_NAME     "env-sense"
#define FIRMWARE_VERSION  "0.1.0"


/*
 * IO Pins
 */
const int PIN_SCL = D1;
const int PIN_SDA = D2;
const int PIN_LED = 2;

/*
 * Misc globals
 */
long time_base;
long now;

HomieNode luxNode("lux", "sensor");

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
 *
 ****/

// Broadcast handler.  Useful for time base.
bool broadcastHandler(const String& level, const String& value) {
  // Only broadcast we know about it IOTtime.
  if (level == "IOTtime") {
	long t = value.toInt();

	if (t < 0) return false;

	time_base = t - millis()/1000;
	return true;
  }
  return false;
}

/*
 * This code called once to set up, but only after completely connected.
 */
void setupHandler() {
}

void setup() {
  void loopHandler();
  Serial.begin(115200);
  Serial.println("Lux sensor");
  Serial.println(FIRMWARE_VERSION);
  Serial << endl << endl;

  time_base = 0;

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  luxNode.advertise("lux");
  luxNode.advertise("time-last-update");

  Homie.setBroadcastHandler(broadcastHandler);

  Homie.setup();
}

/*
 * This code is called once per loop(), but only
 * when connected to WiFi and MQTT broker
 */
void loopHandler() {
}

/*
 * This code runs repeatedly, whether connected to not.
 */
void loop() {

  now = time_base + millis()/1000;

  Homie.loop();
}
