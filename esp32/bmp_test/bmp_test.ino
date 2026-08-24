// SD卡BMP图片显示测试
// 用法：图片用画图软件调整为320x240，另存为24位BMP，命名test.bmp放SD卡根目录
// 状态：2026-08-07 用户侧编译报错 exit status 1，报错原文未拿到，待定位
// 排查方向：1.草图文件夹名必须英文且与.ino同名 2.确认红色报错原文
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>

TFT_eSPI tft = TFT_eSPI();
#define SD_CS 14

uint16_t read16(File &f) { uint16_t r; f.read((uint8_t*)&r, 2); return r; }
uint32_t read32(File &f) { uint32_t r; f.read((uint8_t*)&r, 4); return r; }

void drawBmp(const char *filename, int16_t x, int16_t y) {
  File bmpFile = SD.open(filename);
  if (!bmpFile) { Serial.println("[FAIL] 文件打不开"); return; }
  if (read16(bmpFile) != 0x4D42) { Serial.println("[FAIL] 不是BMP"); bmpFile.close(); return; }
  read32(bmpFile); read32(bmpFile);
  uint32_t dataOffset = read32(bmpFile);
  read32(bmpFile);
  int32_t w = read32(bmpFile);
  int32_t h = read32(bmpFile);
  if (read16(bmpFile) != 1) { bmpFile.close(); return; }
  uint16_t depth = read16(bmpFile);
  if (depth != 24) { Serial.println("[FAIL] 必须24位BMP"); bmpFile.close(); return; }
  read32(bmpFile);
  Serial.printf("图片: %ld x %ld\n", w, h);

  uint32_t rowSize = (w * 3 + 3) & ~3;
  bool flip = true;
  if (h < 0) { h = -h; flip = false; }

  uint8_t  lineBuf[320 * 3];
  uint16_t pixBuf[320];
  for (int row = 0; row < h; row++) {
    uint32_t pos = dataOffset + (flip ? (uint32_t)(h - 1 - row) : (uint32_t)row) * rowSize;
    bmpFile.seek(pos);
    bmpFile.read(lineBuf, rowSize);
    for (int col = 0; col < w; col++) {
      pixBuf[col] = tft.color565(lineBuf[col*3+2], lineBuf[col*3+1], lineBuf[col*3]);
    }
    tft.pushImage(x, y + row, w, 1, pixBuf);
  }
  bmpFile.close();
  Serial.println("[OK] 图片显示完成");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[step] serial ok");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(18, 19, 23, SD_CS);
  Serial.println("[step] sd begin...");
  if (!SD.begin(SD_CS, SPI, 400000)) {
    Serial.println("[FAIL] SD初始化失败（记得把卡插回模块！）");
    while (1) delay(1000);
  }
  Serial.println("[OK] SD就绪");

  tft.init();
  tft.setRotation(1);
  Serial.println("[step] tft ok");
  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH);
  tft.fillScreen(TFT_BLACK);

  // 触摸CS拉高闭嘴（屏幕的CS由TFT_eSPI自己管）
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  drawBmp("/gougou.bmp", 0, 0);
}

void loop() {}
