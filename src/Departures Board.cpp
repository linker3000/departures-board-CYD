/*
 * Departures Board (c) 2025 Gadec Software
 *
 * https://github.com/gadec-uk/departures-board
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/
 *
 * ESP32 "Mini" Board with 3.12" 256x64 OLED Display Panel with SSD1322 controller on-board.
 *
 * OLED PANEL     ESP32 MINI
 * 1 VSS          GND
 * 2 VCC_IN       3.3V
 * 4 D0/CLK       IO18
 * 5 D1/DIN       IO23
 * 14 D/C#        IO5
 * 16 CS#         IO26
 *
 */

// Release version number
#define VERSION_MAJOR 1
#define VERSION_MINOR 0

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "HTTPUpdateGithub.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <weatherClient.h>
#include <raildataXmlClient.h>
#include <githubClient.h>

#include <time.h>

#include <SPI.h>
#include <U8g2lib.h>

#define msDay 86400000 // 86400000 milliseconds in a day
#define msHour 3600000 // 3600000 milliseconds in an hour
#define msMin 60000 // 60000 milliseconds in a second

WebServer server(80);     // Hosting the Web GUI
File fsUploadFile;        // File uploads

// Shorthand for response formats
const char contentTypeJson[] PROGMEM = "application/json";
const char contentTypeText[] PROGMEM = "text/plain";
const char contentTypeHtml[] PROGMEM = "text/html";

// Using NTP to set and maintain the clock
const char* ntpServer = "europe.pool.ntp.org";
struct tm timeinfo;


// Local firmware updates via /update Web GUI
const char* updatePage =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<html><body><p>Firmware Upgrade - upload a <b>firmware.bin</b></p>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('Progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script></body></html>";

// /upload page
const char uploadPage[] PROGMEM =
"<html><body>"
"<h2>Upload a file to the file system</h2><form method='post' enctype='multipart/form-data'><input type='file' name='name'>"
"<input class='button' type='submit' value='Upload'></form></body></html>";

#define SCREEN_WIDTH 256 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define DIMMED_BRIGHTNESS 1 // OLED display brightness level when in sleep/screensaver mode

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 26, /* dc=*/ 5, /* reset=*/ U8X8_PIN_NONE);

// Vertical line positions on the OLED display
#define LINE0 0
#define LINE1 13
#define LINE2 28
#define LINE3 41
#define LINE4 55

//
// Define the fonts we use. These are exact copies of those used on the real National Rail Station Boards
//
const uint8_t NatRailSmall9[973] U8G2_FONT_SECTION("NatRailSmall9") =
  "a\0\3\2\3\4\2\5\5\7\11\0\0\11\0\11\2\1\71\2r\3\264 \5\0\63\5!\7\71\245"
  "\304\240\4\42\7\33-Eb\11#\16=\245M)I\6\245\62(\245$\1$\14=\245U\266\324\266"
  "$\331\42\0%\14<eE\244H\221\24)R\0&\15=\245\215T\222\222DJ\242H\11'\6\31"
  "\255\304\0(\10;%UR\252\25)\11;%EV\252\224\0*\12-\247ERY,K\3+\12"
  "-\247U\30\15R\30\1,\7\32\341L\242\0-\6\13+\305\0.\6\11\245D\0/\13<e]"
  "$ER$e\0\60\12=\245\315\222yK\26\0\61\10\273\245M\42u\31\62\12=\245\315\222\205Y"
  "\333 \63\14=\245\315\222\205\221\252%\13\0\64\14=\245]&%\245d\320\302\4\65\13=\245\305q"
  "HC-Y\0\66\14=\245\315\222\211C\222i\311\2\67\12=\245\305 f\305&\0\70\14=\245\315"
  "\222i\311\222i\311\2\71\14=\245\315\222i\311\20j\311\2:\6!\247D\24;\7*\345L\252\0"
  "<\10<e]\324\330\0=\10\34i\305\20\16\1>\10<eE\330\324\6?\14=\245\315\222\205\221"
  "\226C\21\0@\14=\245\315\222Y\22eH\27\0A\13=\245\315\222i\303\220\331\2B\15=\245\305"
  "\220d\332\240d\332\240\0C\12=\245\315\222\211m\311\2D\12=\245\305\220d\336\6\5E\13=\245"
  "\305\61\34\222\60\34\4F\12=\245\305\61\34\222\260\10G\14=\245\315\222\211\311\220i\311\2H\12="
  "\245Ef\33\206\314\26I\10;%\305\22u\31J\11=\245eG-Y\0K\15=\245E&%%"
  "-\211*Y\0L\10=\245E\330\343 M\12=\245E\266,\211\346\26N\13=\245E\66)\211\264"
  "\331\2O\12=\245\315\222yK\26\0P\14=\245\305\220d\332\240\204E\0Q\12M\241\315\222yK"
  "\306\64R\15=\245\305\220d\332\240\224*Y\0S\13=\245\315\222\251\253\226,\0T\11=\245\305 "
  "\205=\1U\11=\245E\346[\262\0V\12=\245E\346-\251E\0W\12=\245E\346\222(\311-"
  "X\13=\245E\246%\265JM\13Y\12=\245E\246%\265\260\11Z\12=\245\305 f\35\7\1["
  "\7:\345\304\322E\134\12<eE\246eZ\246\5]\7:e\206\322e^\6\23/M\3_\6\14"
  "c\305\20`\6\22\357D\24a\12-\245\315\232\14Z\62\4b\13=\245EX\61i\332\240\0c\11"
  "-\245\315 V\207\0d\12=\245e\305\264i\311\20e\12-\245\315\222\15C:\4f\12<eU"
  "\245\64e%\0g\14=\241\315\240\331\222!\34\24\0h\12=\245EX\61i\266\0i\10;%M"
  "(\265\14j\13La]\16dmR\242\0k\13<eEVR\22))\5l\10;%\205\324\313"
  "\0m\12-\245M\27%\321\264\0n\11-\245Eb\322l\1o\11-\245\315\222\331\222\5p\14="
  "\241\305\220d\266A\11C\0q\12=\241\315\240\331\222!,r\11-\245Eb\22\213\0s\11-\245"
  "\315\240\36\24\0t\12<eM\26\15IV\24u\11-\245E\346\244(\1v\12-\245EfKj"
  "\21\0w\13-\245E\246$J\242t\1x\12-\245E\226\324*\265\0y\13=\241E\346\226\14\341"
  "\240\0z\11-\245\305\240\265\15\2{\17G#\206\266,=E\26\65\311\222d\11|\6\71\245\304!"
  "}\6\15\253\305 ~\15>\345\315\220\204\306\341!\211\22\0\15=\245E\222U\212Q\22%J\1"
  "\200\11$k\215\22I\211\2\0\0\0";

const uint8_t NatRailTall12[1035] U8G2_FONT_SECTION("NatRailTall12") =
  "`\0\3\2\3\4\2\5\5\7\14\0\375\11\375\11\0\1K\2\225\3\362 \5\0s\5!\7I\241"
  "\304!\11\42\7\23/E\242\4#\21M\241M)I\6\245\224DI\62(\245$\1$\17M\241U"
  "\266\224\222lK\242$\331\42\0%\12M\241\345\64e\235\216\0&\20M\241\215T\211\222(\222\222D"
  "J\242H\11'\6\21\257\204\0(\10K!UR\352V)\11K!EV\352R\2*\13=\243U"
  "\245\262X\226\246\10+\12-\245U\30\15R\30\1,\7\32\335L\242\0-\6\13)\305\0.\6\21"
  "\241\204\0/\11M\241e[\307\42\0\60\12M\241\315\222\371-Y\0\61\10\313\241M\42\365e\62\13"
  "M\241\315\222\205\265\216\203\0\63\14M\241\315\222\205%\65\324\222\5\64\16M\241]&%\245$J\6"
  "-\254\0\65\14M\241\305\61\34\322\60\324\222\5\66\15M\241\315\222\211\341\220d\266d\1\67\14M\241"
  "\305 \326\302,\314\302\14\70\15M\241\315\222\331\222%\263%\13\0\71\15M\241\315\222\331\222!\14\265"
  "d\1:\6)\245\204\42;\10\62\343L-Q\0<\10<c]\324\330\0=\10\34g\305\20\16\1"
  ">\10<cE\330\324\6?\14M\241\315\222\205\265b\16E\0@\16M\241\315\222Y\22%Q\206\60"
  "]\0A\13M\241\315\222\331\206!s\13B\15M\241\305\220d\266A\311l\203\2C\13M\241\315\222"
  "\211\275%\13\0D\13M\241\305\220d~\33\24\0E\13M\241\305\261\70$aq\20F\13M\241\305"
  "\261\70$a#\0G\14M\241\315\222\211\245\315\226,\0H\13M\241E\346\66\14\231[\0I\6I"
  "\241\304\3J\11M\241e\37\265d\1K\16M\241E\230III\323\222\250\222\5L\11M\241E\330"
  "\217\203\0M\14M\241E\266,\211\222h\336\2N\15M\241E\66MJ\242$\322M\13O\12M\241"
  "\315\222\371-Y\0P\14M\241\305\220d\266A\11\33\1Q\13]\235\315\222\371-\31\323\0R\15M"
  "\241\305\220d\266A)U\62-S\14M\241\315\222\251jQK\26\0T\11M\241\305 \205\375\4U"
  "\11M\241E\346o\311\2V\12M\241E\346\267\244\26\1W\13M\241E\346K\242$\267\0X\16M"
  "\241E\246%\245$\253DIM\13Y\14M\241E\246%\245$\13;\1Z\13M\241\305 \326:\206"
  "\203\0[\10J\341\304\322\27\1\134\10M\241EX\355X]\10J\341\204\322\227\1^\6\23/M\3"
  "_\6\15\237\305 `\6\22\357D\24a\13\65\241\315\232\14\232\226\14\1b\14M\241E\330b\322\264"
  "IQ\0c\11\65\241\315 \266\16\1d\13M\241e\213i\63)J\0e\14\65\241\315\222\15C\230"
  "%\13\0f\13La\225\222EC\222u\2g\15M\233\215i\63)J\250%\13\0h\12M\241E"
  "\330b\322\334\2i\7A\241D\62\14j\13\134[]\16d\275I\211\2k\15M\241E\330\224\224\264"
  "$\252d\1l\6I\241\304\3m\15\65\241\205\322\242$J\242$J\1n\11\65\241Eb\322\334\2"
  "o\12\65\241\315\222\271%\13\0p\15M\233Eb\322\264IQ\302\42\0q\13M\233\215i\63)J"
  "\330\0r\11\65\241Eb\22\33\1s\12\65\241\315\222\256Z\262\0t\12DaM\26\15I\326(u"
  "\11\65\241E\346IQ\2v\14\65\241E\246%\245$\13#\0w\15\65\241E\222(\211\222(\211\322"
  "\5x\13\65\241E\226\324\302,\251\5y\14M\233E\346IQB-Y\0z\12\65\241\305 fm"
  "\203\0{\21O!N\226h\313\322SdQ\223,I\226\0|\6I\241\305\3}\17^\233]\234\14"
  "c\35\13\303a\211\63\0~\21N\341\315\220\14C(\16\17\212\62\14I\224\0\16N\341E\22f"
  "J\224fI\226\364\37\0\0\0";

