//
// This project detects loss of Internet and tries to fix it
// by reseting the cable modem, which is attached to an external relay.
//
// Turning on (HIGH) the relay interrupts power to the cable modem
//

#include <ESP8266WiFi.h>
#include "passwords.h"

// pins
#define LED	LED_BUILTIN	// Note, LED goes on when this pin driven low.
#define	RELAY	D3		// Doesn't really matter which pin

unsigned long relay_off_time;
unsigned long poll_home;	// When to next poll the house
unsigned long poll_timeout;
unsigned long connecttime;
unsigned long now, previous_now;
unsigned char ToDKnown;		// True if we know the ToD.
unsigned long ToDBase;		// Number of seconds since epoc at boot or last wrap of millis.

#define	WRAP_TIME	(4294967ul)	// (2^32/1000) Number of seconds it takes millis() to wrap.
#define	POLL_PERIOD	10		// ten seconds for now.  After debug complete increase to 5 minutes.
#define	POLL_TIMEOUT	5		// seconds
#define SERVER_TIMEOUT	3		// seconds to server to respond to a successful message send.

#define	WIFI_NONE	0
#define	WIFI_CONNECTING	1
#define	WIFI_CONNECTED	2
unsigned char wifiState;

#define	ERROR_NONE	0
#define	ERROR_CLOSE	1
#define	ERROR_NOCLOSE	2
#define	ERROR_RESET	3

// #define HOME
#define LAKE
#ifdef HOME
const char wifiname[] = "DanielIOT";
const char wifipass[] = HOMEPASS;
const char servername[] = "192.168.1.13";
#endif
#ifdef LAKE
const char wifiname[] = "BesideThePoint";
const char wifipass[] = LAKEPASS;
const char servername[] = "ssh.swdaniel.com";
#endif
const int serverport = 1884;

WiFiClient client;
unsigned char poll_home_state;
unsigned char dialog_number;

void setup() {
  // Get the serial connection up and running
  Serial.begin(9600);
  while (!Serial) ;

  // Configure I/O pins
  pinMode(LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  // Various state variables
  relay_off_time = 0;
  wifiState = WIFI_NONE;
  connecttime = 0;
  now = millis();
  ToDKnown = 0;
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

static void mylog(const char *message) {
  // NYI
  Serial.print("Log message: ");
  Serial.println(message);
}

void loop() {
  long x;
  unsigned char error;
  String line;

  // Time Management
  previous_now = now;
  now = millis();
  if (ToDKnown && previous_now > now)	// Deal with millis() wrapping around
    ToDBase += WRAP_TIME;

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
        mylog("Connected To Wifi");
        poll_home = mark(POLL_PERIOD);
        dialog_number = 0;
      }
      break;
    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        // If the connection fails, wait 3 seconds before reconnecting.
        connecttime = mark(3);
        wifiState = WIFI_NONE;
        mylog("No WiFi");
        poll_home = 0;	// don't poll home
      }
      break;

  }

  // If we don't have any wifi, then nothing we do matters, so exit.
  if (wifiState != WIFI_CONNECTED)
    return;

  error = ERROR_NONE;
  // Time to poll home?
  switch (poll_home_state) {
    // State 0: not connect to home server
    case 0:
      if (dialog_number == 0 && future(poll_home))
        break;	// not yet time to poll the home server
      if (client.connect(servername, serverport)) {
        poll_home_state = 1;
        poll_timeout = mark(POLL_TIMEOUT);
      } else {
        // If we cannot connect, try again later
        error = ERROR_RESET;
      }
      break;
    // State 1: connection live, send message 1
    case 1:
      if (!future(poll_timeout)) {
        error = ERROR_CLOSE;
        mylog("connect timeout");
        break;
      }
      if (!client.connected()) {
        error = ERROR_CLOSE;
        mylog("connection dropped");
        break;
      }

      // We are connected, start the dialog
      switch (dialog_number) {
        case 0:
          client.print(String("reset,stillwater,modem,314159"));
      }
      poll_home_state = 2;
      poll_timeout = mark(SERVER_TIMEOUT);
      break;
    // State 2: Wait for a response
    case 2:
      if (!client.connected()) {
        // poll failed.
        error = ERROR_CLOSE;
        mylog("no response");
        break;
      }
      if (!future(poll_timeout)) {
        error = ERROR_CLOSE;
        mylog("server response timeout");
        break;
      }
      if (!client.available())
        break;	// stay in this state and wait for stuff from client

      line = client.readStringUntil('\n');
      client.stop();

      switch (dialog_number) {
        case 0:

          // Dialog 0 asks for the TOD
          // looking for x,yyy, where x is a single digit resonse code, and yyy is the ToD
          // in seconds since the epoch
          // x = 0: error, unrecognized message
          // x = 1: error, ToD not available
          // x = 9: success
          if (line.length() < 1) {
            error = ERROR_NOCLOSE;
            mylog("response too short");
            break;
          }
          x = line[0] - '0';
          if (x < 1 || x > 9) {
            error =  ERROR_NOCLOSE;
            mylog("reponse code invalid");
            break;
          }
          if (x == 1) {
            // Dialog complete
            error = ERROR_RESET;
            mylog("TOD not available");
            break;
          }
          if (line.length() < 3) {
            error = ERROR_NOCLOSE;
            mylog("response no payload");
            break;
          }
          if (x != 9) {
            error = ERROR_NOCLOSE;
            mylog("response code != 9");
            break;
          }
          x = line.substring(2).toInt();
          ToDBase = x - millis();
          ToDKnown = 1;
          poll_home_state = 0;
          dialog_number++;
          break;
        case 1:

          // Dialog 1 is used for sending log messages.
          // NYI
          error = ERROR_RESET;
          break;
        default:	// dialog number
          ;
      }
    default:
      ;
  }

  switch (error) {
    case ERROR_CLOSE:
      client.stop();
    case ERROR_NOCLOSE:
      mylog("error in dialog with server");
    case ERROR_RESET:
      poll_home_state = 0;
      poll_home = mark(POLL_PERIOD);
      dialog_number = 0;
    default:
      ;
  }
}
