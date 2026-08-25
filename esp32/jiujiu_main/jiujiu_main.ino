// 啾啾 半成品主程序 v0.9 (2026-08-22)
// 功能: 开机画面 + 表情动画 + WiFi(热点/AP兜底) + HTTP消息上屏 + 鼓励语
// 未启用: 触摸 / SD卡 / 语音(后续版本)
#include <FS.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <DHT.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "chinese_phrases.h"
#include "gougou.h"
#include "gb2312_16.h"

TFT_eSPI tft = TFT_eSPI();
#define USE_SD_IMAGE 0  // 0=内置狗头(稳定/快), 1=从SD读图(需按独立SPI接线)
#define ENABLE_SD_LOG 1  // 1=传感器历史记录写入 /sensor.csv
SPIClass SDSPI(HSPI);
#define SD_SCK  14
#define SD_MISO 22
#define SD_MOSI 13
#define SD_CS   33
bool sdOk = false;
String phraseTexts[ZH_PHRASE_COUNT];
int phraseCount = ZH_PHRASE_COUNT;

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
unsigned long lastSensorLogAt = 0;

void appendSensorLog();
void initPhrases();

void readSensors() {
  unsigned long now = millis();
  if (now - lastSensorReadAt < 2000) return;   // DHT11 最多每秒读一次
  lastSensorReadAt = now;
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) { lastTemp = t; lastHumi = h; dhtOk = true; }
  lastAir = analogRead(MQ135_AO_PIN);          // MQ-135 原始 ADC 值
  Serial.printf("[sensor] temp=%.1f humi=%.1f air=%d ok=%d\n", lastTemp, lastHumi, lastAir, dhtOk ? 1 : 0);
  appendSensorLog();
}

void appendSensorLog() {
#if ENABLE_SD_LOG
  if (!sdOk) return;
  if (millis() - lastSensorLogAt < 60000) return;   // 每分钟记一条
  lastSensorLogAt = millis();
  File f = SD.open("/sensor.csv", FILE_APPEND);
  if (!f) {
    f = SD.open("/sensor.csv", FILE_WRITE);
    if (!f) { Serial.println("[SD] open sensor.csv fail"); return; }
    f.println("timestamp_ms,temp,humi,air,dht_ok");
  }
  f.printf("%lu,%.1f,%.1f,%d,%d\n", (unsigned long)millis(), lastTemp, lastHumi, lastAir, dhtOk ? 1 : 0);
  f.close();
  Serial.println("[SD] sensor log appended");
#endif
}

void initPhrases() {
  phraseCount = 0;
  if (sdOk) {
    File f = SD.open("/phrases.txt");
    if (f) {
      while (f.available() && phraseCount < ZH_PHRASE_COUNT) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0 && line.length() <= 48) {
          phraseTexts[phraseCount++] = line;
        }
      }
      f.close();
      if (phraseCount > 0) {
        Serial.printf("[SD] loaded %d phrases from /phrases.txt\n", phraseCount);
        return;
      }
    }
  }
  for (int i = 0; i < ZH_PHRASE_COUNT; i++) phraseTexts[i] = String(zh_phrases[i].text);
  phraseCount = ZH_PHRASE_COUNT;
  Serial.printf("[phrase] using %d built-in phrases\n", phraseCount);
}

// ---------- 配置: 手机热点(改成你的) ----------
const char* WIFI_SSID = "Jiujiu-Hotspot";
const char* WIFI_PWD  = "12345678";
const char* AP_SSID   = "Jiujiu";        // 连不上热点时开的AP
const char* AP_PWD    = "12345678";

// 用 Preferences 保存 App 通过蓝牙配网时填写的热点账号/密码。
Preferences wifiPrefs;

void saveWifiCredentials(const String& ssid, const String& pwd) {
  wifiPrefs.begin("jiujiu", false);
  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("pwd", pwd);
  wifiPrefs.end();
  Serial.printf("[wifi] saved ssid=%s pwdLen=%d\n", ssid.c_str(), pwd.length());
}

void loadWifiCredentials(String& ssid, String& pwd) {
  wifiPrefs.begin("jiujiu", true);
  ssid = wifiPrefs.getString("ssid", String(WIFI_SSID));
  pwd = wifiPrefs.getString("pwd", String(WIFI_PWD));
  wifiPrefs.end();
  Serial.printf("[wifi] load ssid=%s pwdLen=%d\n", ssid.c_str(), pwd.length());
}