const uint8_t NatRailClockSmall7[137] U8G2_FONT_SECTION("NatRailClockSmall7") =
  "\12\0\3\3\3\3\3\1\5\7\7\0\0\7\0\7\0\0\0\0\0\0p\60\12?\343Td\274I*"
  "\0\61\10\274\343HF\272\20\62\13?\343TdRIE*=\63\14?\343TdR\331\230&\251\0"
  "\64\14?\343\315H\22%\311Q*\1\65\13?c\34\244f)MR\1\66\14?\343TdT\213\214"
  "&\251\0\67\11?c\134\205Z\325\0\70\15?\343Td\64IEF\223T\0\71\14?\343Td\64"
  "\211\225&\251\0\0\0\0";

const uint8_t NatRailClockLarge9[177] U8G2_FONT_SECTION("NatRailClockLarge9") =
  "\13\0\4\3\4\4\3\2\5\11\11\0\0\11\0\11\0\0\0\0\0\0\230\60\13\231T\307\205\24\277\222"
  "\270\0\61\11\224W\207\304\210\276 \62\16\231T\307\205\224\234\212\13\71u\7\2\63\17\231T\307\205\224"
  "\234\232B\71*\211\13\0\64\23\231T\327\24\221\204\214\210\32\11!\211\3\61\71\11\0\65\20\231T\307"
  "\301\234\334A\240\34\25\225\304\5\0\66\20\231T\307\205\24\235\334A\204\24+\211\13\0\67\14\231T\303"
  "\201\234\252Ur\32\1\70\17\231T\307\205\24+\211\13)V\22\27\0\71\20\231T\307\205\24+\211\203"
  "\70\71*\211\13\0:\7r\235\2\31\1\0\0\0";

//
// GitHub Client for firmware updates
//  - Pass a GitHub token if updates are to be loaded from a private repository
//
github ghUpdate("");

// Bit and bobs
unsigned long timer = 0;
bool isSleeping = false;            // Is the screen sleeping (showing the "screensaver")
bool sleepEnabled = false;          // Is overnight sleep enabled?
bool dateEnabled = false;           // Showing the date on screen?
bool weatherEnabled = false;        // Showing weather at station location. Requires an OpenWeatherMap API key.
bool enableBus = false;             // Include Bus services on the board?
bool firmwareUpdates = true;        // Check for and install firmware updates automatically at boot?
byte sleepStarts = 0;               // Hour at which the overnight sleep (screensaver) begins
byte sleepEnds = 6;                 // Hour at which the overnight sleep (screensaver) ends
int brightness = 50;                // Initial brightness level of the OLED screen
int waCurrentMajor = 0;             // Holds the major version number of the webapp code
int waCurrentMinor = 0;             // Holds the minor version number of the webapp code
unsigned long lastWiFiReconnect=0;  // Last WiFi reconnection time (millis)
bool firstLoad = true;              // Are we loading for the first time (no station config)?
int prevProgressBarPosition=0;      // Used for progress bar smooth animation
bool wifiConnected = false;         // Connected to WiFi?
unsigned long nextDataUpdate = 0;   // Next National Rail update time (millis)
int dataLoadSuccess = 0;            // Count of successful data downloads
int dataLoadFailure = 0;            // Count of failed data downloads
unsigned long lastLoadFailure = 0;  // When the last failure occurred

// Default network hostname
char hostname[20] = "Departures Board";
char myUrl[24];

// WiFi Manager status
bool wifiConfigured = false;        // Is WiFi configured successfully?

// Station Board Data
char nrToken[37];                   // National Rail Darwin Lite Tokens are in the format nnnnnnnn-nnnn-nnnn-nnnn-nnnnnnnnnnnn, where each 'n' represents a hexadecimal character (0-9 or a-f).
char crsCode[4];                    // Station code (3 character)
float stationLat=0;                 // Selected station Latitude/Longitude (used to get weather for the location)
float stationLon=0;

// Coach class availability
const char* firstClassSeating = " First class seating only.";
const char* standardClassSeating = " Standard class seating only.";
const char* dualClassSeating = " First and Standard class seating available.";

int numMessages=0;
int scrollStopsXpos = 0;
int scrollStopsYpos = 0;
int scrollStopsLength = 0;
bool isScrollingStops = false;
int currentMessage = 0;
int prevMessage = 0;
int prevScrollStopsLength = 0;
char line2[4+MAXBOARDMESSAGES][MAXMESSAGESIZE+12]; // All the possible messages to display

// Line 3 (additional services)
int line3Service = 0;
int scrollServiceYpos = 0;
bool isScrollingService = false;
int prevService = 0;
bool isShowingVia=false;
unsigned long serviceTimer=0;
unsigned long viaTimer=0;

char displayedTime[29] = "";        // The currently displayed time
unsigned long nextClockUpdate = 0;  // Next time we need to check/update the clock display
int fpsDelay=25;                    // Total ms between text movement (for smooth animation)
unsigned long refreshTimer = 0;

#define SCREENSAVERINTERVAL 10000   // How often the screen is changed in sleep mode (ms - 10 seconds)
#define DATAUPDATEINTERVAL 150000   // How often we fetch data from National Rail (ms - 2.5 mins)

// Weather Stuff
char weatherMsg[46];                            // Current weather at station location
unsigned long nextWeatherUpdate = 0;            // When the next weather update is due
String openWeatherMapApiKey = "";               // The API key to use
weatherClient currentWeather;                   // Create a weather client

bool noDataLoaded = true;                       // True if no data received for that station
int lastUpdateResult = 0;                       // Result of last data refresh
unsigned long lastDataLoadTime = 0;             // How long it took to load and process the last refresh

#define MAXHOSTSIZE 48                          // Maximum size of the wsdl Host
#define MAXAPIURLSIZE 48                        // Maximum size of the wsdl url

char wsdlHost[MAXHOSTSIZE];                     // wsdl Host name
char wsdlAPI[MAXAPIURLSIZE];                    // wsdl API url

// RailData XML Client
raildataXmlClient raildata;                     // Create a raildata Client
rdStation station;                              // Holds station data

/*
 * Graphics helper functions for OLED panel
*/
void blankArea(int x, int y, int w, int h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x,y,w,h);
  u8g2.setDrawColor(1);
}

int getStringWidth(const char *message) {
  return u8g2.getStrWidth(message);
}

int getStringWidth(const __FlashStringHelper *message) {
  String temp = String(message);
  char buff[temp.length()+1];
  temp.toCharArray(buff,sizeof(buff));

  return u8g2.getStrWidth(buff);
}

void centreText(const char *message, int line) {
  int width = u8g2.getStrWidth(message);
  if (width<SCREEN_WIDTH) u8g2.drawStr((SCREEN_WIDTH-width)/2,line,message); else u8g2.drawStr(0,line,message);
}

void centreText(const __FlashStringHelper *message, int line) {
  String temp = String(message);
  char buff[temp.length()+1];
  temp.toCharArray(buff,sizeof(buff));
  int width = u8g2.getStrWidth(buff);
  if (width<SCREEN_WIDTH) u8g2.drawStr((SCREEN_WIDTH-width)/2,line,buff); else u8g2.drawStr(0,line,buff);
}

void drawProgressBar(int percent) {
  int newPosition = (percent*190)/100;
  u8g2.drawFrame(32,36,192,12);
  if (prevProgressBarPosition>newPosition) {
    for (int i=prevProgressBarPosition;i>=newPosition;i--) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(33,37,190,10);
      u8g2.setDrawColor(1);
      u8g2.drawBox(33,37,i,10);
      u8g2.updateDisplayArea(0,3,32,3);
      delay(5);
    }
  } else {
    for (int i=prevProgressBarPosition;i<=newPosition;i++) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(33,37,190,10);
      u8g2.setDrawColor(1);
      u8g2.drawBox(33,37,i,10);
      u8g2.updateDisplayArea(0,3,32,3);
      delay(5);
    }
  }
  prevProgressBarPosition=newPosition;
}

void progressBar(const char *text, int percent) {
  u8g2.setFont(NatRailSmall9);
  blankArea(0,24,256,25);
  centreText(text,24);
  drawProgressBar(percent);
}

void progressBar(const __FlashStringHelper *text, int percent) {
  u8g2.setFont(NatRailSmall9);
  blankArea(0,24,256,25);
  centreText(text,24);
  drawProgressBar(percent);
}

