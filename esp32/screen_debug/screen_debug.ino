// =======================================================
//  啾啾 — 屏幕诊断 V2 (降速 + 逐引脚测试)
//  上传后打开: 工具 → 串口监视器 (波特率 115200)
// =======================================================

#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(2000); // 等串口稳定

  Serial.println("=== 啾啾屏幕诊断 V2 ===");

  // 1. 背光
  Serial.println("[1] 背光 P25 -> HIGH");
  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH);
  delay(500);

  // 2. 先手动测试关键引脚
  Serial.println("[2] 手动测试 SPI 引脚...");
  
  // 手动拉 CS 低 -> 检查接线
  pinMode(15, OUTPUT);  // CS
  pinMode(5, OUTPUT);   // DC
  pinMode(4, OUTPUT);   // RST
  pinMode(18, OUTPUT);  // SCK
  pinMode(23, OUTPUT);  // MOSI

  // 手动复位屏幕
  Serial.println("    手动复位屏幕 RST(P4)...");
  digitalWrite(4, LOW);   // RST 拉低
  delay(20);
  digitalWrite(4, HIGH);  // RST 拉高释放
  delay(150);             // 等屏幕复位完

  // 3. 降速 SPI 初始化
  Serial.println("[3] tft.init()...");
  tft.init();
  Serial.println("    init() 完成");

  // 4. 读屏幕 ID
  Serial.println("[4] 读取屏幕 ID...");
  uint32_t id = tft.readcommand32(0x04);
  Serial.print("    ID = 0x");
  Serial.println(id, HEX);

  // 读 Power Mode
  uint8_t pwr = tft.readcommand8(0x0D);
  Serial.print("    Power Mode = 0x");
  Serial.println(pwr, HEX);

  // 读 MADCTL (0x0B)
  uint8_t mad = tft.readcommand8(0x0B);
  Serial.print("    MADCTL = 0x");
  Serial.println(mad, HEX);

  if (id == 0 || id == 0xFFFFFF || id == 0xFFFFFFFF) {
    Serial.println("    *** ID = 0, SPI没通! 接线有问题 ***");
  } else {
    Serial.print("    ID = 0x");
    Serial.print(id, HEX);
    Serial.println(" (期望 0x9341 或 0x0)");
  }

  // 5. 打印配置
  Serial.println("[5] 引脚配置:");
  setup_t cfg;
  tft.getSetup(cfg);
  Serial.print("    MOSI="); Serial.println(cfg.pin_tft_mosi);
  Serial.print("    SCLK="); Serial.println(cfg.pin_tft_clk);
  Serial.print("    CS=");   Serial.println(cfg.pin_tft_cs);
  Serial.print("    DC=");   Serial.println(cfg.pin_tft_dc);
  Serial.print("    RST=");  Serial.println(cfg.pin_tft_rst);
  Serial.print("    MISO="); Serial.println(cfg.pin_tft_miso);

  // 6. 横屏
  Serial.println("[6] setRotation(1)...");
  tft.setRotation(1);

  // 7. 先填黑再填红 (黑->红更明显)
  Serial.println("[7] fillScreen(TFT_BLACK)...");
  tft.fillScreen(TFT_BLACK);
  delay(1000);
  
  Serial.println("[8] fillScreen(TFT_RED)...");
  tft.fillScreen(TFT_RED);
  delay(1000);

  Serial.println("[9] fillScreen(TFT_GREEN)...");
  tft.fillScreen(TFT_GREEN);
  delay(1000);

  Serial.println("[10] fillScreen(TFT_BLUE)...");
  tft.fillScreen(TFT_BLUE);
  delay(1000);

  Serial.println("[11] 写文字...");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("JIUJIU!", 80, 100, 4);

  Serial.println("=== 诊断 V2 结束 ===");
}

void loop() {
  digitalWrite(2, !digitalRead(2));
  delay(500);
}
