// =======================================================
//  啾啾项目 — ILI9341 2.8寸触摸屏 (ESP32 NodeMCU-32S)
//  引脚配置已写死,直接覆盖原文件即可
// =======================================================

#define USER_SETUP_INFO "JiuJiu_ILI9341"

// ---------- 1. 驱动 ----------
#define ILI9341_DRIVER       // 2.8寸 ILI9341

// ---------- 2. 屏幕引脚 (ESP32 NodeMCU-32S) ----------
#define TFT_MISO 19          // MISO -> P19
#define TFT_MOSI 23          // MOSI -> P23
#define TFT_SCLK 18          // CLK  -> P18
#define TFT_CS   15          // CS1  -> P15  (屏片选)
#define TFT_DC    5          // DC   -> P5  (原来定P2,实际接线P5)
#define TFT_RST   4          // RES  -> P4

// ---------- 3. 背光 (软件控制,可熄屏省电) ----------
#define TFT_BL   25          // BLK  -> P25
#define TFT_BACKLIGHT_ON HIGH // 高电平点亮

// ---------- 4. 触摸屏 XPT2046 ----------
#define TOUCH_CS 21           // CS2  -> P21 (触摸片选)
// #define TOUCH_IRQ 22      // PEN  -> 暂不接,后期再加

// ---------- 5. SPI 频率 ----------
#define SPI_FREQUENCY        20000000   // 屏幕 20MHz (降低频率提升长时间显示稳定性)
#define SPI_READ_FREQUENCY    5000000   // 屏幕读取 5MHz (读像素检测更稳定)
#define SPI_TOUCH_FREQUENCY   1000000   // 触摸 1MHz (2026-08-14 降频: 2.5MHz下XPT2046无响应)

// ---------- 6. 字体 ----------
#define LOAD_GLCD    // 8像素小字
#define LOAD_FONT2   // 16像素
#define LOAD_FONT4   // 26像素
#define LOAD_FONT6   // 48像素(数字)
#define LOAD_FONT7   // 7段数码
#define LOAD_FONT8   // 75像素(大数字)
#define LOAD_GFXFF   // Adafruit GFX 兼容字体
#define SMOOTH_FONT  // 平滑字体(显示中文要用)

// ---------- 7. 屏幕方向 (0=竖 90=横 180=竖反转 270=横反转) ----------
#define TFT_ROTATION 1       // 横屏(320x240),默认先横着方便看
