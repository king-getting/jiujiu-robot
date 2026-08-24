// 啾啾 半成品主程序 v0.9 (2026-08-22)
// 功能: 开机画面 + 表情动画 + WiFi(热点/AP兜底) + HTTP消息上屏 + 鼓励语
// 未启用: 触摸 / SD卡 / 语音(后续版本)
#include <FS.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "chinese_phrases.h"
#include "gougou.h"

TFT_eSPI tft = TFT_eSPI();
#define USE_SD_IMAGE 0  // 0=内置狗头(稳定/快), 1=从SD读图(需按独立SPI接线)
SPIClass SDSPI(HSPI);
#define SD_SCK  14
#define SD_MISO 12
#define SD_MOSI 13
#define SD_CS   33
bool sdOk = false;

const char* zh_phrase_texts[] = {
  "我在呢",
  "加油鸭",
  "早点休息",
  "今天也要开心",
  "晚安"
};

// ---------- 传感器 (DHT11 + MQ-135) ----------
// DHT11: DATA->GPIO26, VCC->GPIO27(电源可控); MQ-135: AO->GPIO34(需分压, 见交接文档)
#define DHT_POWER_PIN 27
#define DHT_DATA_PIN  26
#define MQ135_AO_PIN  34
DHT dht(DHT_DATA_PIN, DHT11);
float lastTemp = 0, lastHumi = 0;
int   lastAir = 0;
bool  dhtOk = false;
unsigned long lastSensorReadAt = 0;

void readSensors() {
  unsigned long now = millis();
  if (now - lastSensorReadAt < 2000) return;   // DHT11 最多每秒读一次
  lastSensorReadAt = now;
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) { lastTemp = t; lastHumi = h; dhtOk = true; }
  lastAir = analogRead(MQ135_AO_PIN);          // MQ-135 原始 ADC 值
  Serial.printf("[sensor] temp=%.1f humi=%.1f air=%d ok=%d\n", lastTemp, lastHumi, lastAir, dhtOk ? 1 : 0);
}

// ---------- 配置: 手机热点(改成你的) ----------
const char* WIFI_SSID = "Jiujiu-Hotspot";
const char* WIFI_PWD  = "12345678";
const char* AP_SSID   = "Jiujiu";        // 连不上热点时开的AP
const char* AP_PWD    = "12345678";

WebServer server(80);

// ---------- 颜色 ----------
#define BG_PINK    tft.color565(255, 214, 230)
#define BG_DEEP    tft.color565(245, 130, 160)
#define SKIN       tft.color565(255, 224, 189)
#define BLUSH      tft.color565(255, 150, 160)

// ---------- 状态 ----------
String currentMsg = "";

// 屏幕字体无中文, 非 ASCII 字符统一显示为 ? (中文字库后续版本加)
String sanitizeAscii(const String& s) {
  String out = "";
  for (unsigned int i = 0; i < s.length(); i++) {
    char ch = s[i];
    out += (ch >= 0x20 && ch < 0x7F) ? ch : (char)0x3F;
  }
  return out;
}
unsigned long msgShownUntil = 0;
unsigned long lastBlinkAt = 0;
bool blinking = false;
bool idleDirty = true;
int phraseIdx = 0;
unsigned long lastPhraseAt = 0;
String myIP = "";

// ---------- 简单 JSON 取值 (无 ArduinoJson) ----------
String jsonStr(const String& body, const char* key) {
  String k = String("\"") + key + "\"";
  int i = body.indexOf(k);
  if (i < 0) return "";
  i = body.indexOf(':', i);
  if (i < 0) return "";
  i++;
  while (i < (int)body.length() && (body[i]==' '||body[i]=='\t')) i++;
  if (i >= (int)body.length()) return "";
  if (body[i] == '"') {
    i++;
    String v = "";
    while (i < (int)body.length() && body[i] != '"') { v += body[i]; i++; }
    return v;
  }
  String v = "";
  while (i < (int)body.length() && body[i]!=',' && body[i]!='}' && body[i]!=' ' && body[i]!='\n' && body[i]!='\r') { v += body[i]; i++; }
  return v;
}

// ---------- 绘图 ----------
uint16_t read16(File &f) { uint16_t r; f.read((uint8_t*)&r, 2); return r; }
uint32_t read32(File &f) { uint32_t r; f.read((uint8_t*)&r, 4); return r; }