void drawBuildTime() {
  char timestamp[22];
  char buildtime[20];
  struct tm tm = {};

  sprintf(timestamp,"%s %s",__DATE__,__TIME__);
  strptime(timestamp,"%b %d %Y %H:%M:%S",&tm);
  sprintf(buildtime,"v%d.%d-%02d%02d%02d%02d%02d",VERSION_MAJOR,VERSION_MINOR,tm.tm_year-100,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min);
  u8g2.drawStr(0,53,buildtime);
}

void drawStartupHeading() {
  u8g2.setFont(NatRailTall12);
  centreText(F("Departures Board"),0);
  u8g2.setFont(NatRailSmall9);
  drawBuildTime();
}

// Draw the NR clock (if the time has changed)
void drawCurrentTime(bool update) {
  char sysTime[29];
  getLocalTime(&timeinfo);

  sprintf(sysTime,"%02d:%02d:",timeinfo.tm_hour,timeinfo.tm_min);
  if (strcmp(displayedTime,sysTime)) {
    u8g2.setFont(NatRailClockLarge9);
    blankArea(96,LINE4,64,SCREEN_HEIGHT-LINE4);
    u8g2.drawStr(96,LINE4-1,sysTime);
    u8g2.setFont(NatRailClockSmall7);
    sprintf(sysTime,"%02d",timeinfo.tm_sec);
    u8g2.drawStr(144,LINE4+1,sysTime);
    u8g2.setFont(NatRailSmall9);
    if (update) u8g2.updateDisplayArea(12,6,8,2);
    strcpy(displayedTime,sysTime);
  }
}

// Screensaver Screen
void drawSleepingScreen() {
  char sysTime[8];
  char sysDate[29];

  u8g2.setContrast(DIMMED_BRIGHTNESS);
  u8g2.clearBuffer();
  getLocalTime(&timeinfo);
  sprintf(sysTime,"%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min);
  strftime(sysDate,29,"%d %B %Y",&timeinfo);

  int offset = (getStringWidth(sysDate)-getStringWidth(sysTime))/2;
  u8g2.setFont(NatRailTall12);
  int y = random(39);
  int x = random(SCREEN_WIDTH-getStringWidth(sysDate));
  u8g2.drawStr(x+offset,y,sysTime);
  u8g2.setFont(NatRailSmall9);
  u8g2.drawStr(x,y+13,sysDate);
  u8g2.sendBuffer();
  firstLoad=true;
}

void showUpdateIcon(bool show) {
  if (show) {
    u8g2.setFont(NatRailTall12);
    u8g2.drawStr(250,50,"}");
    u8g2.setFont(NatRailSmall9);
  } else {
    blankArea(250,50,6,13);
  }
  u8g2.updateDisplayArea(31,6,1,2);
}

/*
 * Setup / Notification Screen Layouts
*/
void showSetupScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(NatRailTall12);
  centreText(F("Departures Board first-time setup"),0);
  u8g2.setFont(NatRailSmall9);
  centreText(F("To configure Wi-Fi, please connect to the"),18);
  centreText(F("the \"Departures Board\" network and go to"),32);
  centreText(F("http://192.168.4.1 in a web browser."),46);
  u8g2.sendBuffer();
}

void showNoDataScreen() {
  u8g2.clearBuffer();
  char msg[60];
  u8g2.setFont(NatRailTall12);
  sprintf(msg,"No data available for station code \"%s\"",crsCode);
  centreText(msg,-1);
  u8g2.setFont(NatRailSmall9);
  centreText(F("Please ensure you have selected a valid station"),14);
  centreText(F("Go to the URL below to choose a station..."),26);
  centreText(myUrl,40);
  u8g2.sendBuffer();
}

void showSetupKeysHelpScreen() {
  u8g2.clearBuffer();
  char msg[60];
  u8g2.setFont(NatRailTall12);
  centreText(F("Departures Board Setup"),-1);
  u8g2.setFont(NatRailSmall9);
  centreText(F("Next, you need to enter your API keys."),16);
  centreText(F("Please go to the URL below to start..."),28);
  u8g2.setFont(NatRailTall12);
  centreText(myUrl,50);
  u8g2.sendBuffer();
}

void showSetupCrsHelpScreen() {
  u8g2.clearBuffer();
  char msg[60];
  u8g2.setFont(NatRailTall12);
  centreText(F("Departures Board Setup"),-1);
  u8g2.setFont(NatRailSmall9);
  centreText(F("Next, you need to choose a location. Please"),16);
  centreText(F("go to the URL below to choose a station..."),28);
  u8g2.setFont(NatRailTall12);
  centreText(myUrl,50);
  u8g2.sendBuffer();
}

void showWsdlFailureScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(NatRailTall12);
  centreText(F("The National Rail data feed is unavailable."),-1);
  u8g2.setFont(NatRailSmall9);
  centreText(F("WDSL entry point could not be accessed, so the"),14);
  centreText(F("Departures Board cannot be loaded."),26);
  centreText(F("Please try again later. :("),40);
  u8g2.sendBuffer();
}

void showTokenErrorScreen() {
  char msg[60];
  u8g2.clearBuffer();
  u8g2.setFont(NatRailTall12);
  centreText(F("Access to the National Rail database denied."),-1);
  u8g2.setFont(NatRailSmall9);
  centreText(F("You must enter your personal auth token, please"),14);
  centreText(F("check you have entered it correctly below:"),26);
  sprintf(msg,"%s/keys.htm",myUrl);
  centreText(msg,40);
  u8g2.sendBuffer();
  strcpy(nrToken,"");
}

void showCRSErrorScreen() {
  u8g2.clearBuffer();
  char msg[60];
  u8g2.setFont(NatRailTall12);
  sprintf(msg,"The station code \"%s\" is not valid.",crsCode);
  centreText(msg,-1);
  u8g2.setFont(NatRailSmall9);
  centreText(F("Please ensure you have selected a valid station."),14);
  centreText(F("Go to the URL below to choose a station..."),26);
  centreText(myUrl,40);
  u8g2.sendBuffer();
}

void showFirmwareUpdateWarningScreen(const char *msg, int secs) {
  char countdown[60];
  u8g2.clearBuffer();
  u8g2.setFont(NatRailTall12);
  centreText(F("Firmware Update Available"),-1);
  u8g2.setFont(NatRailSmall9);
  centreText(F("A new version of the Departures Board firmware"),14);
  sprintf(countdown,"will be installed in %d seconds. This provides:",secs);
  centreText(countdown,26);
  sprintf(countdown,"\"%s\"",msg);
  centreText(countdown,40);
  centreText(F("* DO NOT REMOVE THE POWER DURING THE UPDATE *"),54);
  u8g2.sendBuffer();
}

void showFirmwareUpdateProgress(int percent) {
  u8g2.clearBuffer();
  u8g2.setFont(NatRailTall12);
  centreText(F("Firmware Update in Progress"),-1);
  u8g2.setFont(NatRailSmall9);
  progressBar(F("Updating Firmware"),percent);
  centreText(F("* DO NOT REMOVE THE POWER DURING THE UPDATE *"),54);
  u8g2.sendBuffer();
}

void showWebappUpdateProgress(int percent) {
  u8g2.clearBuffer();
  u8g2.setFont(NatRailTall12);
  centreText(F("Web GUI Update in Progress"),-1);
  u8g2.setFont(NatRailSmall9);
  progressBar(F("Updating Files"),percent);
  centreText(F("* DO NOT REMOVE THE POWER DURING THE UPDATE *"),54);
  u8g2.sendBuffer();
}

void showUpdateCompleteScreen(const char *title, const char *msg1, const char *msg2, int secs, bool showReboot) {
  char countdown[60];
  u8g2.clearBuffer();
  u8g2.setFont(NatRailTall12);
  centreText(title,-1);
  u8g2.setFont(NatRailSmall9);
  centreText(msg1,14);
  centreText(msg2,26);
  if (showReboot) sprintf(countdown,"The system will restart in %d seconds.",secs);
  else sprintf(countdown,"The system will continue in %d seconds.",secs);
  centreText(countdown,40);
  u8g2.sendBuffer();
}

/*
 * Utility functions
*/

// Saves a file (string) to the FFS
bool saveFile(String fName, String fData) {
  File f = LittleFS.open(fName,"w");
  if (f) {
    f.println(fData);
    f.close();
    return true;
  } else return false;
}

// Get the Build Timestamp of the running firmware
String getBuildTime() {
  char timestamp[22];
  char buildtime[11];
  struct tm tm = {};

  sprintf(timestamp,"%s %s",__DATE__,__TIME__);
  strptime(timestamp,"%b %d %Y %H:%M:%S",&tm);
  sprintf(buildtime,"%02d%02d%02d%02d%02d",tm.tm_year-100,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min);
  return String(buildtime);
}

// Check if the clock needs to be updated
void doClockCheck() {
  if (!firstLoad) {
    if (millis()>nextClockUpdate) {
      drawCurrentTime(true);
      nextClockUpdate=millis()+500;
    }
  }
}

// Returns true if sleep mode is enabled and we're within the sleep period
bool isSnoozing() {
  if (!sleepEnabled) return false;
  getLocalTime(&timeinfo);
  byte myHour = timeinfo.tm_hour;
  if (sleepStarts > sleepEnds) {
    if ((myHour >= sleepStarts) || (myHour < sleepEnds)) return true; else return false;
  } else {
    if ((myHour >= sleepStarts) && (myHour < sleepEnds)) return true; else return false;
  }
}

// Callback from the raildataXMLclient library when processing data. As this can take some time, this callback is used to try and keep the clock working
// and to provide progress on the initial load at boot
void raildataCallback(int stage, int nServices) {
  if (firstLoad) {
    int percent = ((nServices*20)/MAXBOARDSERVICES)+80;
    progressBar(F("Initialising National Rail interface"),percent);
  } else doClockCheck();
}

// Stores/updates the url of our Web GUI
void updateMyUrl() {
  IPAddress ip = WiFi.localIP();
  snprintf(myUrl,sizeof(myUrl),"http://%u.%u.%u.%u",ip[0],ip[1],ip[2],ip[3]);
}

