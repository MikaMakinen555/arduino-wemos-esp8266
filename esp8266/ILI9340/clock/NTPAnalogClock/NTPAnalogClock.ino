/*
  Analog Clock Face with NTP with 1.8" TFT (ST7735) running on ESP8266
  NTP code extracted from TimeNTP_ESP8266Wifi.ino included in `Time` by Paul Stoffregen
  (c) 2015 Karl Pitrich, MIT Licensed

  Dependencies:
    * https://github.com/adafruit/Adafruit-GFX-Library.git
    * https://github.com/PaulStoffregen/Time.git
    * Depending on display:
      * https://github.com/nzmichaelh/Adafruit-ST7735-Library.git
      * https://github.com/0xPIT/Adafruit_ILI9340
*/

#include <functional>
#include <algorithm>

#include <TimeLib.h> 
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <SPI.h>
#include <Adafruit_GFX.h>

//#define ST7735
#define ILI9340

#if defined(ST7735)
#  include <Adafruit_ST7735.h>
#  define ColorPrimary ST7735_BLACK
#  define ColorBG ST7735_WHITE
#  define ColorRED ST7735_RED
#elif defined(ILI9340)
#  include "Adafruit_ILI9340.h"
#  define ColorPrimary ILI9340_BLACK
#  define ColorBG ILI9340_WHITE
#  define ColorRED ILI9340_RED
#endif

const float degToRad = 0.0174532925;  // 1° == 0.0174532925rad (PI/180) 

/*const char ssid[] = "...";            // network SSID
const char pass[] = "...";  // network password
*/


const char* ssid = "MikroTik-191AD3";
const char* pass = "zebra222";



const int timeZone = 1;               // Central European Time
const char* timerServerDNSName = "0.europe.pool.ntp.org";
IPAddress timeServer;

WiFiUDP Udp;
const unsigned int localPort = 8888;  // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;       // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];   // buffer to hold incoming & outgoing packets

typedef struct Point_s {
  int x;
  int y;
} Point_t;

//#define TFT_CS     15
//#define TFT_DC     2

#define TFT_CS D0   // GPIO 16 interne LED kein Port nötig, da TFT CS an GND
#define TFT_RST 10  // D12 GPIO10 //kein Port nötig, da Reset an Reset angeschlossen
#define TFT_DC D1   // GPIO 5
#define TFT_MOSI D7 // GPIO 13
#define TFT_CLK D5  // GPIO 14
#define TFT_MISO D6 // GPIO 12




#if defined(ST7735)
  Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC);
  Point_t displayCenter = { // 160x128 with rotation=3
    ST7735_TFTHEIGHT_18 / 2,
    ST7735_TFTWIDTH / 2
  };
#elif defined(ILI9340)
  Adafruit_ILI9340 tft = Adafruit_ILI9340(TFT_CS, TFT_DC, -1);
  Point_t displayCenter = { // 160x128 with rotation=3
    ILI9340_TFTHEIGHT / 2,
    ILI9340_TFTWIDTH / 2
  };
#endif

// radius of the clock face
uint16_t clockRadius = std::min(displayCenter.x, displayCenter.y) - 2;

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }

  Serial.println("No NTP response");
  return 0;
}

void drawHourTick(uint8_t hour, Point_t center, uint16_t radius)
{
  float radians = hour * PI / 6.0;

  tft.drawLine(
    center.x + (int)((0.91 * radius * sin(radians))),
    center.y - (int)((0.91 * radius * cos(radians))),
    center.x + (int)((1.00 * radius * sin(radians))),
    center.y - (int)((1.00 * radius * cos(radians))),
    ColorPrimary
  );
}