bool drawBmpFromSd(const char *filename, int16_t x, int16_t y) {
  if (!sdOk) return false;
  File bmpFile = SD.open(filename);
  if (!bmpFile) { bmpFile.close(); return false; }
  if (read16(bmpFile) != 0x4D42) { bmpFile.close(); return false; }
  read32(bmpFile); read32(bmpFile);
  uint32_t dataOffset = read32(bmpFile);
  read32(bmpFile);
  int32_t w = read32(bmpFile);
  int32_t h = read32(bmpFile);
  if (read16(bmpFile) != 1 || read16(bmpFile) != 24) { bmpFile.close(); return false; }
  read32(bmpFile);

  uint32_t rowSize = (w * 3 + 3) & ~3;
  bool flip = true;
  if (h < 0) { h = -h; flip = false; }

  uint8_t  lineBuf[320 * 3];
  uint16_t pixBuf[320];
  for (int row = 0; row < h && row < 240; row++) {
    uint32_t pos = dataOffset + (flip ? (uint32_t)(h - 1 - row) : (uint32_t)row) * rowSize;
    bmpFile.seek(pos);
    bmpFile.read(lineBuf, rowSize);
    for (int col = 0; col < w && col < 320; col++) {
      pixBuf[col] = tft.color565(lineBuf[col*3+2], lineBuf[col*3+1], lineBuf[col*3]);
    }
    tft.pushImage(x, y + row, w, 1, pixBuf);
  }
  bmpFile.close();
  return true;
}

void drawHeart(int cx, int cy, int s, uint16_t c) {
  tft.fillCircle(cx - s/3, cy - s/6, s/3, c);
  tft.fillCircle(cx + s/3, cy - s/6, s/3, c);
  tft.fillTriangle(cx - s*2/3, cy + s/8, cx + s*2/3, cy + s/8, cx, cy + s*2/3, c);
}

void drawWelcome() {
  drawFace(false);
  drawZhPhrase(0);
}

void drawFace(bool blink) {
#if USE_SD_IMAGE
  if (drawBmpFromSd("/gougou.bmp", 0, 0)) return;
#endif
  tft.pushImage(0, 0, GOUGOU_W, GOUGOU_H, gougou_bmp);
}

void drawZhPhrase(int idx) {
  const ZhPhrase& p = zh_phrases[idx % ZH_PHRASE_COUNT];
  int x = (320 - p.w) / 2;
  int y = 196;
  tft.drawBitmap(x, y, p.data, p.w, 24, BG_DEEP, BG_PINK);
}

void drawMessage(const String& text) {
  tft.fillScreen(BG_PINK);
  tft.setTextColor(TFT_WHITE, BG_DEEP);
  tft.setTextDatum(TC_DATUM);
  tft.fillRect(0, 0, 320, 36, BG_DEEP);
  tft.drawString("Message", 160, 8, 2);
  tft.setTextColor(TFT_DARKGREY, BG_PINK);
  tft.setTextDatum(MC_DATUM);
  // 简易换行
  String remain = text;
  int line = 0;
  while (remain.length() > 0 && line < 4) {
    String one = remain;
    if ((int)tft.textWidth(one, 4) > 300) {
      int cut = one.length();
      while (cut > 0 && (int)tft.textWidth(one.substring(0, cut), 4) > 300) cut--;
      one = one.substring(0, cut);
      remain = remain.substring(cut);
    } else {
      remain = "";
    }
    tft.drawString(one, 160, 90 + line * 42, 4);
    line++;
  }
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(BG_DEEP, BG_PINK);
  tft.drawString("from your phone", 160, 225, 2);
  idleDirty = true;
}

// ---------- HTTP ----------
void handleRoot() {
  Serial.println("[http] GET /");
  server.send(200, "text/plain; charset=utf-8",
    "Jiujiu is alive!\nIP: " + myIP + "\nPOST /api/message {ver:1,cmd:\"msg\",text:\"...\"}\nGET /api/status");
}

void handleMessage() {
  String body = server.arg("plain");
  Serial.println("[http] POST args=" + String(server.args()) + " bodyLen=" + String(body.length()));
  for (int a = 0; a < server.args(); a++) {
    Serial.println("[http] arg" + String(a) + " key=" + server.argName(a) + " val=" + server.arg(a).substring(0, 60));
  }
  String cmd = jsonStr(body, "cmd");
  if (cmd == "msg" || cmd == "message") {
    currentMsg = sanitizeAscii(jsonStr(body, "text"));
    if (currentMsg.length() == 0) currentMsg = "(empty)";
    msgShownUntil = millis() + 10000;   // 显示10秒
    drawMessage(currentMsg);
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(200, "application/json", "{\"ok\":false,\"err\":\"bad cmd\"}");
  }
}

void handleStatus() {
  bool wifi = WiFi.status() == WL_CONNECTED;
  String json = "{\"ver\":1,\"ip\":\"" + myIP + "\",\"wifi\":" + (wifi ? "true" : "false") +
                ",\"sd\":false,\"battery\":0}";
  server.send(200, "application/json", json);
}

void handleSensor() {
  readSensors();
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"ver\":1,\"temp\":%.1f,\"humi\":%.1f,\"air\":%d}", lastTemp, lastHumi, lastAir);
  server.send(200, "application/json", buf);
}

void handlePhraseAdd() {
  server.send(200, "application/json", "{\"ok\":true}");
}

void handlePhraseDel() {
  server.send(200, "application/json", "{\"ok\":true}");
}

