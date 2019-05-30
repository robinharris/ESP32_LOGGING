

#define USE_HWSERIAL2
#include <Arduino.h>
#include <HardwareSerial.h>
#include <NMEAGPS.h>
#include <Ticker.h>
#include <PMserial.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <SSD1306Wire.h>

#define pixelPin 4
#define NUMPIXELS 1

// object creation
HardwareSerial gpsSerial(1);
SerialPM pms(PMSx003, Serial2);         // PMSx003, UART
NMEAGPS  gps; // This parses the GPS characters
gps_fix  fix, oldFix; // This holds on to the latest values
Adafruit_NeoPixel pixel(NUMPIXELS, pixelPin, NEO_GRB + NEO_KHZ800);
SSD1306Wire oled(0x3c, 21, 22);
// prototpye definitions
void update();
bool writeSD(char*, int32_t, int32_t, int, int);

Ticker timer1(update, 10 * 1000);  // every 10 seconds

// globals
bool newFix = false;
#define SD_CS 5
uint32_t green = pixel.Color(0, 255, 0);
uint32_t orange = pixel.Color(255, 128, 0 );
uint32_t red = pixel.Color(255, 0, 0);
uint32_t purple = pixel.Color(255, 0, 255);

void update(){
  int pm10, pm25;
  pms.read();
  pm10 = pms.pm10;
  pm25 = pms.pm25;
  if(pms){  // successfull read
    Serial.printf("PM1.0 %2d, PM2.5 %2d, PM10 %2d [ug/m3]\n",
      pms.pm01,pm25,pm10);
  }
  else { // something went wrong
    switch (pms.status) {
    case pms.OK: // should never come here
      break;     // included to compile without warnings
    case pms.ERROR_TIMEOUT:
      Serial.println(F(PMS_ERROR_TIMEOUT));
      break;
    case pms.ERROR_MSG_UNKNOWN:
      Serial.println(F(PMS_ERROR_MSG_UNKNOWN));
      break;
    case pms.ERROR_MSG_HEADER:
      Serial.println(F(PMS_ERROR_MSG_HEADER));
      break;
    case pms.ERROR_MSG_BODY:
      Serial.println(F(PMS_ERROR_MSG_BODY));
      break;
    case pms.ERROR_MSG_START:
      Serial.println(F(PMS_ERROR_MSG_START));
      break;
    case pms.ERROR_MSG_LENGTH:
      Serial.println(F(PMS_ERROR_MSG_LENGTH));
      break;
    case pms.ERROR_MSG_CKSUM:
      Serial.println(F(PMS_ERROR_MSG_CKSUM));
      break;
    case pms.ERROR_PMS_TYPE:
      Serial.println(F(PMS_ERROR_PMS_TYPE));
      break;
    }
  }
  oled.clear();
  String line1 = "PM10: " + String(pm10);
  String line2 = "PM2.5: " + String(pm25);
  oled.drawStringMaxWidth(0,0,128, line1);
  oled.drawStringMaxWidth(0,32,128, line2);
  oled.display();
  if (pm25 < 35) {
    pixel.setPixelColor(0, green);
  }
  else if (pm25 < 53) {
    pixel.setPixelColor(0, orange);
  }
  else if (pm25 < 70){
    pixel.setPixelColor(0, red);
  }
  else {
    pixel.setPixelColor(0, purple);
  }
  pixel.show();
  char dt[50]; // to hold dateTime as a String
  // check if we have a valid fix
  if (fix.valid.location && fix.valid.date && fix.valid.time) {
    // now check if the fix has changed - if not ignore
    if (fix.dateTime.seconds != oldFix.dateTime.seconds || fix.dateTime.minutes != oldFix.dateTime.minutes){
      oldFix = fix;
      int32_t lat = int32_t(fix.latitude() * 10e6);
      int32_t lon = int32_t(fix.longitude() * 10e6);
      Serial.print( F("Location: ") );
      Serial.print( fix.latitude(), 6 );
      Serial.print( ',' );
      Serial.print( fix.longitude(), 6 );
      sprintf(dt, "\t%d-%02d-%02d %02d:%02d:%02d", fix.dateTime.date, fix.dateTime.month, fix.dateTime.year,
        fix.dateTime.hours + 1 , fix.dateTime.minutes, fix.dateTime.seconds);
      Serial.print( dt );
      Serial.println();
      bool sdResult = writeSD(dt, lat, lon, pms.pm10, pms.pm25);
      if (!sdResult){
        Serial.println("SD card write failed");
      }
    }
  }
}

bool writeSD(char* dt, int32_t lat, int32_t lon, int pm10, int pm25){
  String lineToWrite;
  lineToWrite = String(dt) + "," + String(lat) + "," + String(lon) + "," + String(pm10) + "," + String(pm25);
  // Initialize SD card
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return false;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }
  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data2.txt",FILE_APPEND);
  if(!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    file.println("Date, time, latitude, longitude, pm10, pm25\r\n");
    file.println(lineToWrite);
  }
  else {
    Serial.println("File already exists, appending");  
    file.println(lineToWrite);
  }
  file.close();
  return true;
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 13, 12);
  Serial.println(F("Booted"));
  pms.init();
  pixel.begin();
  timer1.start();
  pixel.setPixelColor(0, 255,255,255);
  pixel.show();
  oled.init();
  oled.flipScreenVertically();
  oled.setFont(ArialMT_Plain_16);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawStringMaxWidth(0,0,128,"Starting AQ monitor");
  oled.display();
}

void loop() {
  // read the PM sensor
  timer1.update();
  while (gps.available( gpsSerial )) {
    fix = gps.read();
  }
}