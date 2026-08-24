#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>

TFT_eSPI tft = TFT_eSPI();

struct SdConfig {
  int sck;
  int miso;
  int mosi;
  int cs;
  const char* name;
};

SdConfig configs[] = {
  {18, 19, 23, 14, "A: MISO19/MOSI23 CS14"},
  {18, 23, 19, 14, "B: swap MISO/MOSI CS14"},
  {18, 19, 23, 21, "C: MISO19/MOSI23 CS21"},
  {18, 23, 19, 21, "D: swap MISO/MOSI CS21"},
};

int okIndex = -1;
uint64_t okSize = 0;

void restoreTft() {
  SPI.end();
  tft.init();
  tft.setRotation(1);
  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  restoreTft();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("SD auto-diagnose...", 160, 8, 2);

  for (int i = 0; i < 4; i++) {
    SdConfig& c = configs[i];
    SPI.end();
    SPI.begin(c.sck, c.miso, c.mosi, c.cs);
    pinMode(c.cs, OUTPUT);
    digitalWrite(c.cs, HIGH);
    delay(80);
    if (SD.begin(c.cs, SPI, 400000)) {
      okIndex = i;
      okSize = SD.cardSize();
      SD.end();
      break;
    }
    SD.end();
    delay(80);
  }

  restoreTft();
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  if (okIndex >= 0) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("SD OK", 160, 70, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(configs[okIndex].name, 160, 120, 2);
    char sizeBuf[48];
    snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", okSize / (1024.0 * 1024.0));
    tft.drawString(sizeBuf, 160, 150, 2);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("SD FAIL", 160, 70, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("check wiring/power/card", 160, 120, 2);
    tft.drawString("card must be pushed in", 160, 150, 2);
  }
}

void loop() {}
