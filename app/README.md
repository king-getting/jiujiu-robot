# 啾啾手机 APP（安卓）

> KIMI Work 开发 · 对接标准见 `docs/手机APP开发交接.md` 第五节通信协议
> 当前版本：v0.1.0（框架 + 通信层 + 四功能 UI，内置 Mock 模式）

## 已实现功能

| # | 功能 | 状态 |
|---|------|------|
| 1 | 远程发消息到屏幕 | ✅（`tts` 字段保留但恒为 false——语音模块未安装，固件端忽略即可） |
| 2 | 查看传感器数据 | ✅（温度/湿度/空气质量，手动刷新） |
| 3 | WiFi 配网（BLE） | ✅（含权限申请、扫描、写入、notify 回执） |
| 4 | 鼓励语管理 | ✅（列表/添加/按序号删除） |
| - | Mock 模式 | ✅（设置页开关，默认开，无硬件可调试全部 UI） |
| - | 设备状态心跳 | ✅（设置页"测试连接"） |

## 如何构建 APK

1. 用 **Android Studio**（Hedgehog 及以上）打开本目录（`app/`）
2. 等待 Gradle 同步完成（首次需联网下载依赖）
3. 菜单 **Build → Build APK(s)**，产物在 `app/build/outputs/apk/debug/`
4. 或用命令行（需本地有 Gradle 8+）：`gradle wrapper && ./gradlew assembleDebug`

要求：minSdk 26（Android 8.0），JDK 17。

## 使用说明

1. 安装后默认处于**模拟数据模式**，四个页面可直接体验
2. 啾啾联网后：设置页填入它的局域网 IP → 关掉"模拟数据模式"→ 点"测试连接"确认在线
3. 啾啾未联网时：设置页底部填家里 WiFi 名称+密码 → 点"开始配网"→ 成功后 APP 自动填入 IP 并切换到真机模式

## 固件端需要遵守的约定（对接清单）

### HTTP 接口（JSON，含 `"ver":1`）

| 接口 | 方法 | 请求 | 响应 |
|------|------|------|------|
| `/api/message` | POST | `{ver, cmd:"msg", text, tts}` | `{ok:true}` |
| `/api/sensor` | GET | — | `{ver, temp, humi, air}` |
| `/api/phrase/list` | GET | — | `{ver, list:[...]}` |
| `/api/phrase/add` | POST | `{ver, cmd:"add", text}` | `{ok:true}` |
| `/api/phrase/del` | POST | `{ver, cmd:"del", index}` | `{ok:true}` |
| `/api/status` | GET | — | `{ver, ip, wifi, sd, battery}` |

错误统一：`{ok:false, err:"原因"}`。APP 侧超时 3 秒。

### BLE 配网（APP 已硬编码，固件需一致）

| 项 | 值 |
|---|---|
| Service UUID | `6a696f00-0000-1000-8000-00805f9b34fb` |
| 写入特征值 | `6a696f01-0000-1000-8000-00805f9b34fb` |
| Notify 特征值 | `6a696f02-0000-1000-8000-00805f9b34fb` |

- APP 扫描时按 Service UUID 过滤，固件广播包需包含该 Service UUID
- 写入内容：`{ver:1, cmd:"wifi", ssid:"...", pwd:"..."}`
- 固件配网后 notify 回：`{ok:true, ip:"192.168.x.x"}`；失败回 `{ok:false, err:"..."}`
- APP 侧超时 15 秒

## 代码结构

```
app/src/main/java/com/jiujiu/robot/
├── AppConfig.kt          ← 全局配置（IP/Mock开关）+ api() 入口
├── net/
│   ├── Protocol.kt       ← 协议常量 + 数据模型 + JiuJiuApi 接口
│   ├── HttpApi.kt        ← HTTP 实现（真机，OkHttp，3秒超时）
│   ├── MockApi.kt        ← Mock 实现（无硬件调试）
│   └── BleProvisioner.kt ← BLE 配网（扫描→写入→notify 回执）
└── ui/
    ├── MainActivity.kt    ← 底部导航容器
    ├── MessageFragment.kt ← 功能1 发消息
    ├── SensorFragment.kt  ← 功能2 传感器
    ├── PhraseFragment.kt  ← 功能4 鼓励语
    └── SettingsFragment.kt← 设置 + 功能3 配网
```

分层约定：UI 层只依赖 `JiuJiuApi` 接口，新增指令在 `Protocol.kt` 加 cmd 值并在两个实现里各加一个方法即可。

## 语音功能说明（当前版本已砍）

语音输出（SYN6288）与语音对话（INMP441+ASR+大模型）本版均不实现。
`/api/message` 的 `tts` 字段保留在协议中，APP 发送时恒为 `false`；
后期语音模块到货后，固件实现播报、APP 打开开关即可，协议不变。
