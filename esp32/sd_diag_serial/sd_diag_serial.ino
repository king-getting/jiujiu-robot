#include <SPI.h>
#include <SD.h>

struct SdConfig {
  int sck;
  int miso;
  int mosi;
  int cs;
  const char* name;
};

SdConfig configs[] = {
  {18, 19, 23, 14, "A:MISO19_MOSI23_CS14"},
  {18, 23, 19, 14, "B:SWAP_MISO_MOSI_CS14"},
  {18, 19, 23, 21, "C:MISO19_MOSI23_CS21"},
  {18, 23, 19, 21, "D:SWAP_MISO_MOSI_CS21"},
};

int okIndex = -1;
uint64_t okSize = 0;

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("SD_DIAG_START");

  for (int i = 0; i < 4; i++) {
    SdConfig& c = configs[i];
    SPI.end();
    SPI.begin(c.sck, c.miso, c.mosi, c.cs);
    pinMode(c.cs, OUTPUT);
    digitalWrite(c.cs, HIGH);
    delay(120);
    Serial.printf("TRY_%s\n", c.name);
    bool ok = SD.begin(c.cs, SPI, 400000);
    Serial.printf("RESULT_%s_OK=%d\n", c.name, ok ? 1 : 0);
    if (ok) {
      okIndex = i;
      okSize = SD.cardSize();
      Serial.printf("SIZE_%llu\n", (unsigned long long)okSize);
      SD.end();
      break;
    }
    SD.end();
    delay(120);
  }

  Serial.printf("SD_DIAG_END_OKINDEX=%d\n", okIndex);
}

void loop() {
  delay(1500);
  Serial.printf("REPEAT_OKINDEX=%d\n", okIndex);
}