/*
 * Start-up configuration functions
 */

// Load the API keys from the file system (if they exist)
void loadApiKeys() {
  JsonDocument doc;

  if (LittleFS.exists(F("/apikeys.json"))) {
    File file = LittleFS.open(F("/apikeys.json"), "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings["nrToken"].is<const char*>()) {
          strlcpy(nrToken, settings["nrToken"], sizeof(nrToken));
        }

        if (settings["owmToken"].is<const char*>()) {
          openWeatherMapApiKey = settings["owmToken"].as<String>();
        }
      } else {
        // JSON deserialization failed
      }
      file.close();
    }
  }
}

// Load the configuration settings (if they exist, if not create a default set for the Web GUI page to read)
void loadConfig() {
  JsonDocument doc;

  if (LittleFS.exists(F("/config.json"))) {
    File file = LittleFS.open(F("/config.json"), "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings["crs"].is<const char*>())        strlcpy(crsCode, settings["crs"], sizeof(crsCode));
        if (settings["hostname"].is<const char*>())   strlcpy(hostname, settings["hostname"], sizeof(hostname));
        if (settings["wsdlHost"].is<const char*>())   strlcpy(wsdlHost, settings["wsdlHost"], sizeof(wsdlHost));
        if (settings["wsdlAPI"].is<const char*>())    strlcpy(wsdlAPI, settings["wsdlAPI"], sizeof(wsdlAPI));
        if (settings["showDate"].is<bool>())          dateEnabled = settings["showDate"];
        if (settings["showBus"].is<bool>())           enableBus = settings["showBus"];
        if (settings["sleep"].is<bool>())             sleepEnabled = settings["sleep"];
        if (settings["weather"].is<bool>() && openWeatherMapApiKey.length())
                                                    weatherEnabled = settings["weather"];
        if (settings["update"].is<bool>())            firmwareUpdates = settings["update"];
        if (settings["sleepStarts"].is<int>())        sleepStarts = settings["sleepStarts"];
        if (settings["sleepEnds"].is<int>())          sleepEnds = settings["sleepEnds"];
        if (settings["brightness"].is<int>())         brightness = settings["brightness"];
        if (settings["lat"].is<float>())              stationLat = settings["lat"];
        if (settings["lon"].is<float>())              stationLon = settings["lon"];
      } else {
        // JSON deserialization failed
      }
      file.close();
    }
  } else {
    // Write a default config file so that the Web GUI works initially
    String defaultConfig = "{\"crs\":\"\",\"station\":\"\",\"lat\":0,\"lon\":0,\"weather\":" + String((openWeatherMapApiKey.length())?"true":"false") + F(",\"sleep\":false,\"showDate\":false,\"showBus\":false,\"update\":true,\"sleepStarts\":23,\"sleepEnds\":8,\"brightness\":20}");
    saveFile("/config.json",defaultConfig);
    strcpy(crsCode,"");
  }

  // Load the webapp version if possible
  if (LittleFS.exists(F("/webapp.ver"))) {
    File f = LittleFS.open(F("/webapp.ver"),"r");
    if (f) {
      waCurrentMajor = f.readStringUntil('\n').toInt();
      waCurrentMinor = f.readStringUntil('\n').toInt();
      f.close();
    }
  }
}

// WiFiManager callback, entered config mode
void wmConfigModeCallback (WiFiManager *myWiFiManager) {
  showSetupScreen();
  wifiConfigured = true;
}

/*
 * Station Board functions - pulling updates and animating the Departures Board main display
 */

// Request a data update via the raildataClient
bool getStationBoard() {
  if (!firstLoad) showUpdateIcon(true);
  lastUpdateResult = raildata.updateDepartures(&station,crsCode,nrToken,MAXBOARDSERVICES,enableBus);
  nextDataUpdate = millis()+DATAUPDATEINTERVAL; // default update freq
  if (lastUpdateResult == UPD_SUCCESS || lastUpdateResult == UPD_NO_CHANGE) {
    showUpdateIcon(false);
    lastDataLoadTime=millis();
    noDataLoaded=false;
    dataLoadSuccess++;
    return true;
  } else if (lastUpdateResult == UPD_DATA_ERROR) {
    lastLoadFailure=millis();
    dataLoadFailure++;
    nextDataUpdate = millis() + 30000; // 30 secs
    showUpdateIcon(false);
    return false;
  } else if (lastUpdateResult == UPD_UNAUTHORISED) {
    showTokenErrorScreen();
    while (true) { server.handleClient(); yield();}
  } else {
    showUpdateIcon(false);
    return false;
  }
}

// Draw the primary service line
void drawPrimaryService(bool showVia) {
  int destPos;
  char clipDestination[MAXLOCATIONSIZE];
  char etd[16];

  u8g2.setFont(NatRailTall12);
  blankArea(0,LINE1,256,LINE2-LINE1);
  destPos = u8g2.drawStr(0,LINE1-1,station.service[0].sTime) + 6;
  if (strlen(station.service[0].platform) && strlen(station.service[0].platform)<3 && station.service[0].serviceType == TRAIN) {
    destPos += u8g2.drawStr(destPos,LINE1-1,station.service[0].platform) + 6;
  } else if (station.service[0].serviceType == BUS) {
    destPos += u8g2.drawStr(destPos,LINE1-1,"~") + 6; // Bus icon
  }
  if (isDigit(station.service[0].etd[0])) sprintf(etd,"Exp %s",station.service[0].etd);
  else strcpy(etd,station.service[0].etd);
  int etdWidth = getStringWidth(etd);
  u8g2.drawStr(SCREEN_WIDTH - etdWidth,LINE1-1,etd);
  // Space available for destination name
  int spaceAvailable = SCREEN_WIDTH - destPos - etdWidth - 6;
  if (showVia) strcpy(clipDestination,station.service[0].via);
  else strcpy(clipDestination,station.service[0].destination);
  if (getStringWidth(clipDestination) > spaceAvailable) {
    while (getStringWidth(clipDestination) > (spaceAvailable - 8)) {
      clipDestination[strlen(clipDestination)-1] = '\0';
    }
    // check if there's a trailing space left
    if (clipDestination[strlen(clipDestination)-1] == ' ') clipDestination[strlen(clipDestination)-1] = '\0';
    strcat(clipDestination,"...");
  }
  u8g2.drawStr(destPos,LINE1-1,clipDestination);
  // Set font back to standard
  u8g2.setFont(NatRailSmall9);
}

// Draw the secondary service line
void drawServiceLine(int line, int y) {
  char clipDestination[30];
  char ordinal[5];

  switch (line) {
    case 1:
      strcpy(ordinal,"2nd ");
      break;
    case 2:
      strcpy(ordinal,"3rd ");
      break;
    case 3:
      strcpy(ordinal,"4th ");
      break;
    case 4:
      strcpy(ordinal,"5th ");
      break;
    case 5:
      strcpy(ordinal,"6th ");
      break;
    case 6:
      strcpy(ordinal,"7th ");
      break;
    case 7:
      strcpy(ordinal,"8th ");
      break;
    case 8:
      strcpy(ordinal,"9th ");
      break;
    default:
      sprintf(ordinal,"%d ",line+1);
      break;
  }

  u8g2.setFont(NatRailSmall9);
  blankArea(0,y,256,9);

  if (line<station.numServices) {
    u8g2.drawStr(0,y-1,ordinal);
    int destPos = u8g2.drawStr(23,y-1,station.service[line].sTime) + 4 + 23;
    char plat[3];
    if (station.platformAvailable) {
      if (strlen(station.service[line].platform) && strlen(station.service[line].platform)<3 && station.service[line].serviceType == TRAIN) {
        strncpy(plat,station.service[line].platform,3);
        plat[2]='\0';
      } else {
        if (station.service[line].serviceType == BUS) strcpy(plat,"~");  // Bus icon
        else strcpy(plat,"}}");
      }
      u8g2.drawStr(destPos+11-getStringWidth(plat),y-1,plat);
      destPos+=16;
    }
    char etd[16];
    if (isDigit(station.service[line].etd[0])) sprintf(etd,"Exp %s",station.service[line].etd);
    else strcpy(etd,station.service[line].etd);
    int etdWidth = getStringWidth(etd);
    u8g2.drawStr(SCREEN_WIDTH - etdWidth,y-1,etd);
    // work out if we need to clip the destination
    strcpy(clipDestination,station.service[line].destination);

    int spaceAvailable = SCREEN_WIDTH - destPos - etdWidth - 6;

    if (getStringWidth(clipDestination) > spaceAvailable) {
      while (getStringWidth(clipDestination) > spaceAvailable - 17) {
        clipDestination[strlen(clipDestination)-1] = '\0';
      }
      // check if there's a trailing space left
      if (clipDestination[strlen(clipDestination)-1] == ' ') clipDestination[strlen(clipDestination)-1] = '\0';
      strcat(clipDestination,"...");
    }
    u8g2.drawStr(destPos,y-1,clipDestination);
  } else {
    if (strlen(weatherMsg) && line==station.numServices) {
      // We're showing the weather
      centreText(weatherMsg,y-1);
    } else {
      // We're showing the mandatory attribution
      centreText(F("Powered by National Rail Enquiries"),y-1);
    }
  }
}

