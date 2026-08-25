# 啾啾 — ESP32智能桌面聊天机器人

> 一只放在桌面的智能摆件啾啾。能聊天（云端大模型）、会说话（SYN6288语音合成）、有表情（2.8寸触屏）、知冷暖（温湿度/空气质量传感器）、有手机APP远程互动。

## 仓库结构

```
jiujiu-robot/
├── README.md                  ← 你在这里
├── docs/
│   ├── 项目交接.md             ← 硬件端总文档（引脚/环境/踩坑/路线图）
│   ├── 手机APP开发交接.md       ← APP开发需求与通信协议（KIMI Work用）
│   └── 任务树状图.md            ← 全项目WBS（Mermaid渲染）
├── esp32/
│   ├── config/
│   │   └── User_Setup.h        ← TFT_eSPI引脚配置模板
│   ├── sd_test/                ← SD卡读写测试（✅已跑通）
│   ├── touch_diag/             ← 触摸原始数据诊断（🔧调试中）
│   ├── touch_test/             ← 触摸校准+按钮交互
│   └── bmp_test/               ← SD卡BMP图片显示测试
└── app/                        ← 手机APP（待KIMI Work开发）
```

## 当前状态速览（2026-08-25）

| 模块 | 状态 |
|------|------|
| 屏幕显示 | ✅ 点亮，白屏可自动恢复 |
| SD卡读写 | ✅ 3.3V直连模块可用 |
| 触摸交互 | ✅ 已恢复，不再被旧SD模块干扰 |
| 传感器 | ✅ DHT11 + MQ-135 已接入 |
| 蓝牙BLE | ✅ 配网 / 消息 / AI聊天可用 |
| DeepSeek AI | ✅ 固定可用IP + 释放蓝牙内存后请求成功 |
| 手机APP | ✅ 蓝牙连接、消息、AI聊天可用，UI待继续美化 |

## 从哪开始接手

1. 先读 `docs/项目交接.md` —— 硬件端一切知识都在里面
2. 做APP读 `docs/手机APP开发交接.md` —— 通信协议双方必须遵守
3. 看 `docs/任务树状图.md` —— 全任务分解和优先级

## ⚠️ 需要手动补传的文件（在Windows电脑上，AI无法访问）

| 文件 | 电脑上的位置 | 补传到仓库位置 |
|------|------|------|
| screen_debug.ino | `C:\Users\89640\Documents\智能聊天机器人——啾啾\03_ESP32代码\屏幕驱动\screen_debug\` | `esp32/screen_debug/` |
| User_Setup.h（实际使用的完整版） | `C:\Users\89640\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h` | `esp32/config/User_Setup.h`（覆盖模板） |

补传方法：GitHub网页 → 仓库 → Add file → Upload files，拖进去即可。
