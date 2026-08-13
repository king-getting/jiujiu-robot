// 触摸校准 + 按钮交互demo
// 流程：第一遍 needCalibrate=true 上传 → 点四角箭头 → 串口打印校准数据
//       把数据填入calData，needCalibrate改false，重新上传 → 按钮demo
// 前提：User_Setup.h 中 #define TOUCH_CS 21（无注释）
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

// ===== 校准相关 =====
uint16_t calData[5] = { 0, 0, 0, 0, 0 };  // 校准后把串口给的数据填这里
bool needCalibrate = true;                 // 第一次跑=true，校准完改=false

// ===== 按钮位置 =====
#define BTN_X 110
#define BTN_Y 90
#define BTN_W 100
#define BTN_H 60

int pressCount = 0;

void drawButton(uint16_t color) {
  tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, color);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("PRESS", BTN_X + BTN_W / 2, BTN_Y + BTN_H / 2);
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);          // 横屏
  pinMode(25, OUTPUT);         // 背光老坑，手动开
  digitalWrite(25, HIGH);

  if (needCalibrate) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Touch calibration", 160, 110);
    tft.drawString("Press corner arrows", 160, 130);
    delay(2000);
    // 屏幕四角依次出现箭头，用笔尖/指甲点箭头中心（电阻屏要用点力）
    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    Serial.println("\n=== 校准完成，复制下面这行 ===");
    Serial.printf("uint16_t calData[5] = { %d, %d, %d, %d, %d };\n",
                  calData[0], calData[1], calData[2], calData[3], calData[4]);
    Serial.println("填到代码开头，needCalibrate改为false，重新上传");
  }

  tft.setTouch(calData);
  tft.fillScreen(TFT_BLUE);
  drawButton(TFT_RED);
}

void loop() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    Serial.printf("触摸: x=%d y=%d\n", x, y);
    if (x > BTN_X && x < BTN_X + BTN_W && y > BTN_Y && y < BTN_Y + BTN_H) {
      pressCount++;
      Serial.printf("按钮命中! 第%d次\n", pressCount);
      uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW,
                           TFT_CYAN, TFT_MAGENTA, TFT_ORANGE};
      tft.fillScreen(colors[pressCount % 7]);   // 按一次换个背景色
      drawButton(TFT_DARKGREY);
      delay(300);   // 消抖
    }
  }
}