// Draw and animate the Departures Board
void drawStationBoard() {
  numMessages=0;
  if (firstLoad) {
    // Clear the entire screen for the first load since boot up/wake from sleep
    u8g2.clearBuffer();
    u8g2.setContrast(brightness);
    firstLoad=false;
  } else {
    // Clear the top two lines
    blankArea(0,LINE0,256,LINE2-1);
  }
  u8g2.setFont(NatRailSmall9);
  if (dateEnabled) {
    // Get the date
    char sysTime[29];
    getLocalTime(&timeinfo);
    strftime(sysTime,29,"%a %d %b",&timeinfo);
    int dateWidth = u8g2.drawStr(SCREEN_WIDTH-getStringWidth(sysTime),LINE0-1,sysTime);
    if (SCREEN_WIDTH-getStringWidth(station.location)/2 < (dateWidth+8)) {
      // Draw station name on the left as it's too long to be centered with the date showing
      u8g2.drawStr(0,LINE0-1,station.location);
    } else {
      centreText(station.location,LINE0-1);
    }
  } else {
    centreText(station.location,LINE0-1);
  }

  // Draw the primary service line
  isShowingVia=false;
  viaTimer=millis()+300000;  // effectively don't check for via
  if (station.numServices) {
    drawPrimaryService(false);
    if (strlen(station.service[0].via)) viaTimer=millis()+4000;
    if (station.service[0].isCancelled) {
      // This train is cancelled
      if (strlen(station.service[0].serviceMessage)) {
        strcpy(line2[0],station.service[0].serviceMessage);
        numMessages=1;
      }
    } else {
      // The train is not cancelled
      if (station.service[0].isDelayed && strlen(station.service[0].serviceMessage)) {
        // The train is delayed and there's a reason
        strcpy(line2[0],station.service[0].serviceMessage);
        numMessages++;
      }
      if (strlen(station.service[0].calling)) {
        // Add the calling stops message
        sprintf(line2[numMessages],"Calling at: %s",station.service[0].calling);
        numMessages++;
      }
      if (strcmp(station.service[0].origin, station.location)==0) {
        // Service originates at this station
        if (strlen(station.service[0].opco)) {
          sprintf(line2[numMessages],"This %s service starts here.",station.service[0].opco);
        } else {
          strcpy(line2[numMessages],"This service starts here.");
        }
        // Add the seating if available
        switch (station.service[0].classesAvailable) {
          case 1:
            strcat(line2[numMessages],firstClassSeating);
            break;
          case 2:
            strcat(line2[numMessages],standardClassSeating);
            break;
          case 3:
            strcat(line2[numMessages],dualClassSeating);
            break;
        }
        numMessages++;
      } else {
        // Service originates elsewhere
        strcpy(line2[numMessages],"");
        if (strlen(station.service[0].opco)) {
          if (strlen(station.service[0].origin)) {
            sprintf(line2[numMessages],"This is the %s service from %s.",station.service[0].opco,station.service[0].origin);
          } else {
            sprintf(line2[numMessages],"This is the %s service.",station.service[0].opco);
          }
        } else {
          if (strlen(station.service[0].origin)) {
            sprintf(line2[numMessages],"This service originated at %s.",station.service[0].origin);
          }
        }
        // Add the seating if available
        switch (station.service[0].classesAvailable) {
          case 1:
            strcat(line2[numMessages],firstClassSeating);
            break;
          case 2:
            strcat(line2[numMessages],standardClassSeating);
            break;
          case 3:
            strcat(line2[numMessages],dualClassSeating);
            break;
        }
        if (strlen(line2[numMessages])) numMessages++;
      }
      if (station.service[0].trainLength) {
        // Add the number of carriages message
        sprintf(line2[numMessages],"This train is formed of %d coaches.",station.service[0].trainLength);
        numMessages++;
      }
    }
    // Add any nrcc messages
    for (int i=0;i<station.numMessages;i++) {
      strcpy(line2[numMessages],station.nrccMessages[i]);
      numMessages++;
    }
    // Setup for the first message to rollover to
    isScrollingStops=false;
    currentMessage=numMessages-1;
  } else {
    blankArea(0,LINE2,256,LINE4-LINE2);
    u8g2.setFont(NatRailTall12);
    centreText(F("There are no scheduled services at this station."),LINE1-1);
    numMessages = station.numMessages;
    for (int i=0;i<station.numMessages;i++) {
      strcpy(line2[i],station.nrccMessages[i]);
    }
    // Setup for the first message to rollover to
    isScrollingStops=false;
    currentMessage=numMessages-1;
  }
  u8g2.setFont(NatRailSmall9);
  u8g2.sendBuffer();
}

/*
 * Web GUI functions
 */

// Helper function for returning text status messages
void sendResponse(int code, const __FlashStringHelper* msg) {
  server.send(code, contentTypeText, msg);
}

void sendResponse(int code, String msg) {
  server.send(code, contentTypeText, msg);
}

// Return the correct MIME type for a file name
String getContentType(String filename) {
  if (server.hasArg(F("download"))) {
    return F("application/octet-stream");
  } else if (filename.endsWith(F(".htm"))) {
    return F("text/html");
  } else if (filename.endsWith(F(".html"))) {
    return F("text/html");
  } else if (filename.endsWith(F(".css"))) {
    return F("text/css");
  } else if (filename.endsWith(F(".js"))) {
    return F("application/javascript");
  } else if (filename.endsWith(F(".png"))) {
    return F("image/png");
  } else if (filename.endsWith(F(".gif"))) {
    return F("image/gif");
  } else if (filename.endsWith(F(".jpg"))) {
    return F("image/jpeg");
  } else if (filename.endsWith(F(".ico"))) {
    return F("image/x-icon");
  } else if (filename.endsWith(F(".xml"))) {
    return F("text/xml");
  } else if (filename.endsWith(F(".pdf"))) {
    return F("application/x-pdf");
  } else if (filename.endsWith(F(".zip"))) {
    return F("application/x-zip");
  } else if (filename.endsWith(F(".json"))) {
    return F("application/json");
  } else if (filename.endsWith(F(".gz"))) {
    return F("application/x-gzip");
  } else if (filename.endsWith(F(".svg"))) {
    return F("image/svg+xml");
  } else if (filename.endsWith(F(".webp"))) {
    return F("image/webp");
  }
  return F("text/plain");
}

// Stream a file from the file system
bool handleStreamFile(String filename) {
  if (LittleFS.exists(filename)) {
    File file = LittleFS.open(filename,"r");
    String contentType = getContentType(filename);
    server.streamFile(file, contentType);
    file.close();
    return true;
  } else return false;
}

// Save the API keys POSTed from the keys.htm page
// If an OWM key is passed, this is tested before being committed to the file system. It's not possible
// to check the National Rail token at this point.
//
void handleSaveKeys() {
  String newJSON, owmToken;
  JsonDocument doc;
  bool result = true;
  String msg = F("API keys saved successfully!");

  if ((server.method() == HTTP_POST) && (server.hasArg("plain"))) {
    newJSON = server.arg("plain");
    // Deserialise to get the OWM API key
    DeserializationError error = deserializeJson(doc, newJSON);
    if (!error) {
      JsonObject settings = doc.as<JsonObject>();
      if (settings["owmToken"].is<const char*>()) {
        owmToken = settings["owmToken"].as<String>();
        if (owmToken.length()) {
          // Check if this is a valid token...
          if (!currentWeather.updateWeather(owmToken, "51.52", "-0.13")) {
            msg = F("OpenWeather Map API key is not valid. No changes have been saved.");
            result = false;
          }
        }
      }
      if (result) {
        if (!saveFile("/apikeys.json",newJSON)) {
          msg = F("Failed to save the API keys to the file system (file system corrupt or full?)");
          result = false;
        }
      }
    } else {
      msg = F("Invalid JSON format. No changes have been saved.");
      result = false;
    }
    if (result) {
      sendResponse(200,msg);
      // Load/Update the API Keys in memory
      loadApiKeys();
      // If the station code is blank, we're in the setup process. If not, they keys have been changed so just reboot.
      if (strlen(crsCode)==0) showSetupCrsHelpScreen(); else ESP.restart();
    } else {
      sendResponse(400,msg);
    }
  } else {
    sendResponse(400,F("Invalid"));
  }
}

// Save configuration setting POSTed from index.htm
void handleSaveSettings() {
  String newJSON;

  if ((server.method() == HTTP_POST) && (server.hasArg("plain"))) {
    newJSON = server.arg("plain");
    saveFile("/config.json",newJSON);
    if (strlen(crsCode)==0) {
      // First time setup, we need a full reboot
      sendResponse(200,F("Configuration saved. The Departures Board will now restart."));
      delay(1000);
      ESP.restart();
    } else {
      sendResponse(200,F("Configuration updated. The Departures Board will update shortly."));
      // Reload the new settings
      loadConfig();
      // Force an update asap
      nextDataUpdate = 0;
      nextWeatherUpdate = 0;
      isScrollingService = false;
      isScrollingStops = false;
      isSleeping=false;
      firstLoad=true;
      noDataLoaded=true;
      viaTimer=0;
      timer=0;
      prevProgressBarPosition=70;
      currentMessage=0;
      prevMessage=0;
      prevScrollStopsLength=0;
      isShowingVia=false;
      line3Service=0;
      prevService=0;
      if (!weatherEnabled) strcpy(weatherMsg,"");

      u8g2.clearBuffer();
      drawStartupHeading();
      u8g2.updateDisplay();
    }
  } else {
    // Something went wrong saving the config file!
    sendResponse(400,F("The configuration could not be updated. The Departures Board will restart."));
    delay(1000);
    ESP.restart();
  }
}

/*
 * Expose the file system via the Web GUI with some basic functions for directory browsing, file reading and deletion.
 */

 // Return storage information
String getFSInfo() {
  char info[70];

  sprintf(info,"Total: %d bytes, Used: %d bytes\n",LittleFS.totalBytes(), LittleFS.usedBytes());
  return String(info);
}

