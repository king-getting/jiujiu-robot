// 触摸原始数据诊断：判断XPT2046是否应答
// 2026-08-07 现象：读数恒定 x=8191 y=8191 pressed=1 = 芯片未被选中(CS链路问题)
// 正常表现：不按时数值低位波动pressed=0，按住时x/y随手指位置变化
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  tft.init();
  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH);   // 背光手动开（setup_t里没有pin_tft_bl）
  Serial.println("=== 触摸原始数据诊断 ===");
  Serial.println("用手指/指甲按屏幕，看数值变不变");
  Serial.println("恒定8191=芯片无应答；数值跟随手指=通信正常");
}

void loop() {
  uint16_t x, y;
  bool pressed = tft.getTouchRaw(&x, &y);
  Serial.printf("x=%4d  y=%4d  pressed=%d\n", x, y, pressed);
  delay(200);
}
