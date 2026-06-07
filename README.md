# GameWatch - 游戏窗口监控推送工具

当系统上指定游戏窗口数量低于设定阈值时，通过 PushDeer 推送消息到手机通知。

## 功能

- **窗口监控** — 定时扫描系统上所有可见窗口的标题，正则匹配关键词
- **阈值报警** — 匹配窗口数低于设定值时触发推送
- **PushDeer 推送** — 通过 PushDeer 服务推送消息到手机
- **冷却防刷** — 可配置推送冷却时间，避免重复推送
- **自定义消息** — 支持模板变量，灵活定制推送内容
- **配置保存** — 所有设置保存在 exe 同目录的 `GameWatch.ini`，拷贝即用

## 使用方法

### 1. 下载

从 [Releases](https://github.com/mdt-star/GameWatch/releases) 页面下载最新版本的 `GameWatch-Windows.zip`，解压运行 `GameWatch.exe`。

### 2. 配置

| 配置项 | 说明 |
|--------|------|
| **窗口标题关键词** | 匹配窗口标题的关键词，支持正则。例如 `原神` 或 `Genshin\|Honkai` |
| **低于此数量时报警** | 匹配窗口数低于该值时触发推送 |
| **PushDeer Key** | 你的 PushDeer 推送密钥（格式 `PDU...`） |
| **推送服务地址** | PushDeer 服务地址，默认 `https://api2.pushdeer.com/message/push` |
| **推送消息内容** | 推送内容模板，支持变量见下文 |
| **推送冷却时间** | 两次推送之间的最短间隔（秒），默认 60 秒 |
| **启动时自动监控** | 勾选后程序启动自动开始监控 |

### 3. 消息模板变量

| 变量 | 说明 | 示例 |
|------|------|------|
| `{count}` | 当前匹配窗口数 | `0` |
| `{threshold}` | 设置的阈值 | `1` |
| `{match}` | 匹配关键词 | `原神` |
| `{time}` | 当前时间 | `2026-06-07 22:00:00` |

#### 默认模板

```
⚠️ 游戏窗口不足！当前仅 {count} 个窗口匹配"{match}"，低于阈值 {threshold} 个。
```

#### 自定义示例

```
⚠️ 检测到游戏掉线！当前只有 {count} 个游戏窗口在线（{time}）
```

### 4. PushDeer 配置

1. 下载 [PushDeer App](https://pushdeer.com/)（iOS/Android）
2. 登录后获取你的 **Key**（以 `PDU...` 开头）
3. 填入 GameWatch 的 PushDeer Key 输入框
4. 点击 **测试推送** 验证配置是否正常

## 开发

### 环境要求

- CMake ≥ 3.16
- Qt 6.x 或 Qt 5.15+
- C++17 编译器

### macOS 编译

```bash
brew install qt cmake
cd GameWatch
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build .
./GameWatch
```

### Windows 交叉编译

项目使用 GitHub Actions 自动构建 Windows 版本，详见 [`.github/workflows/build.yml`](.github/workflows/build.yml)。

发布新版本：

```bash
git tag v1.1.0
git push origin v1.1.0
```

## 技术架构

```
┌─────────────────────────────────────────┐
│               Qt Widgets UI              │
│         MainWindow (配置/状态/控制)       │
├──────────────────────┬──────────────────┤
│   WindowMonitor      │ PushDeerClient   │
│   (窗口枚举+匹配)     │ (推送发送)        │
├──────────┬───────────┴────────┬─────────┤
│ macOS    │ Windows            │ 通用     │
│ CGWindow │ EnumWindows        │ WinHTTP  │
│ ListCopy │                    │ /curl    │
└──────────┴────────────────────┴─────────┘
```

- **UI 层** — Qt Widgets 实现配置界面和状态显示
- **监控层** — 跨平台窗口扫描（macOS CoreGraphics / Windows EnumWindows）
- **推送层** — 原生 HTTP 请求（Windows WinHTTP / macOS curl），无需额外依赖

## License

MIT