void drawMinuteDot(uint8_t minute, Point_t center, uint16_t radius)
{
    float radians = minute * PI / 30.0;

    if (minute % 15 == 0) {
      tft.fillCircle(
        center.x + (int)((0.91 * radius * sin(radians))),
        center.y - (int)((0.91 * radius * cos(radians))),
        2,
        ColorPrimary
      );
    }
    else {
      tft.drawPixel(
        center.x + (int)((0.91 * radius * sin(radians))),
        center.y - (int)((0.91 * radius * cos(radians))),
        ColorPrimary
      );
    }
}

void drawClockFace(uint16_t radius, Point_t center) {
  tft.fillScreen(ColorBG);
  tft.setRotation(3);

  for (uint8_t hr = 1; hr <= 12; hr++) {
    drawHourTick(hr, center, radius);
  }

  for (uint8_t min = 1; min <= 60; min++) {
    drawMinuteDot(min, center, radius);
  }
}

uint8_t lastH, lastM, lastS;

void drawClockHands(time_t now, uint16_t radius, Point_t center) {
  float radians;
  uint8_t hh = hour(now);
  uint8_t mm = minute(now);
  uint8_t ss = second(now);

  auto centerLine = [&](Point_t p, uint16_t c = ColorPrimary) {
    tft.drawLine(center.x, center.y, p.x, p.y, c);
  };
  
  auto makePoint = [&](float factor) {
    Point_t pt = {
      x: center.x + (uint16_t)(factor * radius * sin(radians)),
      y: center.y - (uint16_t)(factor * radius * cos(radians))
    };
    return std::move(pt);
  };

  // clear hands
  //tft.fillCircle(center.x, center.y, radius * 0.85, ColorBG);

  // hour
  radians = (lastH % 12) * PI / 6.0 + (PI * lastM / 360.0);
  Point_t pH = makePoint(0.5);
  centerLine(pH, ColorBG);
  radians = (hh % 12) * PI / 6.0 + (PI * mm / 360.0);
  pH = makePoint(0.5);
  centerLine(pH);

  // minute
  radians = (lastM * PI / 30.0) + (PI * lastS / 1800.0);
  Point_t pM = makePoint(0.7);
  centerLine(pM, ColorBG);
  radians = (mm * PI / 30.0) + (PI * ss / 1800.0);
  pM = makePoint(0.7);
  centerLine(pM);

  // second
  radians = lastS * PI / 30.0; 
  Point_t pS = makePoint(0.8);
  centerLine(pS, ColorBG);
  radians = ss * PI / 30.0; 
  pS = makePoint(0.8);
  centerLine(pS, ColorRED);

  lastH = hh;
  lastM = mm;
  lastS = ss;

  // center dot
  tft.fillCircle(center.x, center.y, 3, ColorRED);
}

void setup() 
{
  Serial.begin(115200); 
  while (!Serial);

#if defined(ST7735)
  tft.initR(INITR_BLACKTAB);
#elif defined(ILI9340)
  tft.begin();
#endif

  drawClockFace(clockRadius, displayCenter);

  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Resolving NTP Server IP ");
  WiFi.hostByName(timerServerDNSName, timeServer);
  Serial.println(timeServer.toString());

  Serial.print("Starting UDP... ");
  Udp.begin(localPort);
  Serial.print("local port: ");
  Serial.println(Udp.localPort());

  Serial.println("Waiting for NTP sync");
  setSyncProvider(getNtpTime);
}

void printDigits(Print *p, int digits) {
  p->print(":");
  if(digits < 10) {
    p->print('0');
  }
  p->print(digits);
}

void dumpClock(Print *p) {
  p->print(hour());
  printDigits(p, minute());
  printDigits(p, second());
  p->print(" ");
  p->print(day());
  p->print(".");
  p->print(month());
  p->print(".");
  p->print(year()); 
  p->println(); 
}

time_t prevTime = 0;

void loop()
{
  if (timeStatus() != timeNotSet) {
    time_t current = now();
    if (current != prevTime) {
      drawClockHands(current, clockRadius, displayCenter);
      prevTime = current;
      dumpClock(&Serial);
    }
  }
}