// Send a basic directory listing to the browser
void handleFileList() {
  String path;
  if (!server.hasArg("dir")) path="/"; else path = server.arg("dir");
  File root = LittleFS.open(path);

  String output=F("<html><body>");
  if (!root) {
    output+=F("<p>Failed to open directory</p>");
  } else if (!root.isDirectory()) {
    output+=F("<p>Not a directory</p>");
  } else {
    output+=F("<table>");
    File file = root.openNextFile();
    while (file) {
      output+=F("<tr><td>");
      if (file.isDirectory()) {
        output+="[DIR]</td><td><a href=\"/rmdir?f=" + String(file.path()) + F("\">X</a></td><td><a href=\"/dir?dir=") + String(file.path()) + F("\">") + String(file.name()) + F("</a></td></tr>");
      } else {
        output+=String(file.size()) + F("</td><td><a href=\"/del?f=")+ String(file.path()) + F("\">X</a></td><td><a href=\"/cat?f=") + String(file.path()) + F("\">") + String(file.name()) + F("</a></td></tr>");
      }
      file = root.openNextFile();
    }
  }

  output += "</table><br><br>" + getFSInfo() + F("</body></html>");
  server.send(200,F("text/html"),output);
}

// Stream a file to the browser
void handleCat() {
  String filename;

  if (server.hasArg(F("f"))) {
    handleStreamFile(server.arg("f"));
  } else sendResponse(404,F("Not found"));
}

// Delete a file from the file system
void handleDelete() {
  String filename;

  if (server.hasArg(F("f"))) {
    if (LittleFS.remove(server.arg(F("f")))) {
      // Successfully removed go back to directory listing
      server.sendHeader(F("Location"),F("/dir"));
      server.send(303);
    } else sendResponse(400,F("Failed to delete file"));
  } else sendResponse(404,F("Not found"));

}

// Format the file system
void handleFormatFFS() {
  String message;

  if (LittleFS.format()) message="File System was successfully formatted\n\n" + getFSInfo(); else message=F("File System could not be formatted!");
  sendResponse(200,message);
}

// Upload a file from the browser
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith(F("/"))) {
      filename = "/" + filename;
    }
    fsUploadFile = LittleFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
      sendResponse(200,F("Upload complete"));
    } else {
      sendResponse(500,F("Error: Could not create file."));
    }
  }
}

/*
 * Web GUI handlers
 */

// Fallback function for browser requests
void handleNotFound() {
    if ((LittleFS.exists(server.uri())) && (server.method() == HTTP_GET)) {
      handleStreamFile(server.uri());
    } else sendResponse(404,F("Not Found"));
}

// Send some useful system station information to the browser
void handleInfo() {
  unsigned long uptime = millis();
  char sysUptime[30];
  int days = uptime / msDay ;
  int hours = (uptime % msDay) / msHour;
  int minutes = ((uptime % msDay) % msHour) / msMin;

  sprintf(sysUptime,"%d days, %d hrs, %d min", days,hours,minutes);

  String message = "Hostname: " + String(hostname) + F("\nFirmware version: v") + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + " " + getBuildTime() + F("\nSystem uptime: ") + String(sysUptime) + F("\nFree RAM: ") + String(ESP.getFreeHeap()) + F("\nFree sketch space: ") + String(ESP.getFreeSketchSpace()) + F("\nFree disk space: ") + String(LittleFS.totalBytes() - LittleFS.usedBytes());
  message+="\nCore Plaform: " + String(ESP.getCoreVersion()) + F("\nCPU speed: ") + String(ESP.getCpuFreqMHz()) + F("MHz\nCPU Temperature: ") + String(temperatureRead()) + F("\nWiFi network: ") + String(WiFi.SSID()) + F("\nWiFi signal strength: ") + String(WiFi.RSSI()) + F("dB");
  getLocalTime(&timeinfo);

  sprintf(sysUptime,"%02d:%02d:%02d %02d/%02d/%04d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year+1900);
  message+="\nSystem clock: " + String(sysUptime);
  message+="\nCRS station code: " + String(crsCode) + F("\nSuccessful: ") + String(dataLoadSuccess) + F("\nFailures: ") + String(dataLoadFailure) + F("\nTime since last data load: ") + String((int)((millis()-lastDataLoadTime)/1000)) + F(" seconds");
  if (dataLoadFailure) message+="\nTime since last failure: " + String((int)((millis()-lastLoadFailure)/1000)) + F(" seconds");
  message+="\nLast Result: " + raildata.getLastError();
  message+="\nServices: " + String(station.numServices) + F("\nMessages: ") + String(station.numMessages) + F("\n");
  for (int i=0;i<station.numMessages;i++) message+=String(station.nrccMessages[i]) + "\n";
  message+=F("\nUpdate result code: ");
  switch (lastUpdateResult) {
    case UPD_SUCCESS:
      message+=F("SUCCESS");
      break;
    case UPD_NO_CHANGE:
      message+=F("SUCCESS (NO CHANGES)");
      break;
    case UPD_DATA_ERROR:
      message+=F("DATA ERROR");
      break;
    case UPD_UNAUTHORISED:
      message+=F("UNAUTHORISED");
      break;
    case UPD_HTTP_ERROR:
      message+=F("HTTP ERROR");
      break;
    case UPD_INCOMPLETE:
      message+=F("INCOMPLETE JSON RECEIVED");
      break;
    case UPD_NO_RESPONSE:
      message+=F("NO RESPONSE FROM SERVER");
      break;
    case UPD_TIMEOUT:
      message+=F("TIMEOUT WAITING FOR SERVER");
      break;
    default:
      message+="ERROR CODE (" + String(lastUpdateResult) + F(")");
      break;
  }

  sendResponse(200,message);
}

// Stream the index.htm page unless we're in first time setup and need the api keys
void handleRoot() {
  if (strlen(nrToken)==0 && LittleFS.exists(F("/keys.htm"))) handleStreamFile(F("/keys.htm"));
  else if (strlen(nrToken) && LittleFS.exists(F("/index.htm"))) handleStreamFile(F("/index.htm"));
  else handleInfo();
}

// Send the firmware version to the client (called from index.htm)
void handleFirmwareInfo() {
  String response = "{\"firmware\":\"B" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "W" + String(waCurrentMajor) + "." + String(waCurrentMinor) + F(" Build:") + getBuildTime() + F("\"}");
  server.send(200,contentTypeJson,response);
}

// Force a new data fetch and send the result back to the browser (for debugging)
void handleForceFetch() {
  getStationBoard();
  handleInfo();
}

// Force a reboot of the ESP32
void handleReboot() {
  sendResponse(200,F("Rebooting..."));
  delay(1000);
  ESP.restart();
}

// Erase the stored WiFiManager credentials
void handleEraseWiFi() {
  sendResponse(200,F("Erasing stored WiFi. You will need to use WiFi Manager to reconfigure."));
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  ESP.restart();
}

// "Factory reset" the app - delete WiFi, format file system and reboot
void handleFactoryReset() {
  sendResponse(200,F("Factory reseting the Departures Board..."));
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  LittleFS.format();
  delay(500);
  ESP.restart();
}

// Interactively change the brightness of the OLED panel (called from index.htm)
void handleBrightness() {
  if (server.hasArg(F("b"))) {
    int level = server.arg(F("b")).toInt();
    if (level>0 && level<256) {
      u8g2.setContrast(level);
      brightness = level;
      sendResponse(200,F("OK"));
      return;
    }
  }
  sendResponse(200,F("invalid request"));
}

/*
 * External data functions - weather, stationpicker, firmware updates
 */

// Call the National Rail Station Picker (called from index.htm)
void handleStationPicker() {
  if (!server.hasArg(F("q"))) {
    sendResponse(400, F("Missing Query"));
    return;
  }

  String query = server.arg(F("q"));
  if (query.length() <= 2) {
    sendResponse(400, F("Query Too Short"));
    return;
  }

  const char* host = "stationpicker.nationalrail.co.uk";
  WiFiClientSecure httpsClient;
  httpsClient.setInsecure();
  httpsClient.setTimeout(10000);

  int retryCounter = 0;
  while (!httpsClient.connect(host, 443) && retryCounter++ < 20) {
    delay(50);
  }

  if (retryCounter >= 20) {
    sendResponse(408, F("NR Timeout"));
    return;
  }

  httpsClient.print(String("GET /stationPicker/") + query + F(" HTTP/1.0\r\n") +
                    F("Host: stationpicker.nationalrail.co.uk\r\n") +
                    F("Referer: https://www.nationalrail.co.uk\r\n") +
                    F("Origin: https://www.nationalrail.co.uk\r\n") +
                    F("Connection: close\r\n\r\n"));

  // Wait for response header
  retryCounter = 0;
  while (!httpsClient.available() && retryCounter++ < 15) {
    delay(100);
  }

  if (!httpsClient.available()) {
    httpsClient.stop();
    sendResponse(408, F("NRQ Timeout"));
    return;
  }

  // Parse status code
  String statusLine = httpsClient.readStringUntil('\n');
  if (!statusLine.startsWith(F("HTTP/")) || statusLine.indexOf(F("200 OK")) == -1) {
    httpsClient.stop();

    if (statusLine.indexOf(F("401")) > 0) {
      sendResponse(401, F("Not Authorized"));
    } else if (statusLine.indexOf(F("500")) > 0) {
      sendResponse(500, F("Server Error"));
    } else {
      sendResponse(503, statusLine.c_str());
    }
    return;
  }

  // Skip the remaining headers
  while (httpsClient.connected() || httpsClient.available()) {
    String line = httpsClient.readStringUntil('\n');
    if (line == "\r") break;
  }

  // Start sending response
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, contentTypeJson, "");

  String buffer;
  unsigned long timeout = millis() + 5000UL;

  while ((httpsClient.connected() || httpsClient.available()) && millis() < timeout) {
    while (httpsClient.available()) {
      char c = httpsClient.read();
      if (c <= 128) buffer += c;
      if (buffer.length() >= 1024) {
        server.sendContent(buffer);
        buffer = "";
        yield();
      }
    }
  }

  // Flush remaining buffer
  if (buffer.length()) {
    server.sendContent(buffer);
  }

  httpsClient.stop();
  server.sendContent("");
  server.client().stop();
}

