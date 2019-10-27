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
#include <DHT.h>
#include <DHT_U.h>
#include <Homie.h>

#define FIRMWARE_NAME     "env-sense"
#define FIRMWARE_VERSION  "1.0.7"


/*
 * IO Pins
 */
const int PIN_SCL = D1;
const int PIN_SDA = D2;
const int PIN_LED = 2;
const int PIN_DHT = D5;

/*
 * Misc globals
 */
volatile long time_base;
long now;
long last_light_time;
long published_light_time;
int light;
const long light_period = 10;	// sample every 10 seconds
float temp;
float humidity;
long last_temp_time;
long published_temp_time;
const long temp_period = 10;	// sample every 10 seconds

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
#define DHTTYPE DHT22   // DHT 22  (AM2302) type of temp/humidity sensor we are using
DHT dht(PIN_DHT, DHTTYPE);

HomieNode luxNode("lux", "sensor");
HomieNode tempNode("temp", "sensor");
HomieNode humidityNode("humidity", "sensor");

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
  published_temp_time = 0;
  luxNode.setProperty("unit").send("lux");
  tempNode.setProperty("unit").send("F");
}

void setup() {
  void loopHandler();
  Serial.begin(115200);
  Serial.println("Lux/Temp/RH sensors");
  Serial.println(FIRMWARE_VERSION);
  Serial << endl << endl;

  configureSensor();
  light = 0;
  last_light_time = 0;
  dht.begin();
  temp = 0.;
  humidity = 0.;
  last_temp_time = 0;

  time_base = 0;

  Homie_setFirmware(FIRMWARE_NAME, FIRMWARE_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  luxNode.advertise("lux");
  luxNode.advertise("unit");
  luxNode.advertise("time-last-update");
  tempNode.advertise("temp");
  tempNode.advertise("unit");
  tempNode.advertise("time-last-update");
  humidityNode.advertise("humidity");
  humidityNode.advertise("time-last-update");
  

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

static float getTemp()
{
  return dht.readTemperature(true);
}

static float getHumidity()
{
  return dht.readHumidity();
}

// returns true if we actually read the sensor
static bool processLight()
{
  if (now - last_light_time >= light_period) {
/*XXX*/if (1) {delay(240);light = 998;}else	// test to see if the delay function is the culprit
    light = getLight();
    last_light_time = now;
    return true;
  }
  return false;
}

static void processTH()
{
  if (now - last_temp_time >= temp_period) {
    temp = getTemp();
    humidity = getHumidity();
    last_temp_time = now;
  }
}


/*
 * This code is called once per loop(), but only
 * when connected to WiFi and MQTT broker
 */
void loopHandler() {
  // only publish sensor value if we've a new sample
  if (published_light_time != last_light_time) {
    luxNode.setProperty("lux").send(String(light));
    if (time_base)
      luxNode.setProperty("time-last-update").send(String(last_light_time));
    published_light_time = last_light_time;
  }
  if (published_temp_time != last_temp_time && !isnan(temp) && !isnan(humidity)) {
    tempNode.setProperty("temp").send(String(temp));
    if (time_base)
      tempNode.setProperty("time-last-update").send(String(last_temp_time));
    humidityNode.setProperty("humidity").send(String(humidity));
    humidityNode.setProperty("time-last-update").send(String(last_temp_time));
    published_temp_time = last_temp_time;
  }
}

/*
 * This code runs repeatedly, whether connected to not.
 */
void loop() {

#ifdef maybe_a_bug
  now = time_base + millis()/1000;
#else
  long t;

  t = millis()/1000;
  noInterrupts();
  now = time_base + t;
  interrupts();
#endif


  // because these sensors take a while to read never read
  // both of them on the same iteration of loop().
  if (!processLight())
    processTH();
  Homie.loop();
}
