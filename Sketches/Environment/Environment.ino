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

#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <Homie.h>

#define FIRMWARE_NAME     "env-sense"
#define FIRMWARE_VERSION  "0.1.2"


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
long last_light_time;
long published_light_time;
int light;
const long light_period = 10;	// sample every 10 seconds

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);


HomieNode luxNode("lux", "sensor");

/* convert integer to string */
static char *l_to_s(long i)
{
	static char buf[36];

	itoa(i, buf, 10);
	return buf;
}

void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);          /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
     tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */
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
  published_light_time = 0;
  luxNode.setProperty("unit").send("lux");
}

void setup() {
  void loopHandler();
  Serial.begin(115200);
  Serial.println("Lux sensor");
  Serial.println(FIRMWARE_VERSION);
  Serial << endl << endl;

  configureSensor();
  last_light_time = 0;

  time_base = 0;

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  luxNode.advertise("lux");
  luxNode.advertise("unit");
  luxNode.advertise("time-last-update");

  Homie.setBroadcastHandler(broadcastHandler);

  Homie.setup();
}

/*
 * This routine takes ~ 100ms to run. (Integration time)
 * Returns how much light in lux.
 * A return value of zero indicates sensor is overloaded.  No data.
 */
static int getLight()
{
  sensors_event_t event;
  tsl.getEvent(&event);
  return event.light;
}

static void processLight()
{
  if (now - last_light_time >= light_period) {
    light = getLight();
    last_light_time = now;
  }
}

/*
 * This code is called once per loop(), but only
 * when connected to WiFi and MQTT broker
 */
void loopHandler() {
  // only publish sensor value if we've a new sample
  if (published_light_time == last_light_time)
    return;

  luxNode.setProperty("lux").send(String(light));
  luxNode.setProperty("time-last-update").send(String(last_light_time));
  published_light_time = last_light_time;
}

/*
 * This code runs repeatedly, whether connected to not.
 */
void loop() {

  now = time_base + millis()/1000;
  processLight();
  Homie.loop();
}
