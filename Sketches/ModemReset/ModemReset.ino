//
// This project detects loss of Internet and tries to fix it
// by reseting the cable modem, which is attached to an external relay.
//
// Turning on (HIGH) the relay interrupts power to the cable modem
//

#include <ESP8266WiFi.h>

// pins
#define LED	LED_BUILTIN	// Note, LED goes on when this pin driven low.
#define	RELAY	D3		// Doesn't really matter which pin

unsigned long relay_off_time;
unsigned long connecttime;
unsigned long now;

#define	WIFI_NONE	0
#define	WIFI_CONNECTING	1
#define	WIFI_CONNECTED	2
unsigned char wifiState;

const char wifiname[] = "DanielIOT";
const char wifipass[] = "+dt8ynpFJQ;eu_tbhN";

void setup() {
  // Get the serial connection up and running
  Serial.begin(9600);
  while (!Serial) ;

  // Configure I/O pins
  pinMode(LED, OUTPUT);
  pinMode(RELAY, LOW);

  // Various state variables
  relay_off_time = 0;
  wifiState = WIFI_NONE;
  connecttime = 0;
}

/*
   True if the argument is a time in the future.
   Works even in the event of time rollovers.
   Time rollovers happen about every 50 days.

   Do not use this form times more than half of max time (about 24 days)
   in the future.
*/
#define	HALFMAXTIME	(0x80000000ul)

int future(long t) {
  if (t == 0)
    return 0;
  if (t - millis() < HALFMAXTIME)
    return 1;
  // If the result is to far out in the future, then
  // we really have a negative number, so its in the past.
  return 0;
}

// return a time 't' seconds in the future.
// If that time would be zero, make it one more millisecond.
unsigned long mark(unsigned long t) {
  unsigned long r;

  r = now + t * 1000;
  if (r == 0)
    r = 1;
  return r;
}


void loop() {

  now = millis();

  // Turn on the relay by setting a time in the future when it should go off.

  if (future(relay_off_time)) {
    digitalWrite(RELAY, HIGH);
    digitalWrite(LED, LOW);
  } else {
    digitalWrite(RELAY, LOW);
    digitalWrite(LED, HIGH);
    relay_off_time = 0;
  }

  // Manage the wifi connection
  switch (wifiState) {
    case WIFI_NONE:
      if (!future(connecttime)) {
        WiFi.begin(wifiname, wifipass);
        wifiState = WIFI_CONNECTING;
        connecttime = mark(10);	// no more than 1 attempt every 10 seconds
      }
      break;
    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
          wifiState = WIFI_CONNECTED;
          Serial.println("Connected To Wifi");
      }
      break;
    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        // If the connection fails, wait 3 seconds before reconnecting.
        connecttime = mark(3);
        wifiState = WIFI_NONE;
      }
      break;

  }
  if (wifiState != WIFI_CONNECTED)
    return;
}