// Update the current weather message if weather updates are enabled and we have a lat/lon for the selected location
void updateCurrentWeather() {
  nextWeatherUpdate = millis() + 1200000; // update every 20 mins
  if (!stationLat || !stationLon) return; // No location co-ordinates
  strcpy(weatherMsg,"");
  bool currentWeatherValid = currentWeather.updateWeather(openWeatherMapApiKey, String(stationLat), String(stationLon));
  if (currentWeatherValid) {
    currentWeather.currentWeather.toCharArray(weatherMsg,sizeof(weatherMsg));
    weatherMsg[0] = toUpperCase(weatherMsg[0]);
    weatherMsg[sizeof(weatherMsg)-1] = '\0';
  } else {
    nextWeatherUpdate = millis() + 30000; // Try again in 30s
  }
}

// Callback function for displaying firmware update progress
void update_progress(int cur, int total) {
  int percent = ((cur * 100)/total);
  showFirmwareUpdateProgress(percent);
}

// Attempts to install newer firmware if available
bool checkForFirmwareUpdate() {
  bool result = true;
  int releaseMajor = ghUpdate.releaseId.substring(1,ghUpdate.releaseId.indexOf(".")).toInt();
  int releaseMinor = ghUpdate.releaseId.substring(ghUpdate.releaseId.indexOf(".")+1,ghUpdate.releaseId.indexOf("-")).toInt();
  if (VERSION_MAJOR > releaseMajor) return result;
  if ((VERSION_MAJOR == releaseMajor) && (VERSION_MINOR >= releaseMinor)) return result;

  log_i("Firmware update available %s\n",ghUpdate.releaseId.c_str());

  // Find the firmware binary in the release assets
  String updatePath="";
  for (int i=0;i<ghUpdate.releaseAssets;i++){
    if (ghUpdate.releaseAssetName[i] == "firmware.bin") {
      updatePath = ghUpdate.releaseAssetURL[i];
      break;
    }
  }
  if (updatePath.length()==0) {
    log_e("No firmware binary in release assets\n");
    return result;
  }

  unsigned long tmr=millis()+1000;
  for (int i=30;i>=0;i--) {
    showFirmwareUpdateWarningScreen(ghUpdate.releaseDescription.c_str(),i);
    while (tmr>millis()) {
      yield();
      server.handleClient();
    }
    tmr=millis()+1000;
  }
  u8g2.clearDisplay();
  prevProgressBarPosition=0;
  showFirmwareUpdateProgress(0);  // So we don't have a blank screen
  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.onProgress(update_progress);
  httpUpdate.rebootOnUpdate(false); // Don't auto reboot, we'll handle it

  HTTPUpdateResult ret = httpUpdate.handleUpdate(client, updatePath, ghUpdate.accessToken);
  const char* msgTitle = "Firmware Update";
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      char msg[60];
      sprintf(msg,"The update failed with error %d.",httpUpdate.getLastError());
      result=false;
      for (int i=15;i>=0;i--) {
        showUpdateCompleteScreen(msgTitle,msg,httpUpdate.getLastErrorString().c_str(),i,false);
        delay(1000);
      }
      break;

    case HTTP_UPDATE_NO_UPDATES:
      for (int i=10;i>=0;i--) {
        showUpdateCompleteScreen(msgTitle,"","No firmware updates were available.",i,false);
        delay(1000);
      }
      break;

    case HTTP_UPDATE_OK:
      for (int i=10;i>=0;i--) {
        showUpdateCompleteScreen(msgTitle,"The firmware update has completed successfully.","",i,true);
        delay(1000);
      }
      ESP.restart();
      break;
  }
  u8g2.clearDisplay();
  drawStartupHeading();
  u8g2.sendBuffer();
  return result;
}

// Attempts to update the Web GUI files if newer versions are available
bool checkForWebAppUpdate() {
  File f;
  bool wauResult = true;

  String waAvailableRelease = ghUpdate.releaseId.substring(ghUpdate.releaseId.indexOf("-")+2);
  int releaseMajor = waAvailableRelease.substring(0,waAvailableRelease.indexOf(".")).toInt();
  int releaseMinor = waAvailableRelease.substring(waAvailableRelease.indexOf(".")+1).toInt();

  if (waCurrentMajor > releaseMajor) return wauResult;
  if ((waCurrentMajor == releaseMajor) && (waCurrentMinor >= releaseMinor)) return wauResult;

  log_i("WebApp update available %s\n",ghUpdate.releaseId.c_str());
  int savedProgressPercent = prevProgressBarPosition;
  prevProgressBarPosition=0;
  showWebappUpdateProgress(0);
  int fileNo=0;
  for (int i=0;i<ghUpdate.releaseAssets;i++) {
    // Skip over the .bin firmware files
    if (!ghUpdate.releaseAssetName[i].endsWith(".bin")) {
      if (!ghUpdate.downloadAssetToLittleFS(ghUpdate.releaseAssetURL[i],"/"+ghUpdate.releaseAssetName[i])) {
        wauResult = false;
        break;
      }
      fileNo++;
      int percent = (fileNo*100)/(ghUpdate.releaseAssets-4);  // There are always 4 .bin files in release assets. Everything else is assumed to be Web GUI
      showWebappUpdateProgress(percent);
    }
  }
  if (wauResult) {
    // All the files have been updated, so update the webapp version
    waCurrentMajor = releaseMajor;
    waCurrentMinor = releaseMinor;
    f = LittleFS.open(F("/webapp.ver"),"w");
    if (f) {
      f.println(releaseMajor);
      f.println(releaseMinor);
      f.close();
    }
    log_i("WebApp Updated OK\n");
  } else {
    char msg[60];
    sprintf(msg,"The update failed with error %d.",httpUpdate.getLastError());
    for (int i=15;i>=0;i--) {
      showUpdateCompleteScreen("Web GUI Update","The update did not complete successfully.",ghUpdate.getLastError().c_str(),i,false);
      delay(1000);
    }
  }
  u8g2.clearDisplay();
  drawStartupHeading();
  u8g2.sendBuffer();

  prevProgressBarPosition = savedProgressPercent;
  return wauResult;
}

/*
 * Setup / Loop functions
*/
void setup(void) {
  // These are the default wsdl XML SOAP entry points. They can be overridden in the config.json file if necessary
  strncpy(wsdlHost,"lite.realtime.nationalrail.co.uk",sizeof(wsdlHost));
  strncpy(wsdlAPI,"/OpenLDBWS/wsdl.aspx?ver=2021-11-01",sizeof(wsdlAPI));
  u8g2.begin();                       // Start the OLED panel
  u8g2.setContrast(brightness);       // Initial brightness
  u8g2.setDrawColor(1);               // Only a monochrome display, so set the colour to "on"
  u8g2.setFontMode(1);                // Transparent fonts
  u8g2.setFontRefHeightAll();         // Count entire font height
  u8g2.setFontPosTop();               // Reference from top
  u8g2.setFont(NatRailTall12);        // Set the initial font
  drawStartupHeading();               // Draw the startup heading
  u8g2.sendBuffer();                  // Send to OLED display
  progressBar(F("Initialising Departures Board"),0);

  bool isFSMounted = LittleFS.begin(true);    // Start the File System, format if necessary
  strcpy(station.location,"");                // No default location
  strcpy(weatherMsg,"");                      // No weather message
  strcpy(nrToken,"");                         // No default National Rail token
  loadApiKeys();                              // Load the API keys from the apiKeys.json
  loadConfig();                               // Load the configuration settings from config.json
  u8g2.setContrast(brightness);               // Set the panel brightness to the user saved level

  progressBar(F("Initialising Departures Board"),15);
  u8g2.drawStr(SCREEN_WIDTH-getStringWidth(crsCode),53,crsCode);  // Display the saved station code in the bottom right corner
  u8g2.sendBuffer();

  progressBar(F("Connecting to Wi-Fi"),30);
  WiFi.mode(WIFI_MODE_NULL);        // Reset the WiFi
  WiFi.setSleep(WIFI_PS_NONE);      // Turn off WiFi Powersaving
  WiFi.hostname(hostname);          // Set the hostname ("Departures Board")
  WiFi.mode(WIFI_STA);              // Enter WiFi station mode
  WiFiManager wm;                   // Start WiFiManager
  wm.setAPCallback(wmConfigModeCallback);     // Set the callback for config mode notification
  wm.setWiFiAutoReconnect(true);              // Attempt to auto-reconnect WiFi
  wm.setConnectTimeout(8);
  wm.setConnectRetries(2);

  bool result = wm.autoConnect("Departures Board");    // Attempt to connect to WiFi (or enter interactive configuration mode)
  if (!result) {
      // Failed to connect/configure
      ESP.restart();
  }

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }

  // Get our IP address and store
  updateMyUrl();

  wifiConnected=true;
  WiFi.setAutoReconnect(true);
  u8g2.clearBuffer();                                             // Clear the display
  drawStartupHeading();                                           // Draw the startup heading
  u8g2.drawStr(SCREEN_WIDTH-getStringWidth(crsCode),53,crsCode);  // Display the saved station code in the bottom right corner
  char ipBuff[17];
  WiFi.localIP().toString().toCharArray(ipBuff,sizeof(ipBuff));   // Get the IP address of the ESP32
  centreText(ipBuff,53);                                          // Display the IP address
  progressBar(F("Wi-Fi Connected"),40);
  u8g2.sendBuffer();                                              // Send to OLED panel

  // Configure the local webserver paths
  server.on(F("/"),handleRoot);
  server.on(F("/erasewifi"),handleEraseWiFi);
  server.on(F("/factoryreset"),handleFactoryReset);
  server.on(F("/info"),handleInfo);
  server.on(F("/fetch"),handleForceFetch);
  server.on(F("/formatffs"),handleFormatFFS);
  server.on(F("/dir"),handleFileList);
  server.onNotFound(handleNotFound);
  server.on(F("/cat"),handleCat);
  server.on(F("/del"),handleDelete);
  server.on(F("/reboot"),handleReboot);
  server.on(F("/stationpicker"),handleStationPicker);           // Used by the Web GUI to lookup station codes interactively
  server.on(F("/firmware"),handleFirmwareInfo);                 // Used by the Web GUI to display the running firmware version
  server.on(F("/savesettings"),HTTP_POST,handleSaveSettings);   // Used by the Web GUI to save updated configuration settings
  server.on(F("/savekeys"),HTTP_POST,handleSaveKeys);           // Used by the Web GUI to verify/save API keys
  server.on(F("/brightness"),handleBrightness);                 // Used by the Web GUI to interactively set the panel brightness

  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", updatePage);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    sendResponse(200,(Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        //Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        //Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
      } else {
        //Update.printError(Serial);
      }
    }
  });

  server.on(F("/upload"), HTTP_GET, []() {
      server.send(200, "text/html", uploadPage);
  });
  server.on(F("/upload"), HTTP_POST, []() {
  }, handleFileUpload);

  server.begin();     // Start the local web server

  // Check for Firmware/GUI updates?
  if (firmwareUpdates) {
    progressBar(F("Checking for firmware updates"),45);
    if (ghUpdate.getLatestRelease()) {
      if (checkForFirmwareUpdate()) {
        // Only check for webapp updates if the firmware update did not fail.
        checkForWebAppUpdate();
      }
    } else {
      for (int i=10;i>=0;i--) {
        showUpdateCompleteScreen("Firmware Update Check Failed","Unable to retrieve latest release information.",ghUpdate.getLastError().c_str(),i,false);
        delay(1000);
      }
      log_e("FW Update failed: %s\n",ghUpdate.getLastError().c_str());
      u8g2.clearDisplay();
      drawStartupHeading();
      u8g2.sendBuffer();
    }
  }

  // First time configuration?
  if (strlen(crsCode)==0 || strlen(nrToken)==0) {
    if (strlen(nrToken)==0) showSetupKeysHelpScreen(); else showSetupCrsHelpScreen();
    // First time setup mode will exit with a reboot, so just loop here forever servicing web requests
    while (true) {
      yield();
      server.handleClient();
    }
  }

  configTime(0,0, ntpServer);                 // Configure NTP server for setting the clock
  setenv("TZ","GMT0BST,M3.5.0/1,M10.5.0",1);  // Configure UK TimeZone
  tzset();                                    // Set the TimeZone

  // Check the clock has been set successfully before continuing
  int p=50;
  int ntpAttempts=0;
  bool ntpResult=true;
  progressBar(F("Setting the system clock..."),50);
  if(!getLocalTime(&timeinfo)) {              // attempt to set the clock from NTP
    do {
      delay(500);                             // If no NTP response, wait 500ms and retry
      ntpResult = getLocalTime(&timeinfo);
      ntpAttempts++;
      p+=5;
      progressBar(F("Setting the system clock..."),p);
      if (p>80) p=45;
    } while ((!ntpResult) && (ntpAttempts<10));
  }
  if (!ntpResult) {
    // Some times NTP/UDP fails. A reboot usually fixes it.
    progressBar(F("Failed to set the clock. Rebooting in 5 sec."),0);
    delay(5000);
    ESP.restart();
  }

  progressBar(F("Initialising National Rail interface"),60);
  int res = raildata.init(wsdlHost, wsdlAPI, &raildataCallback);
  if (res != UPD_SUCCESS) {
    showWsdlFailureScreen();
    while (true) { server.handleClient(); yield();}
  }

  progressBar(F("Initialising National Rail interface"),70);
}