// ---------- 大模型配置 (DeepSeek / OpenAI兼容) ----------
#define LLM_API_URL   "https://api.deepseek.com/chat/completions"
#define LLM_MODEL     "deepseek-chat"
#define LLM_API_KEY   "PASTE_YOUR_API_KEY_HERE"
#define LLM_SYSTEM_PROMPT "你是啾啾，一只可爱、温柔、会鼓励人的小狗。回复要简短，不超过60个字，不要用Markdown。"

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

void drawIdle() {
  tft.startWrite();
  drawFace(false);
  drawZhPhrase(phraseIdx);
  tft.endWrite();
}

void drawFace(bool blink) {
#if USE_SD_IMAGE
  if (drawBmpFromSd("/gougou.bmp", 0, 0)) return;
#endif
  tft.pushImage(0, 0, GOUGOU_W, GOUGOU_H, gougou_bmp);
}

void drawZhPhrase(int idx) {
  int n = phraseCount > 0 ? phraseCount : ZH_PHRASE_COUNT;
  const ZhPhrase* p = &zh_phrases[0];
  String target = phraseTexts[idx % n];
  for (int i = 0; i < ZH_PHRASE_COUNT; i++) {
    if (target == String(zh_phrases[i].text)) { p = &zh_phrases[i]; break; }
  }
  int x = (320 - p->w) / 2;
  int y = 196;
  tft.drawBitmap(x, y, p->data, p->w, 24, BG_DEEP, BG_PINK);
}

int utf8Next(const String& s, int i, uint32_t& cp) {
  if (i >= (int)s.length()) return 0;
  uint8_t b = (uint8_t)s[i];
  if (b < 0x80) { cp = b; return 1; }
  if ((b & 0xE0) == 0xC0 && i + 1 < (int)s.length()) {
    cp = ((uint32_t)(b & 0x1F) << 6) | ((uint8_t)s[i + 1] & 0x3F);
    return 2;
  }
  if ((b & 0xF0) == 0xE0 && i + 2 < (int)s.length()) {
    cp = ((uint32_t)(b & 0x0F) << 12) | (((uint8_t)s[i + 1] & 0x3F) << 6) | ((uint8_t)s[i + 2] & 0x3F);
    return 3;
  }
  cp = (uint32_t)'?';
  return 1;
}

bool getGbGlyph(uint32_t cp, uint8_t out[32]) {
  int lo = 0, hi = GB2312_GLYPH_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint16_t c = pgm_read_word(&gb2312_unicode[mid]);
    if (c == cp) {
      memcpy_P(out, &gb2312_font[mid * 32], 32);
      return true;
    }
    if (c < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return false;
}

int utf8CharWidth(uint32_t cp) {
  if (cp < 0x80) return tft.textWidth(String((char)cp), 2);
  return 16;
}

void drawUtf8Wrapped(const String& text, int x, int y, int maxWidth, int lineHeight,
                     uint16_t fg, uint16_t bg, int maxLines) {
  int cx = x, cy = y, line = 0, i = 0;
  String asciiRun = "";
  int asciiW = 0;

  auto flushAscii = [&]() {
    if (asciiRun.length() == 0) return;
    tft.startWrite();
    tft.drawString(asciiRun, cx, cy, 2);
    tft.endWrite();
    cx += asciiW;
    asciiRun = "";
    asciiW = 0;
  };

  while (i < (int)text.length() && line < maxLines) {
    uint32_t cp;
    int n = utf8Next(text, i, cp);
    if (n <= 0) break;
    if (cp == '\n') {
      flushAscii();
      cx = x; cy += lineHeight; line++;
      i += n; continue;
    }
    if (cp < 0x80) {
      String ch((char)cp);
      int w = tft.textWidth(ch, 2);
      if (cx + asciiW + w > x + maxWidth && (cx > x || asciiW > 0)) {
        flushAscii();
        cx = x; cy += lineHeight; line++;
      }
      if (line >= maxLines) break;
      asciiRun += ch;
      asciiW += w;
    } else {
      flushAscii();
      if (cx + 16 > x + maxWidth && cx > x) { cx = x; cy += lineHeight; line++; }
      if (line >= maxLines) break;
      uint8_t glyph[32];
      tft.startWrite();
      if (getGbGlyph(cp, glyph)) tft.drawBitmap(cx, cy, glyph, 16, 16, fg, bg);
      else tft.drawChar(cx, cy, '?', fg, bg, 2);
      tft.endWrite();
      cx += 16;
    }
    i += n;
  }
  flushAscii();
}

void drawMessage(const String& text) {
  tft.fillScreen(BG_PINK);
  tft.setTextColor(TFT_WHITE, BG_DEEP);
  tft.setTextDatum(TC_DATUM);
  tft.fillRect(0, 0, 320, 36, BG_DEEP);
  tft.drawString("Message", 160, 8, 2);
  tft.setTextColor(TFT_DARKGREY, BG_PINK);
  tft.setTextDatum(TL_DATUM);
  drawUtf8Wrapped(text, 10, 70, 300, 30, TFT_DARKGREY, BG_PINK, 5);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(BG_DEEP, BG_PINK);
  tft.drawString("from your phone", 160, 225, 2);
  idleDirty = true;
}

// ---------- HTTP ----------
void handleRoot() {
  Serial.println("[http] GET /");
  server.send(200, "text/plain; charset=utf-8",
    "Jiujiu is alive!\nIP: " + myIP + "\nPOST /api/message {ver:1,cmd:\"msg\"|\"chat\",text:\"...\"}\nGET /api/status");
}

String llmEscape(const String& s) {
  String out = "";
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') { }
    else out += c;
  }
  return out;
}