void handlePhraseList() {
  String json = "{\"ver\":1,\"list\":[";
  for (int i = 0; i < (int)ZH_PHRASE_COUNT; i++) {
    if (i) json += ",";
    json += "\"" + String(zh_phrase_texts[i]) + "\"";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// ---------- BLE (与APP硬编码UUID一致, 见 app/README.md) ----------
#define BLE_SERVICE_UUID     "6a696f00-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_WRITE_UUID  "6a696f01-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR_NOTIFY_UUID "6a696f02-0000-1000-8000-00805f9b34fb"

BLECharacteristic* bleNotifyChar = nullptr;
String bleIncoming = "";
volatile bool bleMsgReady = false;

void bleReply(const char* json) {
  if (bleNotifyChar) {
    bleNotifyChar->setValue((uint8_t*)json, strlen(json));
    bleNotifyChar->notify();
  }
}

class JiuJiuBleWrite : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String body = String(c->getValue().c_str());
    Serial.println("[ble] recv: " + body);
    String cmd = jsonStr(body, "cmd");
    if (cmd == "msg" || cmd == "message") {
      bleIncoming = sanitizeAscii(jsonStr(body, "text"));
      if (bleIncoming.length() == 0) bleIncoming = "(empty)";
      bleMsgReady = true;   // 主循环绘制, 避免与TFT SPI冲突
      bleReply("{\"ok\":true}");
    } else if (cmd == "wifi") {
      bleReply("{\"ok\":false,\"err\":\"半成品未支持配网, 请连AP模式 Jiujiu/12345678\"}");
    } else {
      bleReply("{\"ok\":false,\"err\":\"bad cmd\"}");
    }
  }
};

class JiuJiuBleServer : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    Serial.println("[ble] connected");
  }
  void onDisconnect(BLEServer*) override {
    Serial.println("[ble] disconnected, re-advertising");
    BLEDevice::startAdvertising();
  }
};

void setupBLE() {
  BLEDevice::init("JIUJIU");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new JiuJiuBleServer());
  BLEService* service = server->createService(BLE_SERVICE_UUID);
  BLECharacteristic* writeChar = service->createCharacteristic(
    BLE_CHAR_WRITE_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  writeChar->setCallbacks(new JiuJiuBleWrite());
  bleNotifyChar = service->createCharacteristic(
    BLE_CHAR_NOTIFY_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  bleNotifyChar->addDescriptor(new BLE2902());
  service->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("[ble] server started (JIUJIU)");
}
// ---------- WiFi ----------
void setupWiFi() {
  Serial.println("[WiFi] STA connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 6000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    myIP = WiFi.localIP().toString();
    Serial.printf("[WiFi] STA connected, IP=%s\n", myIP.c_str());
  } else {
    Serial.println("[WiFi] STA failed -> switch to AP only");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    delay(300);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PWD, 1, 0);
    myIP = WiFi.softAPIP().toString();
    Serial.printf("[WiFi] AP mode IP=%s (SSID=%s)\n", myIP.c_str(), AP_SSID);
  }
}

// ---------- 主流程 ----------
void setup() {
  Serial.begin(115200);
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, HIGH);   // DHT11 上电
  delay(500);
  dht.begin();
#if USE_SD_IMAGE
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(18, 19, 23, SD_CS);
  sdOk = SD.begin(SD_CS, SPI, 400000);
  Serial.printf("[SD] %s\n", sdOk ? "ok" : "fail");
#else
  Serial.println("[SD] disabled: using built-in gougou image");
#endif
  tft.init();
  tft.setRotation(1);
  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH);   // 背光
  tft.fillScreen(BG_PINK);
  drawWelcome();
  setupWiFi();
  idleDirty = false;
  lastPhraseAt = millis();
  server.on("/", handleRoot);
  server.on("/api/message", HTTP_POST, handleMessage);
  server.on("/api/message", HTTP_GET, handleMessage);
  server.on("/api/status", handleStatus);
  server.on("/api/sensor", handleSensor);
  server.on("/api/phrase/list", handlePhraseList);
  server.on("/api/phrase/add", HTTP_POST, handlePhraseAdd);
  server.on("/api/phrase/del", HTTP_POST, handlePhraseDel);
  server.begin();
  Serial.println("[HTTP] server started");
  setupBLE();
}

void loop() {
  server.handleClient();
  readSensors();

  // BLE 消息上屏(主循环绘制, 避免与TFT SPI冲突)
  if (bleMsgReady) {
    bleMsgReady = false;
    currentMsg = bleIncoming;
    msgShownUntil = millis() + 10000;
    drawMessage(currentMsg);
  }

  unsigned long now = millis();

  // 消息显示中 -> 不画表情
  if (now < msgShownUntil) return;

  // 从消息页回到待机狗头
  if (idleDirty) {
    idleDirty = false;
    drawFace(false);
    drawZhPhrase(phraseIdx);
  }

  // 鼓励语轮换(每12秒)
  if (now - lastPhraseAt > 12000) {
    lastPhraseAt = now;
    phraseIdx = (phraseIdx + 1) % ZH_PHRASE_COUNT;
    drawZhPhrase(phraseIdx);
  }
}