void loop(void) {
  isSleeping = isSnoozing();

  if (isSleeping && millis()>timer) {       // If the "screensaver" is active, change the screen every 8 seconds
    drawSleepingScreen();
    timer=millis()+8000;
  }

  // WiFi Status icon
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    wifiConnected=false;
    u8g2.setFont(NatRailSmall9);
    u8g2.drawStr(0,56,"\x7F");  // No Wifi Icon
    u8g2.updateDisplayArea(0,7,1,1);
  } else if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
    wifiConnected=true;
    blankArea(0,57,5,7);
    u8g2.updateDisplayArea(0,7,1,1);
    updateMyUrl();  // in case our IP changed
  }

  // Force a manual reset if we've been disconnected for more than 10 secs
  if (WiFi.status() != WL_CONNECTED && millis() > lastWiFiReconnect+10000) {
    WiFi.disconnect();
    delay(100);
    WiFi.reconnect();
    lastWiFiReconnect=millis();
  }

  if ((millis() > nextDataUpdate) && (!isScrollingStops) && (!isScrollingService) && (lastUpdateResult != UPD_UNAUTHORISED) && (!isSleeping) && (wifiConnected)) {
    timer = millis() + 2000;
    if (getStationBoard()) {
      if ((lastUpdateResult == UPD_SUCCESS) || (lastUpdateResult == UPD_NO_CHANGE && firstLoad)) drawStationBoard(); // Something changed so redraw the board.
    } else if (lastUpdateResult == UPD_UNAUTHORISED) showTokenErrorScreen();
	  else if (lastUpdateResult == UPD_DATA_ERROR) {
	    if (noDataLoaded) showNoDataScreen();
	    else drawStationBoard();
	} else if (noDataLoaded) showNoDataScreen();
  } else if (weatherEnabled && (millis()>nextWeatherUpdate) && (!noDataLoaded) && (!isScrollingStops) && (!isScrollingService) && (!isSleeping) && (wifiConnected)) {
    updateCurrentWeather();
  }

  if (millis()>timer && numMessages && !isScrollingStops && !isSleeping && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
    // Need to start a new scrolling line 2
    prevMessage = currentMessage;
    prevScrollStopsLength = scrollStopsLength;
    currentMessage++;
    if (currentMessage>=numMessages) currentMessage=0;
    scrollStopsXpos=0;
    scrollStopsYpos=10;
    scrollStopsLength = getStringWidth(line2[currentMessage]);
    isScrollingStops=true;
  }

  // Check if there's a via destination
  if (millis()>viaTimer) {
    if (station.numServices && strlen(station.service[0].via) && !isSleeping && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
      isShowingVia = !isShowingVia;
      drawPrimaryService(isShowingVia);
      u8g2.updateDisplayArea(0,1,32,3);
      if (isShowingVia) viaTimer = millis()+3000; else viaTimer = millis()+4000;
    }
  }

  if (millis()>serviceTimer && !isScrollingService && !isSleeping && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
    // Need to change to the next service if there is one
    if (station.numServices <= 1 && strlen(weatherMsg)==0) {
      // There's no other services and no weather so just so static attribution.
      drawServiceLine(1,LINE3);
      serviceTimer = millis() + 30000;
      isScrollingService = false;
    } else {
      prevService = line3Service;
      line3Service++;
      if (station.numServices) {
        if ((line3Service>station.numServices && strlen(weatherMsg)==0) || (line3Service>station.numServices+1 && strlen(weatherMsg))) line3Service=1;  // First 'other' service
      } else {
        if (strlen(weatherMsg) && line3Service>1) line3Service=0;
      }
      scrollServiceYpos=10;
      isScrollingService = true;
    }
  }

  if (isScrollingStops && millis()>timer && !isSleeping) {
    blankArea(0,LINE2,256,9);
    if (scrollStopsYpos) {
      // we're scrolling up the message initially
      u8g2.setClipWindow(0,LINE2,256,LINE2+9);
      // if the previous message didn't scroll then we need to scroll it up off the screen
      if (prevScrollStopsLength && prevScrollStopsLength<256 && strncmp("Calling",line2[prevMessage],7)) centreText(line2[prevMessage],scrollStopsYpos+LINE2-12);
      if (scrollStopsLength<256 && strncmp("Calling",line2[currentMessage],7)) centreText(line2[currentMessage],scrollStopsYpos+LINE2-2); // Centre text if it fits
      else u8g2.drawStr(0,scrollStopsYpos+LINE2-2,line2[currentMessage]);
      u8g2.setMaxClipWindow();
      scrollStopsYpos--;
      if (scrollStopsYpos==0) timer=millis()+1500;
    } else {
      // we're scrolling left
      if (scrollStopsLength<256 && strncmp("Calling",line2[currentMessage],7)) centreText(line2[currentMessage],LINE2-1); // Centre text if it fits
      else u8g2.drawStr(scrollStopsXpos,LINE2-1,line2[currentMessage]);
      if (scrollStopsLength < 256) {
        // we don't need to scroll this message, it fits so just set a longer timer
        timer=millis()+6000;
        isScrollingStops=false;
      } else {
        scrollStopsXpos--;
        if (scrollStopsXpos < -scrollStopsLength) {
          isScrollingStops=false;
          timer=millis()+500;  // pause before next message
        }
      }
    }
  }

  if (isScrollingService && millis()>serviceTimer && !isSleeping) {
    blankArea(0,LINE3,256,9);
    if (scrollServiceYpos) {
      // we're scrolling the service into view
      u8g2.setClipWindow(0,LINE3,256,LINE3+9);
      // if the prev service is showing, we need to scroll it up off
      if (prevService>0) drawServiceLine(prevService,scrollServiceYpos+LINE3-12);
      drawServiceLine(line3Service,scrollServiceYpos+LINE3-1);
      u8g2.setMaxClipWindow();
      scrollServiceYpos--;
      if (scrollServiceYpos==0) {
        serviceTimer=millis()+5000;
        isScrollingService=false;
      }
    }
  }

  if (!isSleeping) {
    // Check if the clock should be updated
    doClockCheck();

    // To ensure a consistent refresh rate (for smooth text scrolling), we update the screen every 25ms (around 40fps)
    // so we need to wait any additional ms not used by processing so far before sending the frame to the display controller
    long delayMs = fpsDelay - (millis()-refreshTimer);
    if (delayMs>0) delay(delayMs);
    u8g2.updateDisplayArea(0,3,32,4);
    refreshTimer=millis();
  }

  server.handleClient();
}