String llmUnescape(const String& s) {
  String out = "";
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\' && i + 1 < (int)s.length()) {
      char n = s[i + 1];
      if (n == 'n') { out += '\n'; i++; }
      else if (n == '"') { out += '"'; i++; }
      else if (n == '\\') { out += '\\'; i++; }
      else out += c;
    } else {
      out += c;
    }
  }
  return out;
}

String extractLlmContent(const String& json) {
  int key = json.indexOf("\"content\"");
  if (key < 0) return "";
  int colon = json.indexOf(':', key);
  if (colon < 0) return "";
  int q = json.indexOf('"', colon + 1);
  if (q < 0) return "";
  int end = q + 1;
  while (end < (int)json.length()) {
    if (json[end] == '"' && json[end - 1] != '\\') break;
    end++;
  }
  if (end >= (int)json.length()) return "";
  return json.substring(q + 1, end);
}

String askLLM(const String& userText) {
  if (WiFi.status() != WL_CONNECTED) return "网络还没连上，先让啾啾连WiFi。";
  if (String(LLM_API_KEY) == "PASTE_YOUR_API_KEY_HERE") return "还没填大模型API Key。";

  String body = "{\"model\":\"" + String(LLM_MODEL) +
                "\",\"messages\":[{\"role\":\"system\",\"content\":\"" + llmEscape(LLM_SYSTEM_PROMPT) +
                "\"},{\"role\":\"user\",\"content\":\"" + llmEscape(userText) +
                "\"}],\"max_tokens\":120,\"temperature\":0.8}";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(20000);
  if (!http.begin(client, LLM_API_URL)) return "连接大模型失败。";
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + LLM_API_KEY);
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  Serial.printf("[llm] code=%d len=%d\n", code, resp.length());
  if (code <= 0) return "网络请求失败，检查WiFi和API地址。";
  if (code != 200) return "大模型返回错误：" + String(code);
  String reply = extractLlmContent(resp);
  if (reply.length() == 0) return "啾啾没想好怎么回，再试一次。";
  return llmUnescape(reply);
}

void handleMessage() {
  String body = server.arg("plain");
  Serial.println("[http] POST args=" + String(server.args()) + " bodyLen=" + String(body.length()));
  for (int a = 0; a < server.args(); a++) {
    Serial.println("[http] arg" + String(a) + " key=" + server.argName(a) + " val=" + server.arg(a).substring(0, 60));
  }
  String cmd = jsonStr(body, "cmd");
  if (cmd == "msg" || cmd == "message") {
    currentMsg = jsonStr(body, "text");
    if (currentMsg.length() == 0) currentMsg = "(empty)";
    msgShownUntil = millis() + 10000;   // 显示10秒
    drawMessage(currentMsg);
    server.send(200, "application/json", "{\"ok\":true}");
  } else if (cmd == "chat") {
    String question = jsonStr(body, "text");
    if (question.length() == 0) question = "陪我聊聊天吧";
    currentMsg = "正在想...";
    msgShownUntil = millis() + 30000;
    drawMessage(currentMsg);
    currentMsg = askLLM(question);
    msgShownUntil = millis() + 30000;
    drawMessage(currentMsg);
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(200, "application/json", "{\"ok\":false,\"err\":\"bad cmd\"}");
  }
}

