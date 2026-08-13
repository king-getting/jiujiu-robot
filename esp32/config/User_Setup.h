// =====================================================================
// TFT_eSPI User_Setup.h 模板 — 啾啾项目（ILI9341 + XPT2046）
// 实际使用文件位于：C:\Users\89640\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
// 注意：此模板仅含关键项，实际文件是TFT_eSPI原版User_Setup.h修改而来，
//       驱动选择(ILI9341_DRIVER)等其余配置以本地文件为准。
// =====================================================================

// ----- 驱动 -----
#define ILI9341_DRIVER

// ----- 引脚定义（定稿，勿改）-----
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15   // 屏幕片选 CS1
#define TFT_DC    5   // ⚠️ 关键！之前错写为2导致白屏
#define TFT_RST   4
#define TFT_BL   25   // 背光（注意：setup_t里没有pin_tft_bl，代码里需手动digitalWrite）
#define TOUCH_CS 21   // 触摸片选 CS2 🔧 2026-08-07调试中：确认此行无//注释

// ----- SPI频率 -----
#define SPI_FREQUENCY        40000000   // 屏幕40MHz（实测颜色正常）
#define SPI_TOUCH_FREQUENCY   2500000   // 触摸2.5MHz，山寨芯片无应答可降到1000000试

// ----- 方向 -----
#define TFT_ROTATION 1   // 0=竖屏, 1=横屏90°（320x240）

// ----- 字体 -----
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
