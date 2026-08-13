#include <SPI.h>
#include <SD.h>

#define SD_CS    14
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== 啾啾 SD卡测试 ===");

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);   // 先确保CS拉高

  bool ok = false;
  for (int i = 0; i < 5; i++) {
    Serial.printf("第%d次尝试初始化...\n", i + 1);
    if (SD.begin(SD_CS, SPI, 400000)) {   // 低速400kHz
      ok = true;
      break;
    }
    delay(500);
  }

  if (!ok) {
    Serial.println("[FAIL] 重试5次仍失败");
    Serial.println("检查：1.MOSI/MISO是否接反 2.卡是否FAT32格式 3.VCC是否接5V 4.CS是否接的GPIO14");
    Serial.println("教训：先确认卡插到底（咔哒声）！2026-08-07的失败根因就是接触不良");
    while (1) delay(1000);
  }
  Serial.println("[OK] SD卡初始化成功");

  uint8_t cardType = SD.cardType();
  Serial.print("卡类型: ");
  if (cardType == CARD_MMC)       Serial.println("MMC");
  else if (cardType == CARD_SD)   Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else                            Serial.println("未知");

  Serial.printf("卡容量: %.2f MB\n", SD.cardSize() / (1024.0 * 1024.0));

  // 写测试
  File f = SD.open("/jiujiu_test.txt", FILE_WRITE);
  if (f) {
    f.println("Hello! 我是啾啾~");
    f.println("SD卡读写测试通过!");
    f.close();
    Serial.println("[OK] 写入 /jiujiu_test.txt");
  } else {
    Serial.println("[FAIL] 无法创建文件");
  }

  // 读测试
  f = SD.open("/jiujiu_test.txt");
  if (f) {
    Serial.println("--- 读回内容 ---");
    while (f.available()) Serial.write(f.read());
    f.close();
  } else {
    Serial.println("[FAIL] 无法打开文件");
  }

  Serial.println("=== 测试完成 ===");
}

void loop() {}