void handleStatus() {
  bool wifi = WiFi.status() == WL_CONNECTED;
  String json = "{\"ver\":1,\"ip\":\"" + myIP + "\",\"wifi\":" + (wifi ? "true" : "false") +
                ",\"sd\":" + String(sdOk ? "true" : "false") + ",\"battery\":0}";
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
  for (int i = 0; i < phraseCount; i++) {
    if (i) json += ",";
    json += "\"" + phraseTexts[i] + "\"";
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
String bleChatText = "";
String bleWifiSsid = "";
String bleWifiPwd = "";
volatile bool bleMsgReady = false;
volatile bool bleChatReady = false;
volatile bool bleWifiReady = false;

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
      bleIncoming = jsonStr(body, "text");
      if (bleIncoming.length() == 0) bleIncoming = "(empty)";
      bleMsgReady = true;   // 主循环绘制, 避免与TFT SPI冲突
      bleReply("{\"ok\":true}");
    } else if (cmd == "chat") {
      bleChatText = jsonStr(body, "text");
      if (bleChatText.length() == 0) bleChatText = "陪我聊聊天吧";
      bleChatReady = true;
      bleReply("{\"ok\":true}");
    } else if (cmd == "wifi") {
      bleWifiSsid = jsonStr(body, "ssid");
      bleWifiPwd = jsonStr(body, "pwd");
      if (bleWifiSsid.length() == 0) {
        bleReply("{\"ok\":false,\"err\":\"WiFi名称不能为空\"}");
      } else {
        bleWifiReady = true;  // 主循环执行连接, 避免在 BLE 回调里长时间阻塞
      }
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
void startApMode() {
  Serial.println("[WiFi] switch to AP only");
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(300);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PWD, 1, 0);
  myIP = WiFi.softAPIP().toString();
  Serial.printf("[WiFi] AP mode IP=%s (SSID=%s)\n", myIP.c_str(), AP_SSID);
}

bool connectToWifi(const String& ssid, const String& pwd, unsigned long timeoutMs) {
  Serial.printf("[WiFi] STA connecting to %s ...\n", ssid.c_str());
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pwd.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    myIP = WiFi.localIP().toString();
    Serial.printf("[WiFi] STA connected, IP=%s\n", myIP.c_str());
    return true;
  }
  Serial.println("[WiFi] STA failed");
  return false;
}

void setupWiFi() {
  WiFi.persistent(false);  // 不启用 Arduino 自动重连, 账号由 Preferences 管理
  String ssid, pwd;
  loadWifiCredentials(ssid, pwd);
  if (!connectToWifi(ssid, pwd, 8000)) startApMode();
}

void applyBleWifi() {
  bleWifiReady = false;
  saveWifiCredentials(bleWifiSsid, bleWifiPwd);
  currentMsg = "连接 WiFi: " + bleWifiSsid;
  msgShownUntil = millis() + 30000;
  drawMessage(currentMsg);
  if (connectToWifi(bleWifiSsid, bleWifiPwd, 12000)) {
    bleReply(("{\"ok\":true,\"ip\":\"" + myIP + "\"}").c_str());
    currentMsg = "WiFi 已连接";
    msgShownUntil = millis() + 10000;
    drawMessage(currentMsg);
  } else {
    startApMode();
    bleReply("{\"ok\":false,\"err\":\"WiFi连接失败, 请检查热点名称/密码\"}");
    currentMsg = "WiFi 失败, 已回 AP";
    msgShownUntil = millis() + 10000;
    drawMessage(currentMsg);
  }
}

// ---------- 主流程 ----------
void setup() {
  Serial.begin(115200);
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, HIGH);   // DHT11 上电
  delay(500);
  dht.begin();
#if USE_SD_IMAGE || ENABLE_SD_LOG
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SDSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOk = SD.begin(SD_CS, SDSPI, 400000);
  Serial.printf("[SD] %s (HSPI 14/22/13/33)\n", sdOk ? "ok" : "fail");
#else
  Serial.println("[SD] disabled: using built-in gougou image");
#endif
  initPhrases();
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

  if (bleWifiReady) {
    applyBleWifi();
  }

  if (bleChatReady) {
    bleChatReady = false;
    currentMsg = "正在想...";
    msgShownUntil = millis() + 30000;
    drawMessage(currentMsg);
    currentMsg = askLLM(bleChatText);
    msgShownUntil = millis() + 30000;
    drawMessage(currentMsg);
  }

  unsigned long now = millis();

  // 消息显示中 -> 不画表情
  if (now < msgShownUntil) return;

  // 从消息页回到待机狗头
  if (idleDirty) {
    idleDirty = false;
    drawIdle();
  }

  // 鼓励语轮换(每12秒)
  if (now - lastPhraseAt > 12000) {
    lastPhraseAt = now;
    phraseIdx = (phraseIdx + 1) % phraseCount;
    drawIdle();
  }
}











